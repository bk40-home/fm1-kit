#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# fk_touch's tests on a computer: the glue and fm1-base's touch.c / sense.c through touch.c's own key model, with strict
# flags and the sanitizers; the float half alone with the chip's warnings (-Werror=double-promotion); and that touch.c,
# touch.h, sense.c and fastmath.h are still fm1-base's files at the commit SOURCE names (FM1_BASE=path to an fm1-base
# checkout).
set -e
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/fk_touch_test}
mkdir -p "$OUT"
CC=${CC:-cc}
# (-fno-sanitize=shift-base: fm1-base's fastmath.h fm_exp2_nf adds n << 23 for a negative n, the exponent's two's
# complement as every compiler here does it; the file stays as fm1-base has it. Upstream: (int32_t)((uint32_t)n << 23))
$CC -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -Wdouble-promotion -Wno-unused-function -fsanitize=address,undefined \
    -fno-sanitize=shift-base -fno-sanitize-recover=all -o "$OUT/fk_touch_test" tests/fk_touch_test.c
"$OUT/fk_touch_test"
$CC -std=c99 -Os -ffunction-sections -fno-builtin -Wall -Wextra -Werror -pedantic -ffp-contract=off -Wdouble-promotion \
    -c fk_touch_fpu.c -o "$OUT/fk_touch_fpu.o"
echo "fk_touch_fpu.c: ok (the chip's object, built strict)"
if [ -n "$FM1_BASE" ]; then
    want=$(sed -n 's/^fm1-base *[^ ]* *\([0-9a-f]*\).*/\1/p' SOURCE)
    for f in lib/touch.c lib/touch.h lib/sense.c include/fastmath.h; do
        git -C "$FM1_BASE" show "$want:$f" | cmp -s - "$(basename "$f")" || { echo "FAIL: $(basename "$f") is not fm1-base $want:$f"; exit 1; }
    done
    echo "fm1-base files: as at $want"
fi
