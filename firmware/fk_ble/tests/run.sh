#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# fk_ble's tests on a computer: the glue (tests/fk_ble_test.c) with strict flags and the sanitizers, and that stack/ and
# chip/ are still fm1-base's files at the commit SOURCE names (FM1_BASE=path to an fm1-base checkout: also runs the
# stack's own tests there, tests/bt/run.sh).
set -e
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/fk_ble_test}
mkdir -p "$OUT"
CC=${CC:-cc}
$CC -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -Wno-unused-function -fsanitize=address,undefined \
    -fno-sanitize-recover=all -o "$OUT/fk_ble_test" tests/fk_ble_test.c
"$OUT/fk_ble_test"
if [ -n "$FM1_BASE" ]; then
    sh tools/sync.sh --check "$FM1_BASE"
    (cd "$FM1_BASE" && sh tests/bt/run.sh)
fi
