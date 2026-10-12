#!/bin/bash
# (AI-assisted) Launch Jak 3 with the bridge, load the observer through the REPL, and send real key presses; closes any running game.
# The game's keyboard setting is restored afterwards.
#   run-skate-keys.sh <timeline name in keys.py> [max seconds]
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
J="$(cd "$S/../.." && pwd)"
if [ -z "${JAK3_SKATE_WORK:-}" ]; then
  echo "JAK3_SKATE_WORK is not set" >&2
  exit 1
fi
W="$JAK3_SKATE_WORK"
NAME="${1:-play}"
MAX="${2:-120}"
LOG="$W/run/gk-auto.log"
SETTINGS="$APPDATA/OpenGOAL/jak3/settings/input-settings.json"

mkdir -p "$W/run"
cd "$J" || exit 1
taskkill //F //IM gk.exe > /dev/null 2>&1
taskkill //F //IM goalc.exe > /dev/null 2>&1
cp "$SETTINGS" "$W/run/input-settings.backup.json"

export OPENGOAL_SKATE3_DLL="${OPENGOAL_SKATE3_DLL:-$(cygpath -w "$W/target-ffi/x86_64-pc-windows-msvc/release/skate_ffi.dll")}"
export OPENGOAL_SKATE3_ASSETS="${OPENGOAL_SKATE3_ASSETS:-$(cygpath -w "$W/skate-data-mashup/assets")}"
# Scripted checks must not see a real controller (see run-skate-test.sh). KEEP_PAD=1 keeps it.
[ -n "${KEEP_PAD:-}" ] || export SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT='0x0001/0x0001'
out/build/Release/bin/gk.exe -v --game jak3 -- -boot -fakeiso -debug > "$LOG" 2>&1 &
sleep 25

{
  echo '(make-group "iso")'
  echo '(lt)'
  echo '(ml "goal_src/jak3/pc/debug/skate3-bridge-observe.gc")'
  for i in $(seq 1 90); do
    if grep -a -q "SK3TEST begin" "$LOG"; then break; fi
    sleep 1
  done
  sleep 3
  python -I "$(cygpath -w "$S/keys.py")" "$NAME" >&2
  for i in $(seq 1 "$MAX"); do
    if grep -a -q "SK3TEST done" "$LOG"; then break; fi
    # tasklist itself fails now and then: only believe the game is gone after three misses in a row
    if tasklist 2> /dev/null | grep -q -i "gk.exe"; then MISSES=0; else MISSES=$((MISSES + 1)); fi
    if [ "${MISSES:-0}" -ge 3 ]; then echo "gk exited early" >&2; break; fi
    sleep 1
  done
  echo '(e)'
} | out/build/Release/bin/goalc.exe --game jak3 --disable-ansi > "$W/run/goalc-auto.log" 2>&1
REPL_STATUS=$?

taskkill //F //IM gk.exe > /dev/null 2>&1
taskkill //F //IM goalc.exe > /dev/null 2>&1
cp "$W/run/input-settings.backup.json" "$SETTINGS"

grep -a -n -m3 -A12 "Compilation Error" "$W/run/goalc-auto.log" | cut -c1-220
grep -a -E "SK3TEST|\[skate3\]" "$LOG" | sed 's/\x1b\[[0-9;]*m//g' | cut -c1-300
if [ "$REPL_STATUS" -ne 0 ] || ! grep -a -q "SK3TEST done" "$LOG"; then exit 1; fi
