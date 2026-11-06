/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* MONO2K's seven in the UI (the shipping build; tests/ui_test.c's stubs, not its own tests):
 *   build/host/m2k_ui_test
 * EDIT's FILTER page (MODE CUT RES ENV) is there for SAW ENS STAT PAR DYN WAV NOIS only, not LOFI, TRIO or DRUM; its
 * knobs turn its values; STAT's Stage knob 3 is the FILTER's CUT (TONE gone). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    int bad = 0, ok = 1;
    uint32_t pg = NPAGES;
    for (uint32_t i = 0; i < NPAGES; i++) if (!strcmp(PAGES[i].title, "FILTER")) pg = i;
    static const uint8_t E[] = {ENGI_SAW, ENGI_SAW + 1u, ENGI_SAW + 2u, ENGI_SAW + 3u, ENGI_SAW + 4u, ENGI_WAV, ENGI_NOIS,
                                3u, 6u, ENGI_DRUM};
    ui_power_on();
    for (uint32_t k = 0; k < NELEM(E) && pg < NPAGES; k++) {
        set_engine_of(TSEL, E[k]); TSEL->engine = TSEL->eng_req;
        if (page_visible(pg) != (E[k] >= ENGI_SAW)) { printf("m2k ui: FILTER page, engine %u: %d\n", E[k], page_visible(pg)); ok = 0; }
    }
    ok &= pg < NPAGES && PAGES[pg].fam == FAM_EDIT && PAGES[pg].id[0] == P_FMODE && PAGES[pg].id[1] == P_FCUT &&
          PAGES[pg].id[2] == P_FRES && PAGES[pg].id[3] == P_ED_FLT;
    bad += check("EDIT FILTER (MODE CUT RES ENV): MONO2K's seven only, not LOFI, TRIO or DRUM", ok);

    set_engine_of(TSEL, ENGI_SAW); apply_preset_to(TSEL, 0); go_home(); frame();
    ui.home = 0; ui.page = (uint8_t)pg; ui.force = 1; frame();
    turn(EN_K1, 1); turn(EN_K2, -10); turn(EN_K3, 20); frame();
    printf("m2k ui: FILTER after the knobs: MODE %d CUT %d RES %d\n", TSEL->p[P_FMODE], TSEL->p[P_FCUT], TSEL->p[P_FRES]);
    bad += check("its knobs: KNOB 1 MODE OFF -> LP, KNOB 2 CUT down from 127, KNOB 3 RES up from 0",
                 TSEL->p[P_FMODE] == 1 && TSEL->p[P_FCUT] < 127 && TSEL->p[P_FRES] > 0);
    bad += check("STAT and PAR: Stage knob 3 the FILTER's CUT (TONE gone); their E7 a blank slot",
                 ENGINES[ENGI_SAW + 2u]->knob[2] == P_FCUT && ENGINES[ENGI_SAW + 3u]->knob[2] == P_FCUT &&
                 ENGINES[ENGI_SAW + 2u]->edit[6].min == ENGINES[ENGI_SAW + 2u]->edit[6].max &&
                 ENGINES[ENGI_SAW + 3u]->edit[6].min == ENGINES[ENGI_SAW + 3u]->edit[6].max);
    printf(bad ? "m2k ui: FAILED\n" : "m2k ui: the FILTER page and STAT / PAR's knobs: OK\n");
    return bad != 0;
}
