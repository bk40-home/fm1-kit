# fk_touch: velocity and pressure from the FM-1's own keys

A drop-in module that gives a Dinghy-based FM-1 firmware key velocity and pressure, with no change to the hardware.
It is used by M-Vsynth2, and M-vance will follow. The sensing is Charles Vestal's, from
[fm1-base](https://github.com/charlesvestal/fm1-base) (`lib/touch.c`, `lib/sense.c`, tuned on CTL-1 and MONK-1). It is
copied unchanged; this module only adds the glue. The firmware is not moved onto fm1-base.

## What it does

- **Velocity on all 27 keys.** F3 to C5 (keys 0 to 19, matrix rows PA7 and PA8) are read through the ADC. The velocity
  comes from how deep the key's carbon pill reads just after it lands, on a log scale with fm1-base's curve (0.7).
  C#5 to G5 (keys 20 to 26, row PA6) have no converter. Their velocity comes from a probe through the LED line.
- **Pressure on F3 to C5 only**, polyphonic, 0 to 127: how much harder a held key is pressed than where it settled.
  It is 0 for the first 250 ms while the key settles, then follows the lean, smoothed over about 30 ms. The top seven
  keys have no pressure: that is the hardware.
- **Off until the app turns it on.** While it is off, the scan, the keys and the sound are exactly as before:
  velocity 100, no pressure.
- **Settings** the app keeps: on / off, a velocity curve and a pressure sensitivity, each -5 to +5 (0 is fm1-base's
  tuning). A + curve makes 127 harder to reach; a + sensitivity gives full pressure for less lean.
- fm1-base's per-key calibration (`sense_learn`) stays off, as fm1-base ships it.

## Cost

- **The scan:** with sensing on, each tick of the 10 kHz key scan starts one ADC conversion and takes the previous one.
  A held key's row is driven high on alternate readings. The MASTER pot is read by the scan then (`fm1_an.master`),
  because the converter is the scan's.
- **The main loop:** `fk_touch_poll` every pass; the pressure is worked out every 5 ms over 20 keys.
- **Floats:** touch.c and sense.c use single-precision floats. On the chip they are built as their own object with the
  FPU's flags (`fk_touch_fpu.c`, as M-Vsynth2's `vs_dsp.c`), so the platform and the app stay integer-only. A key's
  velocity is worked out in the scan's interrupt, with the FPU. fm1-base does the same on this chip, with an
  interrupt entry identical to Dinghy's (felucca-bsp `chip/fm1_isr.S`), and builds everything, its audio interrupt
  included, with the FPU's flags.
- **RAM:** about 2.8 KB of buffers, all zero at start, plus 36 B of settings in `.data`. In `.bss` by default (M-Vsynth2).
  They are all `fk_touch_fpu.o`'s, so a firmware whose 96 KB RAM is full places them from its linker script:
  M-vance's `app.ld` excludes that object from `.bss` and puts its `.bss` in `.pool` (zeroed at boot like `.bss`).
- **Key LEDs:** while a sensed key is held, its column's LEDs are dark for the ticks that read it. A lit key may look a
  little dimmer while TOUCH is on.

## Using it in a firmware

The firmware's `hal/` needs the analog key rows: `fm1_input_an.h` (felucca-bsp's code, MPL-2.0), included by
`fm1_input.h`, and felucca-bsp's `fm1_adc.h`. In `platform.c` (see M-Vsynth2's):

```c
#define FK_TOUCH 1                                   /* 0: the stubs (the SMK-37, the web simulator) */
#include ".../fk_touch/fk_touch.h"                   /* before fm1_input.h: the scan's hooks */
#include "fm1_input.h"
...                                                   /* fm1_ms, fm1_adc.h */
#include ".../fk_touch/fk_touch.c"                   /* the glue; MASTER from fm1_an.master while fm1_an.con */
```

`tools/build.py` builds `fk_touch_fpu.c` with the FPU's flags and links it (FM-1 only). On a computer (`DINGHY_HOST`)
`fk_touch.c` includes it instead, with touch.c's own key model: `fk_touch_host_key(k, vel, press %)` plays a key and
`fk_touch_host_tick()` runs once a millisecond.

The app:

```c
fk_touch_set(curve, sens); fk_touch_enable(on);      /* when its settings load or change */
fk_touch_poll();                                     /* every main-loop pass, before the keys */
keys = fk_touch_keys();                              /* bit k: key k held (fm1_in.notes while off) */
vel = fk_touch_vel(k);                               /* a key just down: 1..127 (100 while off) */
p = fk_touch_channel_pressure();                     /* the hardest-pressed held key, 0..127 (0 while off) */
fk_touch_pressure(k); fk_touch_last_vel();           /* for a screen */
```

Turning it on or off ends the held sensed keys (the keys change hands), as fm1-base's `touch_on` says.

## Files

| Path | What | Licence |
| --- | --- | --- |
| `fk_touch.h`, `fk_touch.c`, `fk_touch_fpu.c`, `tests/` | the glue | MPL-2.0 |
| `touch.c`, `touch.h`, `sense.c`, `fastmath.h` | fm1-base, unchanged (Charles Vestal) | MPL-2.0 |
| `LICENSES/` | the MPL-2.0 text | |

`SOURCE` names the fm1-base and felucca-bsp commits. MPL-2.0 is compatible with the GPL-3.0 (its section 3.3): a
GPL-3.0 firmware stays GPL-3.0 as a whole, and these files keep their MPL-2.0 headers.

## Tests

```sh
sh tests/run.sh                         # velocity on every key, curve, pressure, sensitivity, the top row, off
FM1_BASE=../fm1-base sh tests/run.sh    # also checks the four fm1-base files against SOURCE's commit
```

The feel of the keys can only be judged on an FM-1: see the firmware's test plan.
