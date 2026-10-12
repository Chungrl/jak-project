#!/bin/bash
# (AI-assisted) Build the runtime and GOAL code, then close any running game and run the bridge check.
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
J="$(cd "$S/../.." && pwd)"
if [ -z "${JAK3_SKATE_WORK:-}" ]; then
  echo "JAK3_SKATE_WORK is not set" >&2
  exit 1
fi
cd "$J" || { echo "verify failed: repository"; exit 1; }

BUILD_OUTPUT="$(bash "$S/build.sh" all 2>&1)"
BUILD_STATUS=$?
if [ "$BUILD_STATUS" -ne 0 ] || printf '%s\n' "$BUILD_OUTPUT" | grep -a -q -E 'Compilation Error|error C[0-9]+|fatal error|FAILED'; then
  echo "verify failed: build"
  exit 1
fi

CHECK_OUTPUT="$(bash "$S/check-and-run.sh" goal_src/jak3/pc/debug/skate3-bridge-test.gc 150 2>&1)"
CHECK_STATUS=$?
printf '%s\n' "$CHECK_OUTPUT" | grep -a 'SK3TEST' || true
if [ "$CHECK_STATUS" -ne 0 ] || ! printf '%s\n' "$CHECK_OUTPUT" | grep -a -q 'SK3TEST done' || ! printf '%s\n' "$CHECK_OUTPUT" | grep -a -q 'bridge active #t'; then
  echo "verify failed: bridge check"
  exit 1
fi
exit 0
