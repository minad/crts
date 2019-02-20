#!/bin/bash
nsamples=1000
sleeptime=0.1

for x in $(seq 1 $nsamples); do
    (echo "set arch mips";
     echo "set endian little";
     echo "target remote localhost:1234";
     echo "set pagination 0";
     echo "thread apply all bt";
     echo "quit"; cat /dev/zero ) | gdb-multiarch run
    sleep $sleeptime
  done | \
awk '
  BEGIN { s = ""; }
  /^Thread/ { print s; s = ""; }
  /^\#/ { if (s != "" ) { s = s "," $4} else { s = $4 } }
  END { print s }' | \
sort | uniq -c | sort -r -n -k 1,1
