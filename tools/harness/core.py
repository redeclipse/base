"""Shared transport and process plumbing for the Linux/macOS harness scripts.

The Python counterpart of core.ps1: the paths, launching and stopping the game,
the CubeScript command channel (boot.cfg polls home/uitest/harness/cmd_<n>.cfg)
and screenshots. harness.py, editor.py, gi.py, shaders.py and the self-tests
import it. Python 3.8 or later, standard library only.
"""

import os
import platform
import re
import signal
import subprocess
import sys
import time
import zlib

# ---------------------------------------------------------------- paths ----

HARNESS_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(HARNESS_DIR))
HOME_DIR = os.path.join(REPO_ROOT, 'home', 'uitest')
CMD_DIR = os.path.join(HOME_DIR, 'harness')
SHOT_DIR = os.path.join(CMD_DIR, 'shots')
LOG_FILE = os.path.join(HOME_DIR, 'log.txt')
PID_FILE = os.path.join(CMD_DIR, 'pid.txt')
BOOT_LOG = os.path.join(CMD_DIR, 'boot-log.txt')
# The game's stdout and stderr. log.txt has the console; this has what the
# engine only prints to the terminal (fatal() and signal messages on Unix).
GAME_OUTPUT = os.path.join(CMD_DIR, 'game-output.txt')

# Errors the game reports for bad script; surfaced after each batch.
ERROR_PATTERN = re.compile(r'Unknown command:|Unknown alias lookup:|Unknown variable|Could not read|Cannot find')

# The "YYYY-MM-DD HH:MM.SS " stamp the log adds to every line.
STAMP = re.compile(r'^\d{4}-\d{2}-\d{2} \d{2}:\d{2}\.\d{2} ')


class HarnessError(Exception):
    """A harness operation failed; the message says why."""


# ------------------------------------------------------------- output ----

_COLOURS = {'red': 31, 'green': 32, 'yellow': 33, 'cyan': 36, 'grey': 90}


def colour(text, name, stream=None):
    """text in an ANSI colour when the stream is a terminal (and NO_COLOR is unset)."""
    stream = stream or sys.stdout
    if name and not os.environ.get('NO_COLOR') and hasattr(stream, 'isatty') and stream.isatty():
        return '\033[%dm%s\033[0m' % (_COLOURS[name], text)
    return text


def say(text='', name=None, stream=None):
    """Narration, like Write-Host: to stderr, so stdout carries only results."""
    stream = stream or sys.stderr
    print(colour(text, name, stream), file=stream)


def warn(text):
    say('WARNING: ' + text, 'yellow')


# ------------------------------------------------------------ helpers ----

def strip_stamp(line):
    return STAMP.sub('', line)


def write_text(path, text):
    """UTF-8 without a BOM and with \\n line ends: CubeScript parses a BOM as
    part of the first token."""
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def read_log():
    """The whole of log.txt, or '' if there is none yet. 'clearlog' truncates
    and reopens it, so a read can land mid-swap; the batch poll retries."""
    try:
        with open(LOG_FILE, 'r', encoding='utf-8', errors='replace', newline='') as f:
            return f.read()
    except OSError:
        return ''


def to_float(value):
    """The game always prints '.' decimals, and Python's float() never reads
    the locale, so this is invariant by construction."""
    return float(value)


def format_coord(value):
    """value with at most five decimals and no trailing zeros ('0.#####')."""
    text = '%.5f' % float(value)
    text = text.rstrip('0').rstrip('.')
    return '0' if text in ('-0', '') else text


# ------------------------------------------------------- the game binary ----

def _platform_suffix():
    system = platform.system()
    if system == 'Linux':
        return '_linux'
    if system == 'Darwin':
        return '_macos'
    if system == 'FreeBSD':
        return '_bsd'
    if system == 'Windows' or system.startswith(('MINGW', 'MSYS', 'CYGWIN')):
        return '.exe'
    raise HarnessError('Unsupported system: %s' % system)


def _arch():
    # The same mapping as redeclipse.sh.
    machine = platform.machine().lower()
    if re.match(r'^i[3-6]86$|^x86$', machine):
        return 'x86'
    if machine in ('x86_64', 'amd64'):
        return 'amd64'
    if machine in ('arm64', 'aarch64'):
        return 'arm64'
    if machine.startswith('arm'):
        return 'arm'
    return 'native'


def bin_dir():
    return os.path.join(REPO_ROOT, 'bin', _arch())


def game_exe():
    """The client to launch: $RE_HARNESS_EXE if set, else the one redeclipse.sh
    would run, bin/<arch>/redeclipse_<linux|macos|bsd>."""
    override = os.environ.get('RE_HARNESS_EXE')
    if override:
        return os.path.abspath(os.path.expanduser(override))
    suffix = _platform_suffix()
    name = 'redeclipse' + suffix
    return os.path.join(bin_dir(), name)


# ---------------------------------------------------------- the process ----

_child = None  # the Popen this process started, so its exit is seen (no zombie)


def _pid_alive(pid):
    if _child is not None and _child.pid == pid:
        return _child.poll() is None
    if os.name == 'nt':
        out = subprocess.run(['tasklist', '/FI', 'PID eq %d' % pid, '/NH'], capture_output=True, text=True).stdout
        return str(pid) in out
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return False  # someone else's process: the pid was reused
    # A recycled pid would pass the check above: make sure it is the game.
    try:
        out = subprocess.run(['ps', '-o', 'stat=,comm=', '-p', str(pid)], capture_output=True, text=True).stdout.strip()
    except OSError:
        return True
    if not out:
        return False
    stat, _, comm = out.partition(' ')
    return not stat.startswith('Z') and 'redeclipse' in os.path.basename(comm.strip())


def harness_pid():
    """The pid of the running harness game, or None."""
    try:
        with open(PID_FILE) as f:
            pid = int(f.read().strip() or 0)
    except (OSError, ValueError):
        return None
    return pid if pid > 0 and _pid_alive(pid) else None


def is_running():
    return harness_pid() is not None


def assert_running():
    pid = harness_pid()
    if not pid:
        raise HarnessError('Harness is not running. Start it with: tools/harness/harness.py start')
    return pid


def start(width=1280, height=720, focus=False, exe=None, timeout=90):
    """Launches the game against the isolated home dir with boot.cfg's command
    channel. Returns the pid; does nothing but return it if already running."""
    global _child
    existing = harness_pid()
    if existing:
        say('Harness already running (PID %d). Use \'stop\' first to restart.' % existing, 'yellow')
        return existing

    exe = exe or game_exe()
    if not os.path.isfile(exe):
        raise HarnessError('Game binary not found: %s\nBuild it with DEBUG_UTILS defined (see tools/harness/README.md), '
                           'or point RE_HARNESS_EXE at it.' % exe)

    os.makedirs(SHOT_DIR, exist_ok=True)
    # Both sides count from zero, so clear any batches from a previous run.
    for name in os.listdir(CMD_DIR):
        if re.match(r'^cmd_\d+\.cfg$', name):
            os.remove(os.path.join(CMD_DIR, name))
    for path in (LOG_FILE,):
        try:
            os.remove(path)
        except FileNotFoundError:
            pass

    # One list element per argument, so a space in the repo path cannot split
    # the home dir the way an unquoted Start-Process argument would.
    args = [exe, '-h' + HOME_DIR, '-df0', '-dw%d' % width, '-dh%d' % height, '-xexec tools/harness/boot.cfg']

    env = dict(os.environ)
    # Opt into the engine's crash logging (src/engine/main.cpp). Only the
    # child sees it, so it cannot leak into the calling shell.
    env['RE_CRASHLOG'] = '1'
    # The shared libraries beside the binaries, as redeclipse.sh does.
    libvar = {'Darwin': 'DYLD_LIBRARY_PATH'}.get(platform.system(), 'LD_LIBRARY_PATH')
    env[libvar] = bin_dir() + (os.pathsep + env[libvar] if env.get(libvar) else '')
    if not focus and platform.system() == 'Darwin':
        # SDL_HINT_MAC_BACKGROUND_APP: the window opens without taking focus
        # from whatever the user is doing. boot.cfg sets renderunfocused 1.
        env['SDL_MAC_BACKGROUND_APP'] = '1'

    with open(GAME_OUTPUT, 'wb') as out:
        kwargs = {}
        if os.name == 'nt':
            kwargs['creationflags'] = getattr(subprocess, 'CREATE_NEW_PROCESS_GROUP', 0)
        else:
            kwargs['start_new_session'] = True  # a Ctrl+C in the caller's terminal doesn't reach the game
        _child = subprocess.Popen(args, cwd=REPO_ROOT, env=env, stdin=subprocess.DEVNULL, stdout=out,
                                  stderr=subprocess.STDOUT, **kwargs)
    write_text(PID_FILE, '%d\n' % _child.pid)
    say('Starting Red Eclipse (PID %d, %dx%d)...' % (_child.pid, width, height), 'cyan')

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if _child.poll() is not None:
            raise HarnessError('Game exited during startup (code %s).\n%s%s' % (_child.returncode, read_log(), _game_output_tail()))
        if 'HARNESS_READY' in read_log():
            break
        time.sleep(0.5)
    else:
        raise HarnessError('Harness did not come up within %ds. Log:\n%s%s' % (timeout, read_log(), _game_output_tail()))

    # Keep the boot diagnostics: the first batch's clearlog wipes the log.
    write_text(BOOT_LOG, read_log())
    say('Ready. Home: %s' % HOME_DIR, 'green')
    say('Do not minimize the window (screenshots come back black).')
    return _child.pid


def _game_output_tail(lines=20):
    try:
        with open(GAME_OUTPUT, 'r', encoding='utf-8', errors='replace') as f:
            tail = f.read().splitlines()[-lines:]
    except OSError:
        return ''
    return '\nGame output:\n' + '\n'.join(tail) if tail else ''


def stop():
    """Stops the harness game. Returns the pid it stopped, or None."""
    global _child
    pid = harness_pid()
    if not pid:
        say('Harness is not running.')
        _remove(PID_FILE)
        return None
    if os.name == 'nt':
        subprocess.run(['taskkill', '/F', '/PID', str(pid)], capture_output=True)
    else:
        # SIGTERM first (the engine's shutdownsignal quits), then SIGKILL.
        _signal(pid, signal.SIGTERM)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline and _pid_alive(pid):
            time.sleep(0.1)
        if _pid_alive(pid):
            _signal(pid, signal.SIGKILL)
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline and _pid_alive(pid):
                time.sleep(0.1)
    if _child is not None and _child.pid == pid:
        _child.wait()
        _child = None
    _remove(PID_FILE)
    say('Stopped PID %d.' % pid, 'green')
    return pid


def _signal(pid, sig):
    try:
        os.kill(pid, sig)
    except ProcessLookupError:
        pass


def _remove(path):
    try:
        os.remove(path)
    except FileNotFoundError:
        pass


# ------------------------------------------------------------ batches ----

def next_seq():
    best = -1
    try:
        names = os.listdir(CMD_DIR)
    except FileNotFoundError:
        return 0
    for name in names:
        m = re.match(r'^cmd_(\d+)\.cfg$', name)
        if m:
            best = max(best, int(m.group(1)))
    return best + 1


def invoke_batch(script, settle_ms=1, timeout=30):
    """Runs CubeScript in the game and returns its log lines (stamps kept).

    The batch is 'clearlog', the script, then a sleep that echoes a sentinel,
    so once the sentinel is in the log the log holds exactly this batch, and
    at least one frame has been rendered (screenshots need that)."""
    assert_running()
    n = next_seq()
    sentinel = 'HARNESS_END %d' % n
    body = '\n'.join(['clearlog', script, 'sleep %d [ echo "%s" ]' % (max(settle_ms, 1), sentinel)])
    write_text(os.path.join(CMD_DIR, 'cmd_%d.cfg' % n), body + '\n')

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        text = read_log()
        if sentinel in text:
            return [line for line in re.split(r'\r?\n', text) if line and sentinel not in line]
        # A crashed game never writes the sentinel, so notice the exit now
        # rather than after the whole timeout.
        if not harness_pid():
            raise HarnessError('Game exited while running batch %d. Last log:\n%s%s' % (n, read_log(), _game_output_tail()))
        time.sleep(0.1)
    if not harness_pid():
        raise HarnessError('Game exited while running batch %d. Last log:\n%s%s' % (n, read_log(), _game_output_tail()))
    raise HarnessError('Timed out after %ds waiting for batch %d. Last log:\n%s' % (timeout, n, read_log()))


def format_batch_result(lines):
    """The lines as Show-BatchResult prints them: stamps stripped, repeats
    collapsed to one line with an (xN) count (UITREE lines kept as they are),
    and the script errors among them."""
    out, counts, order = [], {}, []
    for line in lines:
        line = strip_stamp(line)
        if line.startswith('UITREE '):
            out.append(line)
            continue
        if line in counts:
            counts[line] += 1
        else:
            counts[line] = 1
            order.append(line)
    for line in order:
        out.append('%s   (x%d)' % (line, counts[line]) if counts[line] > 1 else line)
    errors = [line for line in order if ERROR_PATTERN.search(line)]
    return out, errors


def show_batch_result(lines, stream=None):
    out, errors = format_batch_result(lines)
    for line in out:
        print(line, file=stream or sys.stdout)
    if errors:
        warn('Script errors reported by the game:')
        for line in errors:
            warn('  ' + line)
    return out


def invoke_shot(name, settle_ms=300, timeout=30, quiet=False):
    """Takes a screenshot into harness/shots/<name>.png and returns its path."""
    name = name or 'shot'
    if not re.match(r'^[A-Za-z0-9_.-]+$', name):
        raise HarnessError("Screenshot name must be [A-Za-z0-9_.-]+ (got '%s')." % name)
    target = os.path.join(SHOT_DIR, name + '.png')
    _remove(target)
    # The back buffer must hold a rendered frame, so settle before shooting.
    lines = invoke_batch('sleep %d [ screenshot "harness/shots/%s" ]' % (settle_ms, name), settle_ms + 300, timeout)
    if not quiet:
        show_batch_result(lines)
    for _ in range(20):
        if os.path.exists(target):
            break
        time.sleep(0.1)
    if not os.path.exists(target):
        raise HarnessError('Screenshot was not written: %s' % target)
    return target


# --------------------------------------------------------- screenshots ----

def read_png(path):
    """(width, height, channels, rows) of an 8-bit, non-interlaced PNG, with
    rows a list of bytes, channels per pixel. Enough for the game's own
    screenshots, which is all the self-tests read."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise HarnessError('Not a PNG: %s' % path)
    pos, idat, header = 8, [], None
    while pos < len(data):
        length = int.from_bytes(data[pos:pos + 4], 'big')
        kind = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            header = chunk
        elif kind == b'IDAT':
            idat.append(chunk)
        elif kind == b'IEND':
            break
    width, height = int.from_bytes(header[0:4], 'big'), int.from_bytes(header[4:8], 'big')
    depth, ctype, interlace = header[8], header[9], header[12]
    channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(ctype)
    if depth != 8 or channels is None or interlace:
        raise HarnessError('Unsupported PNG (depth %d, colour type %d, interlace %d): %s' % (depth, ctype, interlace, path))
    raw = zlib.decompress(b''.join(idat))
    stride = width * channels
    rows, prev, i = [], bytearray(stride), 0
    for _ in range(height):
        ftype = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        if ftype == 1:
            for x in range(channels, stride):
                line[x] = (line[x] + line[x - channels]) & 0xFF
        elif ftype == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xFF
        elif ftype == 3:
            for x in range(stride):
                left = line[x - channels] if x >= channels else 0
                line[x] = (line[x] + ((left + prev[x]) >> 1)) & 0xFF
        elif ftype == 4:
            for x in range(stride):
                a = line[x - channels] if x >= channels else 0
                b = prev[x]
                c = prev[x - channels] if x >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        elif ftype != 0:
            raise HarnessError('Bad PNG filter %d: %s' % (ftype, path))
        rows.append(bytes(line))
        prev = line
    return width, height, channels, rows
