#!/bin/sh
# SP-2 measurement on a Raspberry Pi Zero 2 W.
#   scp sp2_daemon.c sp2_client.c run_on_zero.sh pi@zero:~/sp2/ && ssh pi@zero 'cd sp2 && sh run_on_zero.sh'
set -e
cc -O2 -o sp2_daemon sp2_daemon.c
cc -O2 -o sp2_client sp2_client.c
echo "host: $(uname -srm)  cores: $(getconf _NPROCESSORS_ONLN)"
for spec in "framed 20" "ndjson 20" "framed 200"; do
  mode=$(echo "$spec" | cut -d' ' -f1); rate=$(echo "$spec" | cut -d' ' -f2)
  echo "--- $mode @ ${rate}Hz ---"
  ./sp2_daemon --$mode --rate "$rate" --seconds 10 --path /tmp/sp2z.sock 2>&1 &
  sleep 1
  ./sp2_client --$mode --path /tmp/sp2z.sock
  wait
done
echo "Done. Report the cpu percentages and latency figures."
