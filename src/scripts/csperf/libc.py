# libc.py <perf.data> [rows]: main-thread time in libc, by function family and by the caller in the game binary
import sys, re, collections, subprocess
data = sys.argv[1]
D = subprocess.run(['debuginfod-find', 'debuginfo', '/usr/lib/libc.so.6'], capture_output=True, text=True).stdout.strip()
out = subprocess.run(['perf','script','--no-inline','-i',data,'--comm','redeclipse_linu','-F','pid,tid,ip,sym,dsoff'], capture_output=True, text=True).stdout
recs=[]; offs=set(); tot=0
for smp in out.split('\n\n'):
    lines = smp.strip().split('\n')
    if len(lines) < 2: continue
    pid, tid = re.split(r'[/ ]+', lines[0].strip())[:2]
    if pid != tid: continue
    tot += 1
    fr=[]
    for l in lines[1:]:
        m = re.match(r'\s*([0-9a-f]+) (.*?) \((.*?)(?:\+0x([0-9a-f]+))?\)$', l)
        if m: fr.append(m.groups())
    if not fr or 'libc.so' not in fr[0][2]: continue
    caller = next((re.sub(r'\(.*','',n) for a,n,d,o in fr if 'redeclipse_linux' in d), '?')
    recs.append((fr[0][3], caller)); offs.add(fr[0][3])
offs=[o for o in offs if o]
names = dict(zip(offs, subprocess.run(['addr2line','-f','-e',D]+['0x'+o for o in offs],capture_output=True,text=True).stdout.split('\n')[0::2]))
c=collections.Counter((names.get(o,'?'),cl) for o,cl in recs)
fam=collections.Counter()
for (n,cl),v in c.items():
    k='alloc' if re.search(r'malloc|free|tcache|chunk|new|delete',n) else 'strlen' if 'strlen' in n else 'strcmp' if 'strcmp' in n else 'printf' if 'printf' in n else 'strtod' if 'strto' in n or 'mpn' in n or 'round_and' in n else 'mem' if 'mem' in n else 'other:'+n
    fam[k]+=v
print('main samples',tot)
for k,v in fam.most_common(12): print('  family %5.2f%% %s'%(100*v/tot,k))
for (n,cl),v in c.most_common(int(sys.argv[2]) if len(sys.argv)>2 else 25): print('  %5.2f%% %-28s <- %s'%(100*v/tot,n,cl))
