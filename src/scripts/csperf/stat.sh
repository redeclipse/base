#!/usr/bin/env bash
# stat.sh <outdir>: main-thread instructions and cycles per frame for each scene of a drive.sh mode 2 run
# frames come from the CSPERF counters when the build has them, otherwise from the average fps over the 10 second window
out=${1:?outdir}
for scene in menu player maps match; do
    [ -f "$out/$scene.stat" ] || continue
    frames=$(grep -o "^CSPERF $scene frames=[0-9]*" "$out/report.txt" | grep -o "[0-9]*$")
    if [ -n "$frames" ]; then frames=$(awk -v f="$frames" 'BEGIN{print f*10/12}')
    else frames=$(grep -o "^CSFPS $scene fps=[0-9.]*" "$out/report.txt" | grep -o "[0-9.]*$" | awk '{print $1*10}'); fi
    tid=$(grep ",instructions:u" "$out/$scene.stat" | sort -t, -k2 -n -r | head -1 | cut -d, -f1)
    ins=$(grep "^$tid,.*instructions:u" "$out/$scene.stat" | cut -d, -f2); cyc=$(grep "^$tid,.*cycles:u" "$out/$scene.stat" | cut -d, -f2)
    awk -v s="$scene" -v i="$ins" -v c="$cyc" -v n="$frames" 'BEGIN{printf "%-7s frames=%.0f Minstr/frame=%.3f Mcycles/frame=%.3f\n", s, n, i/n/1e6, c/n/1e6}'
done
