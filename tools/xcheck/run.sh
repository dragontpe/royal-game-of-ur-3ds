#!/bin/sh
# usage: run.sh [games] [seed]   (build first: make -C host build/xcheck_gen)
cd "$(dirname "$0")" || exit 1
GAMES=${1:-5000}; SEED=${2:-1}
[ -x .venv/bin/python ] || { python3 -m venv .venv && .venv/bin/pip install -q royalur; }
../../host/build/xcheck_gen "$GAMES" "$SEED" > /tmp/ur_xcheck_$$.txt || exit 1
.venv/bin/python check.py /tmp/ur_xcheck_$$.txt
RC=$?
rm -f /tmp/ur_xcheck_$$.txt
exit $RC
