/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kris (bk40-home) */
/* fk_sustain: the sustain pedal for any engine (Kris 25 Nov 2026: "need that for all engines"). Header only, no
 * hardware, no engine of its own: it sits between the note-offs and the engine.
 *
 * A note let go while the pedal is down is held: its note-off is kept back (with the caller's mask: the parts it was
 * meant for, or 1) and sent when the pedal comes up. A note struck again while held is the caller's note-on as usual,
 * and is no longer held (the next let-go decides). The pedal is CC64 (>= 64 down, as MIDI says), or any switch.
 * Only the SOUND waits: what a firmware sends out (MIDI to a computer) it sends as before – the computer has its own
 * sustain.
 *
 *   static fk_sus_t sus;                                  zeroed: pedal up, nothing held (fk_sus_init does the same)
 *   on a note-on:    fk_sus_note_on(&sus, note);  then the engine's note-on
 *   on a note-off:   if (fk_sus_note_off(&sus, note, mask)) engine note-off      (0: held, the pedal will send it)
 *   on CC64 / pedal: fk_sus_pedal(&sus, value, my_off, ctx);   my_off(ctx, note, mask) for each held one on release
 *
 * Used by JT-80x0 (FM-1: firmware/src/mvance.c; VIBE FM: vibe/jt80x0/jt_midi.h). One thread: call it where the notes
 * are handled (the main loop; a VIBE firmware's control flow). */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t on;                                 /* the pedal is down */
    uint8_t held[128];                          /* a note let go while it was: the caller's mask (0 = not held) */
} fk_sus_t;

typedef void (*fk_sus_off_fn)(void *ctx, uint32_t note, uint32_t mask);

static inline void fk_sus_init(fk_sus_t *s)
{
    uint32_t n;
    s->on = 0;
    for (n = 0; n < 128u; n++) s->held[n] = 0;
}

static inline int fk_sus_down(const fk_sus_t *s) { return s->on != 0; }

/* a note-on: if it was held, it is not any more (the caller plays it as always) */
static inline void fk_sus_note_on(fk_sus_t *s, uint32_t note)
{
    if (note < 128u) s->held[note] = 0;
}

/* a note-off: 1 = send it now; 0 = held (pedal down): it goes on release. mask: the caller's (parts, channels: 1..255) */
static inline int fk_sus_note_off(fk_sus_t *s, uint32_t note, uint32_t mask)
{
    if (note >= 128u || !s->on) return 1;
    s->held[note] |= (uint8_t)(mask ? mask : 1u);
    return 0;
}

/* the pedal: value >= 64 down (CC64), else up. On release every held note-off goes to off(); returns how many */
static inline uint32_t fk_sus_pedal(fk_sus_t *s, uint32_t value, fk_sus_off_fn off, void *ctx)
{
    uint32_t n, sent = 0, down = value >= 64u;
    if (down == s->on) return 0;
    s->on = (uint8_t)down;
    if (down) return 0;
    for (n = 0; n < 128u; n++)
        if (s->held[n]) { uint32_t m = s->held[n]; s->held[n] = 0; if (off) off(ctx, n, m); sent++; }
    return sent;
}
