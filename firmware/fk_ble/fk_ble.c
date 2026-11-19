/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal (fm1-base device/bt_own.c: the start, the address, the two rings, which this follows)
 * Copyright (C) 2026 Kris (bk40-home) */
/* fk_ble: Bluetooth MIDI on the own BLE stack, for a Dinghy-based FM-1 firmware. The interface is fk_ble.h; how it
 * fits a firmware is README.md. Part of the platform's unity build (one #include, after the HAL).
 *
 * No JieLi library, no shim, no tasks: the stack runs in the radio's two interrupts (IRQ 45 event, IRQ 29 RX) at
 * priority 2, below the audio (3) and TIMER5 (4), and talks to the main loop through two single-producer rings:
 *   in   the BLE interrupts write (ble_app_midi_in), the main loop reads (fk_ble_get)
 *   out  the main loop writes (fk_ble_send), the BLE event interrupt reads (ble_app_midi_peek / pop, ble_att.c)
 * The radio calibrates itself at every start (felucca-bsp chip/fm1_rf_cal.h): nothing is stored, and the address
 * comes from the flash's unique ID, so it is the same on every boot and a computer keeps recognising the FM-1.
 *
 * FK_BLE_HOST: built on a computer for the tests (tests/fk_ble_test.c): the rings and the state, with the test
 * standing in for the stack and the radio. */
#include "fk_ble.h"

#ifndef FK_BLE_NAME
#define FK_BLE_NAME "FM-1"
#endif
#ifndef FK_BLE_MS
#define FK_BLE_MS() fm1_ms
#endif
#ifndef RING_PUBLISH
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#endif
#define BLE_DEVICE_NAME FK_BLE_NAME               /* "NAME XXXX" (BLE_NAME_HEX): at most 24 characters in all */
_Static_assert(sizeof FK_BLE_NAME - 1u <= 19u, "FK_BLE_NAME: at most 19 characters (\" XXXX\" follows it)");
#ifndef BLE_DIAG
#define BLE_DIAG 0                                /* the link layer's counters left out (2.4 KB of RAM): no log here */
#endif

#ifndef FK_BLE_HOST
#include "ble_stack.c"                            /* the stack, its functions static; ble_hw_wl82.c and chip/fm1_ble.h */
#if BLE_DIAG
#include "ble_diag.c"
#endif
/* the SDK board's values for the three constant records (fm1-base bt_own.c OWN_TRIMS; RF-CALIBRATION.md §3.1, §3.3,
 * §3.4): crystal load, PA bias, Wi-Fi per-rate gains (zero: the defaults kept). Record 187 is measured at each start */
static const struct ble_rf_trims FK_TRIMS = {{0x0B, 0x0B}, {1, 7, 4, 7, 11, 1, 7}, {0}, {0}};
#endif

/* ---- the two rings (USB-MIDI packets, cable 0) */
#define FK_OQ 64u                                 /* out: the main loop writes, the BLE event interrupt reads */
static uint32_t fk_out[FK_OQ];
static uint16_t fk_out_ms[FK_OQ];
static volatile uint32_t fk_ow, fk_or;
#define FK_IQ 64u                                 /* in: the BLE interrupts write, the main loop reads */
static uint32_t fk_in[FK_IQ];
static volatile uint32_t fk_iw, fk_ir, fk_ilost;

static volatile uint8_t fk_linked;                /* a computer connected (ble_app_state, from the interrupts) */
static volatile uint8_t fk_quiet_on;              /* the first second of a start: the audio renders silence */
static uint8_t fk_started, fk_off = 1;
static uint32_t fk_t0;
static uint8_t fk_addr[6];

/* what the firmware takes from Bluetooth: channel messages (CIN 8..E) and the real-time bytes (CIN F, F8..FF).
 * SysEx (CIN 4..7) and system common are dropped: the patch transfer and the screen reader stay on USB */
static int fk_pass(uint32_t pkt)
{
    const uint32_t cin = pkt & 0x0Fu, st = (pkt >> 8) & 0xFFu;
    if (cin >= 0x8u && cin <= 0xEu)
        return (st >> 4) == cin;                  /* (the status agrees with its CIN) */
    return cin == 0xFu && st >= 0xF8u;
}

/* ---- the stack's side (BLE_API: static in the unity build) */
BLE_API int ble_app_midi_peek(uint32_t *pkt, uint32_t *t_ms)
{
    const uint32_t r = fk_or;
    if (r == fk_ow)
        return 0;
    *pkt = fk_out[r % FK_OQ];
    *t_ms = fk_out_ms[r % FK_OQ];
    return 1;
}
BLE_API void ble_app_midi_pop(void)
{
    if (fk_or != fk_ow)
        fk_or++;
}
BLE_API void ble_app_midi_in(uint32_t pkt, uint16_t ts, uint16_t last_ts)
{
    const uint32_t w = fk_iw;
    (void)ts, (void)last_ts;                      /* (delivered as it comes: the BLE-MIDI timestamps are not used) */
    if (!fk_pass(pkt))
        return;
    if (w - fk_ir >= FK_IQ) {
        fk_ilost++;
        return;
    }
    fk_in[w % FK_IQ] = pkt & ~0xF0u;              /* cable 0 */
    RING_PUBLISH();                               /* the slot before the index */
    fk_iw = w + 1u;
}
BLE_API void ble_app_state(void) { fk_linked = (uint8_t)(ble_connected() != 0); }

/* ---- the address: a static random one (Core Vol 6 Part B 1.3.2.1: the top two bits 11) from the flash's unique ID
 * (fm1-base bt_own.c own_addr) */
static void fk_make_addr(uint8_t a[6], const uint8_t *u)
{
    uint32_t h = 2166136261u, g = 0x9E3779B9u, i = 1;
    while (u && i < 16u && u[i] == u[0])
        i++;
    if (u && i < 16u) {                           /* (all one value: no ID) */
        for (i = 0; i < 16u; i++) {
            h = (h ^ u[i]) * 16777619u;
            g = (g ^ u[i]) * 2246822519u;
        }
        h ^= h >> 16;
    } else {                                      /* no ID: a random one, this boot's */
        h = fm1_ticks();
        g = fm1_ticks() * 2654435761u;
    }
    a[0] = (uint8_t)h;
    a[1] = (uint8_t)(h >> 8);
    a[2] = (uint8_t)g;
    a[3] = (uint8_t)(g >> 8);
    a[4] = (uint8_t)(g >> 16);
    a[5] = (uint8_t)((g >> 24) | 0xC0u);
}

/* ---- fk_ble.h */
static void fk_ble_attach(const uint8_t *uid)
{
    fk_make_addr(fk_addr, uid);
#ifndef FK_BLE_HOST
    ble_hw_wl82_attach();                         /* the two vectors, masked until the radio runs */
#endif
}

static void fk_ble_start(void)
{
    if (fk_started)
        return;
    fk_started = 1;
    fk_t0 = FK_BLE_MS();
    fk_quiet_on = 1;                              /* before the trim: the audio interrupt stops reading the flash */
    RING_PUBLISH();
    ble_init(fk_addr, 1);
#ifndef FK_BLE_HOST
    fm1_rf_cal_on = 1;                            /* (our own measurement of record 187, not a stored one) */
    ble_hw_wl82_start(&FK_TRIMS);                 /* the radio's start-up and calibration (~0.5 s, from RAM) */
    fm1_ble_irqs_hold(1);
#endif
    ble_enable(!fk_off);
#ifndef FK_BLE_HOST
    fm1_ble_irqs_hold(0);
#endif
}

static void fk_ble_enable(int on)
{
    fk_off = (uint8_t)!on;
    if (!fk_started) {
        if (on)
            fk_ble_start();
        return;
    }
#ifndef FK_BLE_HOST
    fm1_ble_irqs_hold(1);
#endif
    ble_enable(on);                               /* off: TERMINATE_IND to a computer, advertising stops */
#ifndef FK_BLE_HOST
    fm1_ble_irqs_hold(0);
#endif
    if (!on)
        fk_linked = 0;
}

static void fk_ble_poll(void)
{
    if (fk_quiet_on && FK_BLE_MS() - fk_t0 > 1000u)
        fk_quiet_on = 0;                          /* the trim's high current over: the render back */
}

static int fk_ble_get(uint32_t *pkt)
{
    const uint32_t r = fk_ir;
    if (r == fk_iw)
        return 0;
    *pkt = fk_in[r % FK_IQ];
    RING_PUBLISH();                               /* the slot read before it is given back */
    fk_ir = r + 1u;
    return 1;
}

static int fk_ble_state(void)
{
    if (!fk_started || fk_off)
        return FK_BLE_OFF;
    return fk_linked && ble_midi_ready() ? FK_BLE_LINKED : FK_BLE_ADV;
}

static int fk_ble_send(uint32_t pkt)
{
    const uint32_t w = fk_ow;
    if (fk_ble_state() != FK_BLE_LINKED || w - fk_or >= FK_OQ)
        return 0;
    fk_out[w % FK_OQ] = pkt & ~0xF0u;
    fk_out_ms[w % FK_OQ] = (uint16_t)(FK_BLE_MS() & 0x1FFFu);   /* BLE-MIDI's 13-bit millisecond timestamp */
    RING_PUBLISH();
    fk_ow = w + 1u;
    return 1;
}

static int fk_ble_quiet(void) { return fk_quiet_on; }
static uint32_t fk_ble_lost(void) { return fk_ilost; }
