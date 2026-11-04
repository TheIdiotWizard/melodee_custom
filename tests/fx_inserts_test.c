/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* A track's inserts, restored: any audio effect in a track's three slots, through the real mixer, and the one REVERB
 * (choosing it in a track's slot takes it from the shared rack, and back). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static void reset_all(void)
{
    ui_power_on();
    for (uint32_t id = 0; id < RES_COUNT; id++) resource_release(id);
    memset(fx_state, 0, sizeof fx_state);
    fx_clear_all();
    song.master_q12 = 4096;                                 /* (the host's master pot: up) */
}
static uint64_t render(uint32_t blocks, int note)
{
    int32_t out[CTL * 2];
    uint64_t e = 0;
    set_engine_of(&trk[0], ENGI_FM6); apply_preset_to(&trk[0], 8);
    for (uint32_t k = 0; k < 40; k++) mix_block(out, CTL);   /* (the engine switch done) */
    if (note) trk_note_on(&trk[0], 60, 100);
    for (uint32_t k = 0; k < blocks; k++) { mix_block(out, CTL); for (uint32_t i = 0; i < CTL * 2u; i++) e += (uint32_t)abs(out[i]); }
    if (note) trk_note_off(&trk[0], 60);
    return e;
}
static void mixer_test(void)
{
    reset_all();
    uint64_t dry = render(200, 1);
    reset_all();
    trk[0].p[P_FXTYPE] = FX_CRUSH; trk[0].p[P_FXMIX] = 127; trk[0].p[P_FXRATE] = 10; trk[0].p[P_FXCHAR] = 90;
    uint64_t wet = render(200, 1);
    assert(dry && wet && wet != dry);                       /* the insert changes the track */
    assert(resource[RES_FX0 + FX_TRACK_SI(0, 0)].size);     /* its buffer from the arena */
    trk[0].p[P_FXMIX] = 0;
    render(64, 0);
    assert(!resource[RES_FX0 + FX_TRACK_SI(0, 0)].size);    /* MIX 0: given back */
    puts("Track inserts in the mixer: an audible CRUSH, its arena buffer given back at MIX 0: OK");
}
static void reverb_test(void)
{
    reset_all();
    song.g[G_MFXTYPE] = FX_REVERB;
    go_title("FX"); fx_rack_row[0] = 0;
    TSEL->p[P_FXTYPE] = FX_ROOM;
    turn(EN_K1, 1);                                         /* ROOM -> REVERB on the track */
    assert(TSEL->p[P_FXTYPE] == FX_REVERB && song.g[G_MFXTYPE] == FX_OFF);
    go_title("SHARED"); fx_rack_row[1] = 1;
    song.g[G_MFX2TYPE] = FX_ROOM; turn(EN_K1, 1);           /* and back to the shared rack */
    assert(song.g[G_MFX2TYPE] == FX_REVERB && TSEL->p[P_FXTYPE] == FX_OFF);
    puts("One REVERB: a track's slot takes it from the shared rack, and the shared rack from the track: OK");
}
int main(void)
{
    mixer_test(); reverb_test();
    puts("FX inserts: passed");
    return 0;
}
