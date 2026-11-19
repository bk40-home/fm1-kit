/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kris (bk40-home) */
/* fk_ble's glue on a computer (FK_BLE_HOST): the rings both ways, what is let in, the state, the quiet second, the
 * address. The stack and the radio are stood in for here; the stack's own tests are fm1-base's tests/bt/run.sh.
 *   sh tests/run.sh   (from fk_ble/) */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t fm1_ms, ticks = 12345u;
static uint32_t fm1_ticks(void) { return ticks += 977u; }
#define BLE_API static
/* the stack, stood in for */
static int st_init, st_enable = -1, st_ready, st_conn;
static uint8_t st_addr[6];
static void ble_init(const uint8_t a[6], uint8_t rnd) { memcpy(st_addr, a, 6); st_init += rnd == 1u; }
static void ble_enable(int on) { st_enable = on; }
static int ble_midi_ready(void) { return st_ready; }
static int ble_connected(void) { return st_conn; }

#define FK_BLE_HOST 1
#define FK_BLE_NAME "M-Vsynth2"
#include "../fk_ble.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-70s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static uint32_t pk(uint32_t st, uint32_t d1, uint32_t d2) { return (st >> 4) | st << 8 | d1 << 16 | d2 << 24; }
static void link(int on) { st_conn = on; st_ready = on; ble_app_state(); }

int main(void)
{
    uint32_t p, t, i, n;
    uint8_t uid[16], a1[6], a2[6];

    /* ---- off until enabled */
    for (i = 0; i < 16u; i++) uid[i] = (uint8_t)(0x30u + i * 7u);
    fk_ble_attach(uid);
    check("attach: nothing started, state OFF", !st_init && fk_ble_state() == FK_BLE_OFF);
    check("attach: the address's top two bits 11 (static random)", (fk_addr[5] & 0xC0u) == 0xC0u);
    memcpy(a1, fk_addr, 6);
    fk_ble_attach(uid);
    check("the same unique ID: the same address on every boot", !memcmp(a1, fk_addr, 6));
    uid[3] ^= 1u;
    fk_ble_attach(uid);
    memcpy(a2, fk_addr, 6);
    check("another unique ID: another address", memcmp(a1, a2, 6) != 0);
    memset(uid, 0xFF, sizeof uid);
    fk_ble_attach(uid);
    check("no ID (all FF): a random one, still static random", (fk_addr[5] & 0xC0u) == 0xC0u && memcmp(a2, fk_addr, 6));
    fk_ble_attach(0);
    check("no ID (0): a random one", (fk_addr[5] & 0xC0u) == 0xC0u);
    uid[3] = 0x31u;
    fk_ble_attach(uid);
    memcpy(a1, fk_addr, 6);
    check("not sent while OFF", !fk_ble_send(pk(0x90, 60, 100)) && fk_ow == 0u);

    /* ---- the first ON starts it, quiet for a second */
    fm1_ms = 5000u;
    fk_ble_enable(1);
    check("ON: the stack started with our address (random type), advertising", st_init == 1 && !memcmp(st_addr, a1, 6) &&
          st_enable == 1 && fk_ble_state() == FK_BLE_ADV);
    check("ON: quiet at once (before the trim)", fk_ble_quiet());
    fm1_ms = 5900u; fk_ble_poll();
    check("still quiet at 0.9 s", fk_ble_quiet());
    fm1_ms = 6001u; fk_ble_poll();
    check("the render back after 1 s", !fk_ble_quiet());
    fk_ble_enable(1);
    check("ON again: not started twice", st_init == 1);
    check("advertising: nothing goes out", !fk_ble_send(pk(0x90, 60, 100)));

    /* ---- linked: out */
    link(1);
    check("a computer connected, MIDI on: LINKED", fk_ble_state() == FK_BLE_LINKED);
    st_ready = 0;
    check("connected, notifications not on yet: still ADV", fk_ble_state() == FK_BLE_ADV);
    st_ready = 1;
    fm1_ms = 0x12345u;
    check("a note out queued", fk_ble_send(pk(0x90, 60, 100) | 0x30u));
    check("the stack peeks it: cable 0, its 13-bit ms", ble_app_midi_peek(&p, &t) && p == pk(0x90, 60, 100) && t == (0x12345u & 0x1FFFu));
    ble_app_midi_pop();
    check("popped: empty", !ble_app_midi_peek(&p, &t));
    for (n = 0; fk_ble_send(pk(0xB0, 7, n & 127u)); n++) ;
    check("out ring: 64 then full (dropped, not overwritten)", n == FK_OQ);
    for (i = 0; ble_app_midi_peek(&p, &t); i++) {
        if (p != pk(0xB0, 7, i & 127u)) break;
        ble_app_midi_pop();
    }
    check("out ring: all 64 in order", i == FK_OQ);

    /* ---- in: what is let through */
    ble_app_midi_in(pk(0x91, 64, 90) | 0x20u, 0, 0);
    ble_app_midi_in(0x0Fu | 0xF8u << 8, 0, 0);                 /* clock */
    ble_app_midi_in(0x04u | 0xF0u << 8 | 0x7Du << 16 | 0x4Du << 24, 0, 0);   /* SysEx start */
    ble_app_midi_in(0x07u | 0x56u << 8 | 0x01u << 16 | 0xF7u << 24, 0, 0);   /* SysEx end */
    ble_app_midi_in(0x02u | 0xF3u << 8 | 0x05u << 16, 0, 0);   /* song select: system common */
    ble_app_midi_in(0x09u | 0x80u << 8 | 60u << 16, 0, 0);     /* a CIN that disagrees with its status */
    ble_app_midi_in(pk(0xE0, 0, 64), 0, 0);                     /* bend */
    check("in: the note (cable 0)", fk_ble_get(&p) && p == pk(0x91, 64, 90));
    check("in: the clock", fk_ble_get(&p) && p == (0x0Fu | 0xF8u << 8));
    check("in: SysEx, system common and a bad CIN dropped; the bend next", fk_ble_get(&p) && p == pk(0xE0, 0, 64));
    check("in: empty", !fk_ble_get(&p));
    for (i = 0; i < FK_IQ + 5u; i++)
        ble_app_midi_in(pk(0x90, i & 127u, 1), 0, 0);
    check("in ring full: the 5 past it counted lost", fk_ble_lost() == 5u);
    for (i = 0; fk_ble_get(&p); i++)
        if (p != pk(0x90, i & 127u, 1)) break;
    check("in ring: the first 64 in order", i == FK_IQ);

    /* ---- off and on */
    fk_ble_enable(0);
    check("OFF: the stack told, state OFF, nothing out", st_enable == 0 && fk_ble_state() == FK_BLE_OFF &&
          !fk_ble_send(pk(0x90, 1, 1)));
    fk_ble_enable(1);
    check("ON again: advertising (not linked until a computer connects)", st_enable == 1 && fk_ble_state() == FK_BLE_ADV);
    link(1);
    check("connected again: LINKED", fk_ble_state() == FK_BLE_LINKED);
    link(0);
    check("disconnected: ADV", fk_ble_state() == FK_BLE_ADV);

    /* ---- wrap: the 32-bit indices past their top */
    fk_iw = fk_ir = 0xFFFFFFF0u;
    fk_ow = fk_or = 0xFFFFFFF0u;
    link(1);
    for (i = 0; i < 40u; i++) {
        ble_app_midi_in(pk(0x90, i, 2), 0, 0);
        if (!fk_ble_get(&p) || p != pk(0x90, i, 2)) break;
        if (!fk_ble_send(pk(0x80, i, 0)) || !ble_app_midi_peek(&p, &t) || p != pk(0x80, i, 0)) break;
        ble_app_midi_pop();
    }
    check("both rings across the index wrap", i == 40u);

    printf(fails ? "fk_ble: %d FAILED\n" : "fk_ble: all passed\n", fails);
    return fails != 0;
}
