#!/usr/bin/env python3
"""Time a song's lyrics to its recording, word by word, for the musiclyrics kept in the timing .cfg beside a music track.

Isolates the vocals with demucs, transcribes them with whisper to find roughly where each line of the lyrics is sung, then
force aligns each line's own words within that stretch to find when every word is sung. Needs whisperx and demucs, which
want a Python older than 3.14, see doc/menu-dancing.md.

usage: src/scripts/alignlyrics.py <track.ogg> <lyrics.txt> [vocals.wav]

The lyrics are plain text, a line of the song on each line, with [Section] headers and blank lines ignored. Prints the
musiclyrics block to paste into the timing file, and how well each line matched to stderr. Pass the vocals if they've
already been separated, to skip demucs.
"""
import contextlib
import logging
import os
import re
import subprocess
import sys
import tempfile
from difflib import SequenceMatcher

import whisperx

for handler in logging.getLogger('whisperx').handlers:
    handler.setStream(sys.stderr)  # stdout is kept for the lyrics

DEVICE = 'cuda'
SLACK = 0.6  # seconds either side of a line's matched words to search for the rest of them in
REPEAT = 0.15  # seconds a word can be from where the votes for it put it before it's moved there
SQUASHED = 0.15  # seconds closer than which a word is squashed up against the words either side, a little less than an eighth
SIMILAR = 0.6  # how alike two lines' words must be, as difflib's ratio, for each to vote on when the other's are sung
BACKINGBEATS = 2  # beats between the parts of backing vocals, as in (Whoa- oh- oh- oh!), which can't be told apart in the singing
ALTMODEL = 'WAV2VEC2_ASR_LARGE_LV60K_960H'  # a second, larger aligner, for a second opinion on every word
PAUSE = 0.25  # seconds of quiet in the vocals that count as a pause in the singing


def say(*args):
    print(*args, file=sys.stderr)


def norm(word):
    return re.sub(r"[^a-z0-9']", '', word.lower().replace('’', "'"))


def read_lyrics(path):
    """The lines of the lyrics, each word split into parts at its hyphens, the hyphen kept on the end of each part but the last,
    so each part is timed and lit up as it's sung, as in "razor- wire," or "(Whoa- oh- oh- oh!)"."""
    lines = []
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line and not re.fullmatch(r'\[.*\]', line):
                lines.append(re.sub(r'(?<=\w)-(?=\w)', '- ', line))
    return lines


def match_words(ours, heard):
    """Pairs of indexes of our words and the words heard that line up, aligned across the whole song at once so repeated lines
    like a chorus each find their own place, with near misses ("crypt" for "grip") counting for less than exact ones."""
    def score(a, b):
        if a == b:
            return 2
        return 1 if SequenceMatcher(None, a, b).ratio() >= 0.6 else -1
    n, m = len(ours), len(heard)
    best = [[0]*(m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        best[i][0] = -i
    for j in range(1, m + 1):
        best[0][j] = -j
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            best[i][j] = max(best[i - 1][j - 1] + score(ours[i - 1], heard[j - 1]), best[i - 1][j] - 1, best[i][j - 1] - 1)
    pairs = []
    i, j = n, m
    while i and j:
        s = score(ours[i - 1], heard[j - 1])
        if best[i][j] == best[i - 1][j - 1] + s:
            if s > 0:
                pairs.append((i - 1, j - 1))
            i, j = i - 1, j - 1
        elif best[i][j] == best[i - 1][j] - 1:
            i -= 1
        else:
            j -= 1
    return pairs[::-1]


def separate(track, tmp):
    subprocess.run([sys.executable, '-m', 'demucs', '--two-stems=vocals', '-n', 'htdemucs', '-o', tmp, track], check=True,
                   stdout=subprocess.DEVNULL)
    return os.path.join(tmp, 'htdemucs', os.path.splitext(os.path.basename(track))[0], 'vocals.wav')


def transcribe(audio, align_model, align_meta):
    model = whisperx.load_model('large-v3', DEVICE, compute_type='float16', language='en')
    result = model.transcribe(audio, batch_size=8, language='en')
    result = whisperx.align(result['segments'], align_model, align_meta, audio, DEVICE)
    return [w for s in result['segments'] for w in s['words'] if 'start' in w and norm(w['word'])]


def align_line(text, start, end, audio, align_model, align_meta):
    """When each of the line's words starts and ends, somewhere between start and end seconds."""
    result = whisperx.align([{'text': text, 'start': start, 'end': end}], align_model, align_meta, audio, DEVICE)
    got = [w for s in result['segments'] for w in s['words']]
    words = text.split()
    times = [None]*len(words)
    ends = [None]*len(words)
    # the aligner hands back the words it was given in order, without any it couldn't place a time on
    i = 0
    for w in got:
        while i < len(words) and norm(words[i]) != norm(w['word']):
            i += 1
        if i >= len(words):
            break
        if 'start' in w:
            times[i], ends[i] = w['start'], w['end']
        i += 1
    # words without a time are spread evenly between their neighbours
    known = [i for i, t in enumerate(times) if t is not None]
    if not known:
        return None, None
    for i in range(len(words)):
        if times[i] is None:
            lo = max([k for k in known if k < i], default=None)
            hi = min([k for k in known if k > i], default=None)
            if lo is None:
                times[i] = times[hi] - 0.2*(hi - i)
            elif hi is None:
                times[i] = ends[lo] + 0.2*(i - lo - 1)
            else:
                times[i] = ends[lo] + (times[hi] - ends[lo])*(i - lo)/(hi - lo)
            ends[i] = times[i] + 0.15
    return times, ends


def median(values):
    values = sorted(values)
    n = len(values)
    return values[n//2] if n % 2 else (values[n//2 - 1] + values[n//2])/2


def cross_check(timed, lines, quiet):
    """Puts each word where most of the votes for it agree it's sung. Where they're split evenly, it's put where it isn't squashed up
    against the words either side, and failing that with the votes nearest where it is now. Both aligners vote, and so does every other line with mostly
    the same words, like each time a chorus comes round or "A whisper to a shout" and "A whisper to a roar", which keep the same
    rhythm, shifted by how far apart they are. A word with no other line to vote on it, where the two aligners disagree with a
    pause in the singing between them, is taken to come after the pause, as either can run a word into the end of the last."""
    keys = [[norm(w) for w in t['words']] for t in timed]
    moves = []
    for a, t in enumerate(timed):
        # each the time and the line voting for it, twice over for backing vocals placed where the singing comes back
        votes = [[(s, a), (r, a)]*(2 if i in t['fixed'] else 1) for i, (s, r) in enumerate(zip(t['times'], t['alt']))]
        for b, u in enumerate(timed):
            match = SequenceMatcher(None, keys[a], keys[b], autojunk=False)
            if b == a or match.ratio() < SIMILAR:
                continue
            pairs = [(i + k, j + k) for i, j, n in match.get_matching_blocks() for k in range(n)]
            if len(pairs) < 3:
                continue
            shift = median([t['times'][i] - u['times'][j] for i, j in pairs])
            for i, j in pairs:
                votes[i] += [(u['times'][j] + shift, b), (u['alt'][j] + shift, b)]*(2 if j in u['fixed'] else 1)
        for i, w in enumerate(t['words']):
            own = t['times'][i]
            if len(votes[i]) > 2:
                agree = lambda v: [x for x in votes[i] if abs(x[0] - v[0]) <= REPEAT]
                at = lambda v: median([x[0] for x in agree(v)])
                room = lambda v: (i == 0 or v - t['times'][i - 1] >= SQUASHED) and (i + 1 == len(t['times']) or t['times'][i + 1] - v >= SQUASHED)
                best = max(votes[i], key=lambda v: (len(agree(v)), room(at(v)), -abs(v[0] - own)))
                target, why = at(best), 'in step with the other times it\'s sung'
            else:
                lo, hi = sorted(x[0] for x in votes[i])
                if hi - lo <= REPEAT or not any(lo < x and y <= hi + 0.05 for x, y in quiet):
                    continue
                target, why = hi, 'after the pause, as the other aligner has it'
            if abs(own - target) > REPEAT:
                moves.append((t, i, target, why))
    # all moved at once, as long as they keep each line's words in order
    after = {id(t): list(t['times']) for t in timed}
    for t, i, target, why in moves:
        after[id(t)][i] = target
    for t, i, target, why in moves:
        new = after[id(t)]
        if (i and target <= new[i - 1]) or (i + 1 < len(new) and target >= new[i + 1]):
            say('warning: line %d left "%s" at %.2f, as %.2f %s is out of order: %s' %
                (t['line'] + 1, t['words'][i], t['times'][i], target, why, lines[t['line']]))
            continue
        say('line %d moved "%s" from %.2f to %.2f, %s: %s' % (t['line'] + 1, t['words'][i], t['times'][i], target, why, lines[t['line']]))
        t['ends'][i] += target - t['times'][i]
        t['times'][i] = target


def pauses(audio):
    """Where the singing pauses, as (start, end) seconds, from how loud the vocals are every 50 milliseconds."""
    hop = 800  # samples at 16kHz
    loud = [sum(x*x for x in audio[i:i + hop])/hop for i in range(0, len(audio) - hop, hop)]
    quiet = 0.01*sorted(loud)[int(len(loud)*0.9)]  # well below the singing
    found, start = [], None
    for i, l in enumerate(loud + [quiet + 1]):
        if l < quiet and start is None:
            start = i
        elif l >= quiet and start is not None:
            if (i - start)*hop/16000 >= PAUSE:
                found.append((start*hop/16000, i*hop/16000))
            start = None
    return found


def read_tempo(track):
    """The track's tempo from the timing file beside it, or 174."""
    try:
        with open(os.path.splitext(track)[0] + '.cfg', encoding='utf-8') as f:
            found = re.search(r'^\s*musictempo\s+([0-9.]+)', f.read(), re.M)
            return float(found[1]) if found else 174.0
    except OSError:
        return 174.0


def comes_back(audio, after, until):
    """Where the singing comes back after it first drops away, for at least a PAUSE, below a quarter of how loud it was just
    after the given time, before the other; or None. Backing vocals come in like this after a held word."""
    hop = 800  # samples at 16kHz, 50 milliseconds
    loud = lambda t: (sum(x*x for x in audio[int(t*16000):int(t*16000) + hop])/hop)**0.5
    ref = median([loud(after + k*0.05) for k in range(8)])
    t, run = after + 0.2, 0
    while t < until:
        if loud(t) < ref*0.25:
            run += 1
        elif run*0.05 >= PAUSE*0.8 and loud(t) >= ref*0.3:
            return t
        elif loud(t) >= ref*0.25:
            run = 0
        t += 0.05
    return None


def quote(word):
    return '"%s"' % word.replace('^', '^^').replace('"', '^"')


def main():
    track, lyricsfile = sys.argv[1], sys.argv[2]
    lines = read_lyrics(lyricsfile)
    with tempfile.TemporaryDirectory() as tmp, contextlib.redirect_stdout(sys.stderr):
        vocals = sys.argv[3] if len(sys.argv) > 3 else separate(track, tmp)
        audio = whisperx.load_audio(vocals)
        align_model, align_meta = whisperx.load_align_model(language_code='en', device=DEVICE)
        alt_model, alt_meta = whisperx.load_align_model(language_code='en', device=DEVICE, model_name=ALTMODEL)
        heard = transcribe(audio, align_model, align_meta)
        quiet = pauses(audio)
        tempo = read_tempo(track)

        # match the lyrics' words to the words heard, in order, to find roughly where each line is sung
        ours = [(li, wi, norm(w)) for li, line in enumerate(lines) for wi, w in enumerate(line.split())]
        anchors = [[] for _ in lines]
        for a, b in match_words([w for _, _, w in ours], [norm(w['word']) for w in heard]):
            anchors[ours[a][0]].append((ours[a][1], heard[b]))

        # each line is searched for within its matched words, give or take a little, never past the lines either side
        found = [(a[0][1]['start'], a[-1][1]['end']) if a else None for a in anchors]
        timed = []
        for li, line in enumerate(lines):
            prev = max([f[1] for f in found[:li] if f], default=0.0)
            nxt = min([f[0] for f in found[li + 1:] if f], default=len(audio)/16000)
            # backing vocals in brackets at the end of a line are placed apart from the rest of it, below
            words = line.split()
            b = next((i for i, w in enumerate(words) if w.startswith('(') and i), len(words))
            sung = ' '.join(words[:b])
            heardsung = [(wi, w) for wi, w in anchors[li] if wi < b]
            if heardsung:
                first, last = heardsung[0][1]['start'], heardsung[-1][1]['end']
                start, end = max(first - SLACK, prev), min(last + SLACK, nxt)
                # words at the start of a line that weren't heard are sung from where the singing comes back after a pause, and
                # ones at the end until it next pauses
                if heardsung[0][0] > 0:
                    start = max(max([q for p, q in quiet if q <= first], default=prev) - 0.1, prev)
                if heardsung[-1][0] < b - 1:
                    end = min(min([p for p, q in quiet if p >= last], default=nxt) + 0.1, nxt)
                # and a line with backing vocals after it is searched for up to the next, as they make it hard to hear where it ends
                if b < len(words):
                    end = nxt
            else:
                say('warning: line %d not heard, searching between its neighbours: %s' % (li + 1, line))
                start, end = prev, nxt
            times, ends = align_line(sung, start, end, audio, align_model, align_meta)
            if times is None:
                say('warning: line %d could not be aligned, left out: %s' % (li + 1, line))
                continue
            say('%3d %7.2f-%7.2f  heard %2d/%2d words  %s' % (li + 1, times[0], ends[-1], len(heardsung), len(times), line))
            # where whisper heard a word somewhere else than the aligner put it, with a pause in the singing between the two, the
            # word comes after the pause: the aligner tends to run a word after a pause into the end of the one before it, and
            # whisper to catch a blip at the end of a word before a pause as the next, so whichever is after it is taken
            for wi, w in heardsung:
                lo, hi = sorted((w['start'], times[wi]))
                if norm(w['word']) == norm(words[wi]) and hi == w['start'] and any(lo < p and q <= hi + 0.05 for p, q in quiet) and \
                        (wi == 0 or times[wi - 1] < w['start']) and (wi + 1 == len(times) or w['start'] < times[wi + 1]):
                    say('line %d took "%s" at %.2f where it was heard, not %.2f: %s' % (li + 1, words[wi], w['start'], times[wi], line))
                    times[wi], ends[wi] = w['start'], w['end']
            alt, _ = align_line(sung, start, end, audio, alt_model, alt_meta)
            alt = alt or list(times)
            # backing vocals come in where the singing comes back after dropping away from the word before them, or straight after
            # it if it doesn't, each part of them BACKINGBEATS on from the last, and count for more when voting where they come in
            fixed = set()
            if b < len(words):
                back = comes_back(audio, times[-1], nxt - 0.5)  # not the next line coming in
                if back is None:
                    back = ends[-1]
                else:
                    fixed = set(range(b, len(words)))
                    say('line %d put "%s" at %.2f, where the singing comes back: %s' % (li + 1, words[b], back, line))
                parts = [back + k*BACKINGBEATS*60/tempo for k in range(len(words) - b)]
                times += parts
                ends += [p + BACKINGBEATS*60/tempo for p in parts]
                alt += parts
            timed.append({'line': li, 'words': words, 'times': times, 'ends': ends, 'alt': alt, 'fixed': fixed})

    cross_check(timed, lines, quiet)
    # a line's last word can be held on into the next, which takes over when it starts
    for k in range(1, len(timed)):
        timed[k - 1]['ends'][-1] = min(timed[k - 1]['ends'][-1], timed[k]['times'][0])
    print('// the lyrics as they\'re sung: each line the millis it starts and ends, then each word and the millis it\'s sung at')
    print('musiclyrics [')
    for t in timed:
        print('    [%d %d %s]' % (round(t['times'][0]*1000), round(t['ends'][-1]*1000),
                                  ' '.join('%s %d' % (quote(w), round(s*1000)) for w, s in zip(t['words'], t['times']))))
    print(']')


if __name__ == '__main__':
    main()
