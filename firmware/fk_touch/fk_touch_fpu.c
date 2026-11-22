/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kris (bk40-home): the glue. touch.c, touch.h, sense.c, fastmath.h: Charles Vestal (fm1-base) */
/* fk_touch's float half: fm1-base's touch.c and sense.c, unchanged, in a translation unit of their own. They use
 * single-precision floats (the velocity's log scale, the pressure), so on the chip this file is built apart with the
 * FPU's flags, as vs_dsp.c is (tools/build.py: -mcpu=r3 -mfprev1 -ffp-contract=off -Werror=double-promotion), while
 * the platform and the app stay integer-only. It touches no hardware: what touch.c asks of its platform comes through
 * the fk_plat_* functions, which fk_touch.c (the platform's unity build) defines. On a computer (DINGHY_HOST)
 * fk_touch.c includes this file instead, with touch.c's key model (PLAT_HOST). */
#include <stdint.h>

/* ---- fm1-base's platform, as touch.c uses it (fm1-base include/plat.h) */
#define NKEYS 27
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
uint32_t fk_plat_ms(void);                /* fk_touch.c: milliseconds (fm1_ms) */
uint32_t fk_plat_keys(void);              /* fk_touch.c: the scan's keys (fm1_in.notes) */
void fk_plat_sense(int on);               /* fk_touch.c: the scan reads the analog rows (fm1_an) */
#define plat_ms() fk_plat_ms()
#define plat_keys() fk_plat_keys()
#define plat_sense(on) fk_plat_sense(on)
#define plat_remote_keys() 0u
#define plat_analog_mask() 0x000FFFFFu    /* keys 0..19 (F3..C5), rows PA7 and PA8: FK_TOUCH_ANALOG */
#define FM1B_MIRROR 0
#ifdef DINGHY_HOST
#define PLAT_HOST 1                       /* touch.c's key model: app_host_key, app_host_tick */
#endif

/* its buffers (about 2.8 KB, all zero at start) are this object's .bss: a firmware whose RAM is full places them
 * from its linker script (M-vance's app.ld puts fk_touch_fpu.o's .bss in .pool) */
#include "touch.h"
/* fm1-base's files as they are: helpers this build does not call (the calibration's save and load, the raw
 * aftertouch), and two of its comparisons that gcc's -Wextra asks to be cast, are theirs to change, not ours */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "touch.c"
#pragma GCC diagnostic pop
