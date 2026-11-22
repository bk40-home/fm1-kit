/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kris (bk40-home): the glue. touch.c, touch.h, sense.c, fastmath.h: Charles Vestal (fm1-base) */
/* fk_touch: velocity and pressure from the FM-1's own keys, for Dinghy-based firmwares (M-Vsynth2, M-vance).
 *
 * fm1-base's lib/touch.c and lib/sense.c, unchanged (SOURCE says which commit), on the analog key rows that the scan
 * reads when hal/fm1_input_an.h is in the build. Off until the app turns it on: with it off the scan, the keys and the
 * sound are as before (velocity 100, no pressure).
 *
 *   velocity   all 27 keys. F3..C5 (keys 0..19, rows PA7 / PA8) from the ADC; C#5..G5 (keys 20..26, row PA6) from a
 *              probe through the LED line
 *   pressure   F3..C5 only, polyphonic, 0..127: how much harder a held key is pressed than where it settled
 *
 * In platform.c (FK_TOUCH 1; the SMK-37 and the web simulator build with FK_TOUCH 0, the stubs below):
 *   #include "fk_touch.h"        before fm1_input.h: the scan's hooks (FM1_INPUT_ANALOG, _DRIVE, _PROBE, _LED_OK)
 *   #include "fm1_input.h"
 *   ...                           fm1_ms defined
 *   #include "fk_touch.c"
 * In the app's main loop:
 *   fk_touch_poll();              every pass, before reading the keys (events in, pressure every 5 ms)
 *   keys = fk_touch_keys();       bit k: key k held (fm1_in.notes while off)
 *   vel = fk_touch_vel(k);        a key that has just gone down: its velocity, 1..127 (100 while off)
 *   p = fk_touch_channel_pressure();   the hardest-pressed held key, 0..127 (0 while off)
 * Settings (the app keeps them): fk_touch_enable(on), fk_touch_set(curve, sens), each -5..+5, 0 as fm1-base tuned it.
 * The host build (DINGHY_HOST): fk_touch_host_key(k, vel, press %) plays a key through fm1-base's own key model;
 * fk_touch_host_tick() once a millisecond. */
#pragma once
#include <stdint.h>

#ifndef FK_TOUCH
#define FK_TOUCH 1
#endif

#define FK_TOUCH_NKEYS 27u
#define FK_TOUCH_ANALOG 0x000FFFFFu       /* keys 0..19 (F3..C5): rows PA7 and PA8 (FM1_KEYMAP ids 14..33) */
#define FK_TOUCH_CURVE_MIN (-5)
#define FK_TOUCH_CURVE_MAX 5

#if FK_TOUCH
#include "touch.h"                        /* APP_KEY_SENSE: app_key_sample, app_key_drive, app_key_probe, app_key_led_ok */
/* the scan's hooks (hal/fm1_input_an.h): a reading by (column, row bit) to the key it is, F3 = 0 */
#define FK_TOUCH_KEY_OF(col, rowbit) ((uint32_t)FM1_KEYMAP[rowbit][col] - 14u)
#define FM1_INPUT_ANALOG(col, rowbit, v, driven) \
    (FM1_KEYMAP[rowbit][col] >= 14 ? app_key_sample(FK_TOUCH_KEY_OF(col, rowbit), (v), fm1_ticks(), (int)(driven)) : (void)0)
#define FM1_INPUT_DRIVE(col, rowbit) (FM1_KEYMAP[rowbit][col] >= 14 && app_key_drive(FK_TOUCH_KEY_OF(col, rowbit)))
#define FM1_INPUT_PROBE(col, rowbit, v, pressed) \
    (FM1_KEYMAP[rowbit][col] >= 14 ? app_key_probe(FK_TOUCH_KEY_OF(col, rowbit), (v), (pressed)) : (void)0)
#define FM1_INPUT_LED_OK(col, rowbit) (FM1_KEYMAP[rowbit][col] < 14 || app_key_led_ok(FK_TOUCH_KEY_OF(col, rowbit)))

#define FK_TOUCH_API static __attribute__((unused))   /* (an app may use only some of them) */
FK_TOUCH_API void fk_touch_enable(int on);
FK_TOUCH_API int fk_touch_on(void);
FK_TOUCH_API void fk_touch_set(int curve, int sens);
FK_TOUCH_API void fk_touch_poll(void);
FK_TOUCH_API uint32_t fk_touch_keys(void);
FK_TOUCH_API uint32_t fk_touch_vel(uint32_t key);
FK_TOUCH_API uint32_t fk_touch_pressure(uint32_t key);
FK_TOUCH_API uint32_t fk_touch_channel_pressure(void);
FK_TOUCH_API uint32_t fk_touch_last_vel(void);
#ifdef DINGHY_HOST
static void fk_touch_host_key(uint32_t k, uint32_t vel, uint32_t press);
static void fk_touch_host_tick(void);
#endif
#else
/* no sensing (FK_TOUCH 0): the keys as the scan has them, velocity 100, no pressure */
#define fk_touch_enable(on) ((void)(on))
#define fk_touch_on() 0
#define fk_touch_set(curve, sens) ((void)(curve), (void)(sens))
#define fk_touch_poll() ((void)0)
#define fk_touch_keys() ((uint32_t)fm1_in.notes)
#define fk_touch_vel(key) ((void)(key), 100u)
#define fk_touch_pressure(key) ((void)(key), 0u)
#define fk_touch_channel_pressure() 0u
#define fk_touch_last_vel() 0u
#endif
