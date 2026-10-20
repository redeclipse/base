# who.py <perf.data> <function>: what a function spends its samples on, and its callers
import sys, re, collections, subprocess
data, target = sys.argv[1], sys.argv[2]
out = subprocess.run(['perf','script','-i',data,'--comm','redeclipse_linu','-F','pid,tid,ip,sym,dso'], capture_output=True, text=True).stdout
leafc = collections.Counter(); callers = collections.Counter(); tot=0
for smp in out.split('\n\n'):
    lines = smp.strip().split('\n')
    if len(lines) < 2: continue
    frames = []
    for l in lines[1:]:
        m = re.match(r'\s*([0-9a-f]+) (.*) \((.*)\)$', l)
        if m: frames.append((m.group(1), m.group(2), m.group(3)))
    idx = None
    for i,(a,n,d) in enumerate(frames):
        if 'redeclipse_linux' in d:
            idx = i; break
    if idx is None: continue
    if not frames[idx][1].startswith(target): continue
    tot += 1
    leafc['%s:%s' % (frames[0][1] if frames[0][1] != '[unknown]' else frames[0][0][-5:], 'libc' if idx else 'self')] += 1
    chain = [re.sub(r'\(.*','',f[1]) for f in frames[idx:idx+6]]
    callers[' <- '.join(chain)] += 1
print('samples', tot)
for k,v in leafc.most_common(12): print('  leaf', v, k)
for k,v in callers.most_common(10): print('  ', v, k)
