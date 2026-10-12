#!/bin/bash
# (AI-assisted) Run the graphics comparison, close any running game, and build viewing sheets in the work folder.
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
J="$(cd "$S/../.." && pwd)"
if [ -z "${JAK3_SKATE_WORK:-}" ]; then
  echo "JAK3_SKATE_WORK is not set" >&2
  exit 1
fi
W="$JAK3_SKATE_WORK"
D="$APPDATA/OpenGOAL/jak3/screenshots"
VIEW="$W/run/gfx-view"
mkdir -p "$VIEW" "$W/run"
set -o pipefail
NO_SKATE=1 bash "$S/check-and-run.sh" goal_src/jak3/pc/debug/gfx-enhance-compare.gc 200 2>&1 | grep -a "SK3TEST\|COMPILE\|Error\|gk exited"
grep -a -i "failed to compile\|failed to link\|error compiling" -A6 "$W/run/gk-auto.log" | sed 's/\x1b\[[0-9;]*m//g' | head -20
cd "$D" || exit 1
for l in 0-desert-start 1-ctyinda-start 2-foresta-start 3-sewer-start 4-ctyport-start; do
  python -I "$(cygpath -w "$S/montage.py")" "$(cygpath -w "$VIEW/$l-ab.png")" 0.5 full sk3-gfx-$l-a.png sk3-gfx-$l-b.png > /dev/null
  python -I "$(cygpath -w "$S/montage.py")" "$(cygpath -w "$VIEW/$l-c.png")" 0.5 full sk3-gfx-$l-c.png sk3-gfx-$l-d.png > /dev/null
  python -I "$(cygpath -w "$S/montage.py")" "$(cygpath -w "$VIEW/$l-crop.png")" 1 0.3,0.4,0.6,0.9 sk3-gfx-$l-a.png sk3-gfx-$l-b.png > /dev/null
done
echo; ls "$VIEW" | tr '\n' ' '
