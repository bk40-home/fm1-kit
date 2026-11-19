/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kris (bk40-home) */
/* fk_ble: Bluetooth MIDI for a Dinghy-based FM-1 firmware (M-Vsynth2, M-vance), as a drop-in module.
 *
 * The stack is Henri David's own BLE stack from Optimist, as fm1-base carries it (ble_stack = "own"): nothing of
 * JieLi's is linked, so a GPL-3.0 firmware can ship it. The FM-1 is a BLE-MIDI peripheral: a Mac, an iPhone or an
 * iPad connects to it. One link at a time, no pairing (macOS and iOS connect without it). See README.md.
 *
 * Include fk_ble.c once, in the platform's unity build, after the HAL (fm1_time.h, fm1_irq.h, fm1_cc.h) and with
 * fk_ble/chip and fk_ble/stack on the include path. Before the #include the firmware may define:
 *   FK_BLE_NAME "M-Vsynth2"   the advertised name; " XXXX" (from the address) follows it. At most 19 characters
 *   FK_BLE_MS()               milliseconds since power-on (default: fm1_ms)
 *
 * The calls (all from the main loop, unless said):
 *   fk_ble_attach(uid)        at boot, interrupts off, before the vector table is locked (fm1_guard_lock_top): the
 *                             two BLE vectors, masked, and the address. uid: the flash's 16-byte unique ID
 *                             (fk_ble_uid.h), so the address is the same on every boot; 0 for a random one. No radio
 *                             register is touched
 *   fk_ble_enable(on)         the player's switch. The first on starts the radio: it calibrates (about 0.5 s, the main
 *                             loop held; see fk_ble_quiet) and advertises. Off ends the link and stops advertising; on
 *                             again advertises. Call it at boot, before the audio starts, when the setting is ON
 *   fk_ble_poll()             often (every few ms): nothing else needs calling
 *   fk_ble_get(&pkt)          1: a USB-MIDI packet in from Bluetooth (cable 0; CIN in bits 0..3, status in 8..15,
 *                             data in 16..31, as usb.c's ring); 0: none. Channel messages and the one-byte system
 *                             ones (F8 FA FB FC ...) only: SysEx from Bluetooth is dropped (see README.md)
 *   fk_ble_send(pkt)          a USB-MIDI packet out to the computer (dropped when nothing listens or the ring is
 *                             full); 1 queued, 0 not
 *   fk_ble_state()            FK_BLE_OFF, FK_BLE_ADV (on, nothing connected), FK_BLE_LINKED (connected, MIDI on)
 *   fk_ble_quiet()            1 for the first second after fk_ble_start: the audio interrupt should render silence
 *                             and read nothing from the flash (fm1-base radio.c radio_quiet: a flash read during the
 *                             radio's trim can come back corrupted)
 *   fk_ble_lost()             messages in from Bluetooth dropped because the ring was full (for a DIAG page)
 */
#ifndef FK_BLE_H
#define FK_BLE_H
#include <stdint.h>

enum { FK_BLE_OFF = 0, FK_BLE_ADV = 1, FK_BLE_LINKED = 2 };

static void fk_ble_attach(const uint8_t *uid);
static void fk_ble_start(void);                   /* (fk_ble_enable's: the first on) */
static void fk_ble_enable(int on);
static void fk_ble_poll(void);
static int fk_ble_get(uint32_t *pkt);
static int fk_ble_send(uint32_t pkt);
static int fk_ble_state(void);
static int fk_ble_quiet(void);
static uint32_t fk_ble_lost(void);

#endif
