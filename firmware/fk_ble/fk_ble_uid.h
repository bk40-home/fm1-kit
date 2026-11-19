/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; Charles Vestal (felucca-bsp chip/fm1_flash.h
 * fl_uid_ram, which this repeats on Dinghy's flash helpers) */
/* The flash's factory unique ID, for fk_ble_attach: an address that is the same on every boot. Include it after the
 * HAL's fm1_flash.h (fl_enter, fl_cs, fl_tx, fl_rx, fl_exit, RAMFN). Call it as the HAL's other flash entry points:
 * interrupts off, through FL_FAR, dst in RAM:
 *     static uint8_t uid[16];
 *     if (flash_ok && FL_FAR(fk_ble_uid_ram)(uid) == 0) fk_ble_attach(uid); else fk_ble_attach(0);
 * 0x4B and four dummy bytes; the stock Puya P25Q80 gives 16 bytes (a Winbond-style part 8, the rest 0xFF): the same on
 * every read of the same part. */
#ifndef FK_BLE_UID_H
#define FK_BLE_UID_H
static RAMFN int fk_ble_uid_ram(uint8_t *dst)
{
    fl_saved_t s;
    uint32_t i;
    if (fl_enter(&s))
        return -1;
    fl_cs(0);
    for (i = 0; i < 5u; i++)
        fl_tx(i ? 0 : 0x4B);
    for (i = 0; i < 16u; i++)
        dst[i] = fl_rx();
    fl_cs(1);
    fl_exit(&s);
    return 0;
}
#endif
