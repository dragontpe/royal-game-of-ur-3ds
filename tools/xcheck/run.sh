#!/bin/sh
# usage: run.sh [games] [seed]   (runs both boards)   (build first: make -C host build/xcheck_gen)
cd "$(dirname "$0")" || exit 1
GAMES=${1:-5000}; SEED=${2:-1}
[ -x .venv/bin/python ] || { python3 -m venv .venv && .venv/bin/pip install -q royalur; }
RC=0
for B in 0 1; do
  ../../host/build/xcheck_gen "$GAMES" "$SEED" "$B" > /tmp/ur_xcheck_$$.txt || exit 1
  .venv/bin/python check.py /tmp/ur_xcheck_$$.txt || RC=1
done
rm -f /tmp/ur_xcheck_$$.txt
exit $RC
