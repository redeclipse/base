# fold.py <perf.data> [funcs.txt]: share of main-thread samples spent in the interpreter itself and the libc it calls
import sys, os, re, collections, subprocess
data = sys.argv[1]
out = subprocess.run(['perf','script','--no-inline','-i',data,'--comm','redeclipse_linu','-F','pid,tid,ip,sym,dso'], capture_output=True, text=True).stdout
samples = out.split('\n\n')
cmdfuncs = set(open(sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'funcs.txt')).read().split())
tot = 0; inscript = 0; interp = 0
byfunc = collections.Counter(); libcby = collections.Counter(); leafself = collections.Counter()
for smp in samples:
    lines = smp.strip().split('\n')
    if len(lines) < 2: continue
    pid, tid = re.split(r'[/ ]+', lines[0].strip())[:2]
    if pid != tid: continue
    tot += 1
    frames = []
    for l in lines[1:]:
        m = re.match(r'\s*[0-9a-f]+ (.*) \((.*)\)$', l)
        if not m: continue
        frames.append((m.group(1), m.group(2)))
    names = [f[0] for f in frames]
    if any(n.startswith('runcode(') for n in names): inscript += 1
    # first real (non-inlined) frame in our binary
    leaf = None; libc = frames and 'redeclipse_linux' not in frames[0][1]
    for n, d in frames:
        if 'redeclipse_linux' in d and d != 'inlined':
            leaf = n; break
    if leaf is None: continue
    base = re.sub(r'\(.*', '', leaf).split(' ')[-1]
    base = re.sub(r'<.*', '', base)
    if base in cmdfuncs or base.startswith('_stdcmd') or 'hashbase<hashnameset<ident>' in leaf:
        interp += 1
        byfunc[base + (' [libc]' if libc else '')] += 1
print('main-thread samples %d; stack contains runcode %.1f%%; interpreter self (incl libc it calls) %.1f%%' % (tot, 100*inscript/tot, 100*interp/tot))
for k, v in byfunc.most_common(30): print('  %5.2f%% %s' % (100*v/tot, k))
