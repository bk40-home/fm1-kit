/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kris (bk40-home) */
/* fk_touch's tests on a computer (tests/run.sh): the glue and fm1-base's touch.c / sense.c through touch.c's own key
 * model (PLAT_HOST: what the FM-1's analog rows would read for a key landing at a velocity and pressed harder).
 *   off           the keys as scanned, velocity 100, no pressure
 *   velocity      F3..C5 and the top row land at the velocity asked (within 2)
 *   pressure      a held key pressed harder: 0 while it settles, then up to the share asked, back to 0 at release;
 *                 channel pressure is the hardest-pressed key; the top row has none
 *   curve, sens   CURVE + makes the same landing softer, - harder; SENS + more pressure for the same lean
 *   off again     the held sensed keys end (the keys change hands), velocity 100 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- the platform fk_touch.c expects from Dinghy's platform.c (the host's) */
static volatile uint32_t fm1_ms;
static volatile struct { uint32_t notes; } fm1_in;
#define DINGHY_HOST 1
#define FK_TOUCH 1
#include "../fk_touch.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void run_ms(uint32_t n)
{
    while (n--) {
        fm1_ms++;
        fk_touch_host_tick();
        fk_touch_poll();
    }
}
/* key k down at vel (and the scan's bit: the top row is the digital scan's), held for ms, pressed press % harder */
static void down(uint32_t k, uint32_t vel, uint32_t press)
{
    fk_touch_host_key(k, vel, press);
    fm1_in.notes |= 1u << k;
}
static void up(uint32_t k)
{
    fk_touch_host_key(k, 0, 0);
    fm1_in.notes &= ~(1u << k);
}
static uint32_t iabs(int32_t v) { return (uint32_t)(v < 0 ? -v : v); }

/* the velocity key k's press reads, landing at vel (curve c), 0 when the key never shows as down */
static uint32_t press_vel(uint32_t k, uint32_t vel, int c_before, int c_after)
{
    uint32_t v;
    fk_touch_set(c_before, 0);
    down(k, vel, 0);
    run_ms(2);                                     /* (the model computes its landing on its first sample) */
    fk_touch_set(c_after, 0);
    run_ms(60);
    v = (fk_touch_keys() >> k & 1u) ? fk_touch_vel(k) : 0u;
    up(k);
    run_ms(60);
    fk_touch_set(0, 0);
    return v;
}

int main(void)
{
    static const uint32_t VELS[] = {1, 20, 45, 64, 80, 100, 127};
    uint32_t i, k, v, p, peak;

    /* off */
    fm1_in.notes = 1u << 3;
    CHECK(!fk_touch_on(), "off at start");
    CHECK(fk_touch_keys() == 1u << 3, "off: keys %08x as scanned", (unsigned)fk_touch_keys());
    CHECK(fk_touch_vel(3) == 100u, "off: velocity %u", (unsigned)fk_touch_vel(3));
    CHECK(fk_touch_channel_pressure() == 0u, "off: pressure");
    fm1_in.notes = 0;

    fk_touch_enable(1);
    CHECK(fk_touch_on(), "on");
    run_ms(20);

    /* velocity: every sensed key, and the top row */
    for (k = 0; k < FK_TOUCH_NKEYS; k++)
        for (i = 0; i < sizeof VELS / sizeof VELS[0]; i++) {
            v = press_vel(k, VELS[i], 0, 0);
            CHECK(v && iabs((int32_t)v - (int32_t)VELS[i]) <= 2u, "key %u at %u: velocity %u", (unsigned)k, (unsigned)VELS[i], (unsigned)v);
        }
    CHECK(fk_touch_last_vel() != 0u, "last velocity kept");

    /* curve: the same landing, read with CURVE +5 softer, -5 harder */
    v = press_vel(9, 64, 0, 0);
    p = press_vel(9, 64, 0, 5);
    i = press_vel(9, 64, 0, -5);
    CHECK(p + 5u < v && i > v + 5u, "curve: 0 %u, +5 %u, -5 %u", (unsigned)v, (unsigned)p, (unsigned)i);

    /* pressure: key 9 (A3) held 60 % harder, key 4 30 % */
    down(9, 80, 60);
    run_ms(100);
    CHECK(fk_touch_pressure(9) == 0u, "pressure while settling: %u", (unsigned)fk_touch_pressure(9));
    run_ms(600);
    p = fk_touch_pressure(9);
    CHECK(iabs((int32_t)p - 76) <= 4u, "pressure 60 %%: %u", (unsigned)p);
    down(4, 80, 30);
    run_ms(700);
    CHECK(fk_touch_pressure(4) > 20u && fk_touch_pressure(4) < p, "pressure 30 %%: %u", (unsigned)fk_touch_pressure(4));
    CHECK(fk_touch_channel_pressure() == p, "channel pressure %u is the hardest key's %u", (unsigned)fk_touch_channel_pressure(), (unsigned)p);
    up(9);
    up(4);
    run_ms(60);
    CHECK(fk_touch_channel_pressure() == 0u, "pressure after release: %u", (unsigned)fk_touch_channel_pressure());

    /* sens: + more pressure for the same lean (the model leans by the share asked at the sens in use: set it after
     * the landing, as with the curve) */
    fk_touch_set(0, 0);
    down(9, 80, 40);
    run_ms(700);
    v = fk_touch_pressure(9);
    fk_touch_set(0, 5);
    run_ms(300);
    peak = fk_touch_pressure(9);
    up(9);
    run_ms(60);
    fk_touch_set(0, 0);
    CHECK(peak > v, "sens +5: %u, 0: %u", (unsigned)peak, (unsigned)v);

    /* the top row: velocity, no pressure */
    down(22, 90, 80);
    run_ms(700);
    CHECK(fk_touch_keys() >> 22 & 1u, "top row held");
    CHECK(fk_touch_pressure(22) == 0u, "top row: no pressure (%u)", (unsigned)fk_touch_pressure(22));
    up(22);
    run_ms(60);

    /* off again, a sensed key held: it ends (touch_on: the keys change hands); then the scan's keys, 100 */
    down(9, 80, 0);
    run_ms(60);
    CHECK(fk_touch_keys() >> 9 & 1u, "held before off");
    fm1_in.notes &= ~(1u << 9);                    /* (the scan let it go too) */
    fk_touch_enable(0);
    CHECK(!(fk_touch_keys() >> 9 & 1u), "off: the sensed key ends");
    fm1_in.notes = 1u << 9;
    CHECK(fk_touch_keys() == 1u << 9 && fk_touch_vel(9) == 100u && fk_touch_pressure(9) == 0u, "off: as scanned, 100, 0");

    printf(fails ? "fk_touch_test: %d FAILED\n" : "fk_touch_test: ok\n", fails);
    return fails != 0;
}
