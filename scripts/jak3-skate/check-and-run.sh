#!/bin/bash
# (AI-assisted) Compile-check a GOAL test file and run it in the game only if it compiles; closes any running game.
#   check-and-run.sh <goal file> [max seconds]
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
J="$(cd "$S/../.." && pwd)"
if [ -z "${JAK3_SKATE_WORK:-}" ]; then
  echo "JAK3_SKATE_WORK is not set" >&2
  exit 1
fi
cd "$J" || exit 1
mkdir -p "$JAK3_SKATE_WORK/run"
ERR=$(out/build/Release/bin/goalc.exe --game jak3 --disable-ansi --cmd "(begin (make-group \"iso\") (m \"$1\"))" 2>&1 | grep -a -m1 -A12 "Compilation Error" | cut -c1-220)
if [ -n "$ERR" ]; then
  echo "COMPILE FAILED"
  echo "$ERR"
  exit 1
fi
bash "$S/run-skate-test.sh" "$1" "${2:-120}"
