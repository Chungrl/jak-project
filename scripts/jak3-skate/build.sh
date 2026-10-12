#!/bin/bash
# (AI-assisted) Rebuild gk (C++) and the Jak 3 game code; closes a running game. Prints failures and final lines.
#   build.sh [cpp|goal|all]
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
J="$(cd "$S/../.." && pwd)"
if [ -z "${JAK3_SKATE_WORK:-}" ]; then
  echo "JAK3_SKATE_WORK is not set" >&2
  exit 1
fi
W="$JAK3_SKATE_WORK"
WHAT="${1:-all}"
if [ "$WHAT" != cpp ] && [ "$WHAT" != goal ] && [ "$WHAT" != all ]; then
  echo "build failed: expected cpp, goal, or all" >&2
  exit 1
fi
mkdir -p "$W/run"
cd "$J" || exit 1
taskkill //F //IM gk.exe > /dev/null 2>&1
taskkill //F //IM goalc.exe > /dev/null 2>&1

FAILED=0
if [ "$WHAT" = cpp ] || [ "$WHAT" = all ]; then
  VS="${JAK3_SKATE_VS:-C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools}"
  CM="$VS"'\Common7\IDE\CommonExtensions\Microsoft\CMake'
  cat > "$W/run/build-gk.bat" <<EOF
@echo off
set "PATH=$CM\\CMake\\bin;$CM\\Ninja;%LOCALAPPDATA%\\bin\\NASM;C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer;%PATH%"
call "$VS\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul
cmake --build out/build/Release --target gk goalc
EOF
  cmd //c "$(cygpath -w "$W/run/build-gk.bat")" > "$W/run/build-gk.log" 2>&1
  CPP_STATUS=$?
  echo "cpp build exit $CPP_STATUS"
  CPP_ERRORS="$(grep -a -E "error C|FAILED|fatal error|skate3_bridge\.cpp\([0-9]+\): error" "$W/run/build-gk.log" | cut -c1-300 | head -12)"
  if [ -n "$CPP_ERRORS" ]; then echo "$CPP_ERRORS"; fi
  if [ "$CPP_STATUS" -ne 0 ] || [ -n "$CPP_ERRORS" ]; then FAILED=1; fi
fi

if [ "$WHAT" = goal ] || [ "$WHAT" = all ]; then
  out/build/Release/bin/goalc.exe --game jak3 --disable-ansi --cmd '(mi)' > "$W/run/build-goal.log" 2>&1
  GOAL_STATUS=$?
  echo "goal build exit $GOAL_STATUS"
  GOAL_ERRORS="$(grep -a -n -m2 -A14 "Compilation Error" "$W/run/build-goal.log" | cut -c1-260 | head -40)"
  if [ -n "$GOAL_ERRORS" ]; then echo "$GOAL_ERRORS"; fi
  grep -a "Successfully built" "$W/run/build-goal.log" | tail -1
  if [ "$GOAL_STATUS" -ne 0 ] || [ -n "$GOAL_ERRORS" ]; then FAILED=1; fi
fi
exit "$FAILED"
