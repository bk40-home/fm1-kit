# fk_ble: Bluetooth MIDI for Dinghy-based FM-1 firmwares

A drop-in module that gives an FM-1 firmware Bluetooth MIDI. It is used by M-Vsynth2 and M-vance. The FM-1 shows up as
a BLE-MIDI device called `NAME XXXX`, and a Mac, iPhone or iPad connects to it like any Bluetooth MIDI keyboard.

It does not move the firmware onto [fm1-base](https://github.com/charlesvestal/fm1-base). It takes only fm1-base's
**own** Bluetooth stack (`ble_stack = "own"`): Henri David's stack from [Optimist](https://github.com/w0ts/optimist),
with felucca-bsp's radio drivers. None of JieLi's closed libraries are linked, so a GPL-3.0 firmware can ship it.

WiFi is left out on purpose. On the FM-1 it needs JieLi's binary-only libraries, which a GPL firmware can't ship. It
can't run alongside Bluetooth, and it needs the RAM that USB audio uses.

## What it does

- The FM-1 is a BLE-MIDI **peripheral**: one computer or phone at a time connects to it. It does not connect out to
  Bluetooth keyboards.
- **In:** notes, CCs, bend, pressure, program change, clock and transport. They reach the app through `midi_get`,
  which returns 3 for Bluetooth. **SysEx from Bluetooth is dropped:** the patch library, the transfer and the screen
  reader stay on USB.
- **Out:** everything the app sends with `midi_send` goes to USB and Bluetooth. The MIDI clock out (`midi_rt_at`, the
  audio interrupt's) goes to USB only.
- **No pairing:** macOS and iOS connect without it. The address comes from the flash's unique ID, so it is the same
  on every boot and the computer keeps recognising the FM-1.
- **On / off:** the player's switch, kept with the settings. The first ON calibrates the radio (about 0.5 s; the
  main loop waits). The sound is silent for the first second after it, as on fm1-base (`fk_ble_quiet`).
- **Interrupts:** the stack runs in the radio's two interrupts at priority 2, below the audio (3) and TIMER5 (4). It
  never delays the audio; Bluetooth waits for an audio half instead.

## Cost

Measured from a 32-bit build of the module on a computer, so treat these as estimates:
- **RAM:** about 5.5 KB of `.bss`, plus 48 B of `.noinit`. The link layer's diagnostics are left out (`BLE_DIAG 0`).
- **Flash:** about 7 KB of tables. fm1-base gives 41 to 46 KB of code for the same stack.
- **RAM code:** one small function, `fm1_rf_burst_run`, plus the 16-byte ID read.

`./build.sh` prints the real figures.

## Using it in a firmware

`platform.c` does it all (see M-Vsynth2's or M-vance's):

```c
#define FK_BLE_NAME APP_NAME          /* or the app's own: at most 19 characters */
#include "fk_ble_uid.h"               /* after fm1_flash.h */
#include "fk_ble.c"
...
fk_ble_attach(uid);                   /* fm1_cstart: interrupts off, before fm1_guard_lock_top */
fk_ble_poll();                        /* platform_poll */
fk_ble_get(&pk) / fk_ble_send(pk)     /* midi_get / midi_send */
if (!fk_ble_quiet()) app_audio(...)   /* the audio interrupt */
```

The app calls `fk_ble_enable(on)` when its setting loads or changes, and `fk_ble_state()` to show OFF, ON or
LINKED. `tools/build.py` adds `fk_ble/`, `chip/` and `stack/` to the include path. With `--no-ble` it builds without
Bluetooth (`FK_BLE=0`) as `NAME-usb.fwsc`.

## Files

| Path | What | Licence |
| --- | --- | --- |
| `fk_ble.h`, `fk_ble.c`, `fk_ble_uid.h`, `tests/`, `tools/` | the glue, after fm1-base's `device/bt_own.c` | MPL-2.0 |
| `stack/` | fm1-base `bt/ble`, unchanged: Henri David's stack | GPL-3.0-only OR MPL-2.0 (each file's header) |
| `chip/fm1_ble.h`, `fm1_ble_rf.h` | felucca-bsp's radio drivers, unchanged (Optimist, Henri David) | MPL-2.0 |
| `chip/fm1_rf_cal.h` | the radio's calibration: JieLi's SDK, rewritten in C by Charles Vestal | Apache-2.0 |
| `chip/ble_rf_tables_v15.h` | the radio's start-up tables from stock V15 (JieLi, M-VAVE) | Apache-2.0 |
| `chip/ble_rf_tables.h` | one line: the name `fm1_ble_rf.h` includes | MPL-2.0 |
| `LICENSES/` | MPL-2.0 and Apache-2.0 texts | |

`SOURCE` names the fm1-base and felucca-bsp commits. To update: check out fm1-base at a newer commit (with its `bsp`
submodule), run `sh tools/sync.sh PATH`, update `SOURCE`, then run the tests.

## Tests

```sh
sh tests/run.sh                         # the glue: the rings, what is let in, the state, the quiet second, the address
FM1_BASE=../fm1-base sh tests/run.sh    # also checks stack/ and chip/ against fm1-base, and runs the stack's tests
```

The radio itself can only be tested on an FM-1: see the firmware's test plan.
