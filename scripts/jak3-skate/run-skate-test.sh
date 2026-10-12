#!/bin/bash
# (AI-assisted) Launch Jak 3 with the Skate 3 bridge, load a GOAL test through the REPL,
# keep the REPL connected until the test prints "SK3TEST done", then close the game and print its log.
# This closes any running game before starting.
#   run-skate-test.sh <goal file relative to the project> [max seconds for the test]
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
J="$(cd "$S/../.." && pwd)"
if [ -z "${JAK3_SKATE_WORK:-}" ]; then
  echo "JAK3_SKATE_WORK is not set" >&2
  exit 1
fi
W="$JAK3_SKATE_WORK"
TEST="${1:-goal_src/jak3/pc/debug/skate3-bridge-test.gc}"
MAX="${2:-120}"
LOG="$W/run/gk-auto.log"
SHOTS="$APPDATA/OpenGOAL/jak3/screenshots"

mkdir -p "$W/run"
cd "$J" || exit 1
taskkill //F //IM gk.exe > /dev/null 2>&1
taskkill //F //IM goalc.exe > /dev/null 2>&1
mkdir -p "$SHOTS"
rm -f "$SHOTS"/sk3-*.png

if [ -z "${NO_SKATE:-}" ]; then
  export OPENGOAL_SKATE3_DLL="${OPENGOAL_SKATE3_DLL:-$(cygpath -w "$W/target-ffi/x86_64-pc-windows-msvc/release/skate_ffi.dll")}"
  export OPENGOAL_SKATE3_ASSETS="${OPENGOAL_SKATE3_ASSETS:-$(cygpath -w "$W/skate-data-mashup/assets")}"
fi
# Scripted checks must not see a real controller: anyone picking one up while a check runs
# drives Jak and spoils it. SDL skips every device except the (non-existent) one named here.
# Set KEEP_PAD=1 to play by hand.
[ -n "${KEEP_PAD:-}" ] || export SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT='0x0001/0x0001'
out/build/Release/bin/gk.exe -v --game jak3 -- -boot -fakeiso -debug > "$LOG" 2>&1 &
sleep 25

{
  echo '(make-group "iso")'
  echo '(lt)'
  echo "(ml \"$TEST\")"
  for i in $(seq 1 "$MAX"); do
    if grep -a -q "SK3TEST done\|SK3TEST FAILED" "$LOG"; then break; fi
    # tasklist itself fails now and then: only believe the game is gone after three misses in a row
    if tasklist 2> /dev/null | grep -q -i "gk.exe"; then MISSES=0; else MISSES=$((MISSES + 1)); fi
    if [ "${MISSES:-0}" -ge 3 ]; then echo "gk exited early" >&2; break; fi
    sleep 1
  done
  sleep 1
  echo '(e)'
} | out/build/Release/bin/goalc.exe --game jak3 --disable-ansi > "$W/run/goalc-auto.log" 2>&1
REPL_STATUS=$?

taskkill //F //IM gk.exe > /dev/null 2>&1
taskkill //F //IM goalc.exe > /dev/null 2>&1

grep -a -n -m3 -A12 "Compilation Error" "$W/run/goalc-auto.log" | cut -c1-220
grep -a -E "SK3TEST|SK3JOINT|\[skate3\]" "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | cut -c1-260
echo "--- screenshots"
ls "$SHOTS" 2> /dev/null | grep "^sk3-"
if [ "$REPL_STATUS" -ne 0 ] || ! grep -a -q "SK3TEST done" "$LOG"; then exit 1; fi
