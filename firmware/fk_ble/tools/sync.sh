#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copy fm1-base's own BLE stack (bt/ble) and felucca-bsp's radio drivers (bsp/chip) into stack/ and chip/, unchanged.
#   sh tools/sync.sh FM1_BASE           copy from an fm1-base checkout (its bsp submodule checked out)
#   sh tools/sync.sh --check FM1_BASE   only say whether stack/ and chip/ match it (the tests run this)
# The checkout should be at the commits SOURCE names; a newer one: update SOURCE in the same commit.
set -e
cd "$(dirname "$0")/.."
check=0
[ "$1" = "--check" ] && { check=1; shift; }
B="${1:?usage: tools/sync.sh [--check] FM1_BASE}"
[ -f "$B/bt/ble/ble_stack.c" ] && [ -f "$B/bsp/chip/fm1_ble.h" ] || { echo "sync: $B is not an fm1-base checkout with bsp/"; exit 1; }
want=$(awk '$1 == "fm1-base" {print $3}' SOURCE)
have=$(git -C "$B" rev-parse HEAD 2>/dev/null || echo unknown)
[ "$want" = "$have" ] || echo "note: $B is at $have, SOURCE names $want"
bad=0
pair() {   # pair FROM TO
    if [ $check = 1 ]; then
        cmp -s "$1" "$2" || { echo "differs: $2"; bad=1; }
    else
        cp "$1" "$2"
    fi
}
for f in "$B"/bt/ble/*.c "$B"/bt/ble/*.h; do pair "$f" "stack/$(basename "$f")"; done
for f in fm1_ble.h fm1_ble_rf.h fm1_rf_cal.h ble_rf_tables_v15.h; do pair "$B/bsp/chip/$f" "chip/$f"; done
pair "$B/bsp/NOTICE" chip/NOTICE.felucca-bsp
if [ $check = 1 ]; then
    [ $bad = 0 ] && echo "sync: stack/ and chip/ match $B" || exit 1
else
    echo "sync: copied from $B; now: sh tests/run.sh"
fi
