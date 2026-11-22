/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kris (bk40-home): the glue. touch.c, touch.h, sense.c, fastmath.h: Charles Vestal (fm1-base) */
/* fk_touch's glue (fk_touch.h says how to use it). Part of platform.c's unity build, after fm1_input.h, fm1_adc.h and
 * fm1_ms: integers only, as the platform is. It gives fm1-base's touch.c (fk_touch_fpu.c, built apart with the FPU's
 * flags on the chip) the platform it asks for, and the app its interface. */
#if FK_TOUCH
#include "fk_touch.h"

/* ---- the platform touch.c asks for (fk_touch_fpu.c) */
uint32_t fk_plat_ms(void) { return fm1_ms; }
uint32_t fk_plat_keys(void) { return fm1_in.notes; }

/* the scan reads the analog rows (hal/fm1_input_an.h), as fm1-base's plat_sense (device/plat_fm1.c): the converter's
 * clock 7 / delay 2 (a conversion fits between two ticks), a held key driven, the top row probed. MASTER comes from
 * the scan then (fm1_an.master): read once now so it does not dip to 0 before the scan's first slot. Off: the scan
 * lets the converter go after its last conversion, and the main loop has it again */
void fk_plat_sense(int on)
{
#ifdef DINGHY_HOST
    (void)on;
#else
    if (!on) {
        uint32_t t0 = fm1_ticks();
        fm1_an.con = 0;
        while (fm1_an.pend && fm1_ticks() - t0 < 1000u * FM1_TICKS_PER_US)
            ;                                     /* (the tick takes a pending one within 100 us) */
        return;
    }
    if (!fm1_an.con) {
        int32_t a = fm1_adc_read(FM1_ADC_MASTER);
        if (a >= 0)
            fm1_an.master = (uint16_t)a;
    }
    fm1_an.drive = 1;
    fm1_an.probe = 1;
    fm1_an.con = (uint16_t)FM1_ADC_CON(7, 2);
#endif
}

#ifdef DINGHY_HOST
#include "fk_touch_fpu.c"                         /* (a computer: floats anywhere; the chip links it as its own object) */
#endif

/* ---- the interface (fk_touch.h) */
static uint8_t fk__last_vel;

static void fk_touch_enable(int on) { touch_on(on); }
static int fk_touch_on(void) { return touch_is_on(); }
static void fk_touch_set(int curve, int sens)
{
    touch_curve(curve);
    touch_sens(sens);
}
static void fk_touch_poll(void)
{
    if (touch_is_on())
        touch_poll();
}
static uint32_t fk_touch_keys(void) { return touch_keys(); }   /* (off: fm1_in.notes) */
static uint32_t fk_touch_vel(uint32_t key)
{
    uint32_t v;
    if (!touch_is_on() || key >= FK_TOUCH_NKEYS)
        return 100u;
    v = touch_vel(key);
    fk__last_vel = (uint8_t)v;
    return v;
}
static uint32_t fk_touch_pressure(uint32_t key) { return touch_is_on() ? touch_pressure(key) : 0u; }
static uint32_t fk_touch_channel_pressure(void)
{
    uint32_t k, p = 0;
    if (!touch_is_on())
        return 0;
    for (k = 0; k < FK_TOUCH_NKEYS; k++)
        if (FK_TOUCH_ANALOG >> k & 1u) {
            uint32_t q = touch_pressure(k);
            if (q > p)
                p = q;
        }
    return p;
}
static uint32_t fk_touch_last_vel(void) { return fk__last_vel; }

#ifdef DINGHY_HOST
/* the host: key k held, landing at vel (0: up), pressed press % harder than it settled (touch.c's model) */
static void fk_touch_host_key(uint32_t k, uint32_t vel, uint32_t press) { app_host_key(k, vel, press); }
static void fk_touch_host_tick(void)
{
    if (touch_is_on())
        app_host_tick();
}
#endif
#endif
