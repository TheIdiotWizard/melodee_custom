/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* MONO2K's four and Mini-Tone's three in the UI (the shipping build; tests/ui_test.c's stubs, not its own tests):
 *   build/host/m2k_ui_test
 * EDIT's FILTER page (MODE CUT RES ENV) is there for SAW ENS WAV FMTONE DUALVCO ANALOG4 only, not NOISE (its own
 * filter), LOFI, DRUM or the retired STAT PAR DYN; its knobs turn its values; FMTONE's and ANALOG4's own pages show on their engine only and
 * their knobs turn the controls kept in the operator slots. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    int bad = 0, ok = 1;
    uint32_t pg = NPAGES;
    for (uint32_t i = 0; i < NPAGES; i++) if (!strcmp(PAGES[i].title, "FILTER")) pg = i;
    static const struct { uint8_t e, on; } E[] = {{ENGI_SAW, 1}, {ENGI_SAW + 1u, 1}, {ENGI_WAV, 1}, {11u, 0},
        {ENGI_FMTONE, 1}, {ENGI_DUAL, 1}, {ENGI_ANA4, 1}, {ENGI_PULS, 1}, {ENGI_SAW + 3u, 0}, {ENGI_SAW + 4u, 0},
        {3u, 0}, {ENGI_DRUM, 0}};
    ui_power_on();
    for (uint32_t k = 0; k < NELEM(E) && pg < NPAGES; k++) {
        set_engine_of(TSEL, E[k].e); TSEL->engine = TSEL->eng_req;
        if (page_visible(pg) != E[k].on) { printf("m2k ui: FILTER page, engine %u: %d\n", E[k].e, page_visible(pg)); ok = 0; }
    }
    ok &= pg < NPAGES && PAGES[pg].fam == FAM_EDIT && PAGES[pg].id[0] == P_FMODE && PAGES[pg].id[1] == P_FCUT &&
          PAGES[pg].id[2] == P_FRES && PAGES[pg].id[3] == P_ED_FLT;
    bad += check("EDIT FILTER (MODE CUT RES ENV): the seven only (PULS too), not NOISE, LOFI, DRM or the retired PAR DYN", ok);

    ok = 1;
    uint32_t nf = 0, na = 0, a4mix = NPAGES;
    for (uint32_t i = 0; i < NPAGES; i++) {
        if (!PAGES[i].own) continue;
        nf += PAGES[i].own == ENGI_FMTONE; na += PAGES[i].own == ENGI_ANA4;
        if (PAGES[i].own == ENGI_ANA4 && !strcmp(PAGES[i].title, "MIX")) a4mix = i;
        static const uint8_t ALL[] = {ENGI_SAW, 11u, ENGI_FMTONE, ENGI_DUAL, ENGI_ANA4, 3u, ENGI_DRUM};
        for (uint32_t k = 0; k < NELEM(ALL); k++) {
            set_engine_of(TSEL, ALL[k]); TSEL->engine = TSEL->eng_req;
            if (page_visible(i) != (ALL[k] == PAGES[i].own)) { printf("m2k ui: page %s, engine %u: %d\n", PAGES[i].title, ALL[k], page_visible(i)); ok = 0; }
        }
        for (uint32_t j = 0; j < 4; j++)   /* each knob a described control of its engine */
            if (PAGES[i].id[j] != 0xFF && !mt_tdesc(PAGES[i].own, PAGES[i].id[j])) { printf("m2k ui: page %s knob %u undescribed\n", PAGES[i].title, j); ok = 0; }
    }
    bad += check("FMTONE's four pages and ANALOG4's five: on their engine only, each knob a control of it", ok && nf == 4 && na == 5);
    set_engine_of(TSEL, ENGI_ANA4); apply_preset_to(TSEL, 0); go_home(); frame();
    int16_t mix0 = TSEL->p[P_FM1_LEVEL];
    ui.home = 0; ui.page = (uint8_t)a4mix; ui.force = 1; frame();
    turn(EN_K1, mix0 > 60 ? -5 : 5); frame();
    printf("m2k ui: ANALOG4 MIX knob 1: %d -> %d\n", mix0, TSEL->p[P_FM1_LEVEL]);
    bad += check("ANALOG4 MIX: KNOB 1 turns the control kept in its operator slot", a4mix < NPAGES && TSEL->p[P_FM1_LEVEL] != mix0);

    set_engine_of(TSEL, ENGI_SAW); apply_preset_to(TSEL, 0); go_home(); frame();
    ui.home = 0; ui.page = (uint8_t)pg; ui.force = 1; frame();
    turn(EN_K1, 1); turn(EN_K2, -10); turn(EN_K3, 20); frame();
    printf("m2k ui: FILTER after the knobs: MODE %d CUT %d RES %d\n", TSEL->p[P_FMODE], TSEL->p[P_FCUT], TSEL->p[P_FRES]);
    bad += check("its knobs: KNOB 1 MODE OFF -> LP, KNOB 2 CUT down from 127, KNOB 3 RES up from 0",
                 TSEL->p[P_FMODE] == 1 && TSEL->p[P_FCUT] < 127 && TSEL->p[P_FRES] > 0);
    {   /* MIDI CC 20..23: the channel's track's Stage knobs, on Mini-Tone's three (FMTONE's two in the operator slots) */
        uint8_t keep = settings_midi_knob_mode;
        settings_midi_knob_mode = MK_ABSOLUTE;
        static const uint8_t MT[] = {ENGI_FMTONE, ENGI_DUAL, ENGI_ANA4};
        ok = 1;
        for (uint32_t j = 0; j < NELEM(MT); j++) {
            set_engine_of(TSEL, MT[j]); apply_preset_to(TSEL, 0); go_home(); frame();
            for (uint32_t k = 0; k < 4u; k++) {
                uint32_t id = ENGINES[MT[j]]->knob[k];
                const param_desc_t *d = param_desc_of(MT[j], id);
                midi_fixed_apply(k, song.sel, 0, 0, 0); int lo = TSEL->p[id];
                midi_fixed_apply(k, song.sel, 127, 0, 0); int hi = TSEL->p[id];
                if (lo != d->min || hi != d->max) { printf("m2k ui: %s CC %u (%s): %d..%d, want %d..%d\n", ENGINES[MT[j]]->name, 20u + k, d->label, lo, hi, d->min, d->max); ok = 0; }
            }
        }
        settings_midi_knob_mode = keep;
        bad += check("CC 20..23 on FMTONE, DUALVCO, ANALOG4: each Stage knob over its whole range (FMTONE's LEV A / B too)", ok);
    }
    printf(bad ? "m2k ui: FAILED\n" : "m2k ui: the FILTER page and FMTONE / ANALOG4's own pages: OK\n");
    return bad != 0;
}
