/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
int main(void)
{
    int bad=0; reset(); up_boot();
    bad+=check("engine and native collection IDs remain unchanged",NENGINES==22 && ENGI_MIDI==21 && !eng_ok(20) && USER_GENERAL==16 && USER_NATIVE_FM==17 && USER_NATIVE_CZ==18 && ENGI_PROPHET==19 && USER_NATIVE_P5==20);
#if MELODEE_FELUCCA_ENGINES
    /* Felucca's PHASE (2), LOFI (3), VOICE (5), TRIO (6) and PHYS (9) restored at their original numbers; CZ-1 (15)
     * retired (its stored tracks keep the number, play silent); SID's engine 3 plays as LOFI */
    const uint8_t retired[]={11,15};
    {
        static const struct { uint8_t e, n; const char *name; } back[] = {
            {2, 6, "PHASE"}, {3, 5, "LOFI"}, {5, 4, "VOICE"}, {6, 5, "TRIO"}, {7, 5, "WHEEL"}, {9, 9, "PHYS"}};
        for (uint32_t j = 0; j < NELEM(back); j++) {
            uint32_t e = back[j].e, shown = 0;
            for (uint32_t k = 0; k < NENG_SHOWN; k++) shown |= eng_vis(k) == e;
            bad+=check("Felucca engine restored: selectable, listed, its presets and name",
                       eng_ok(e) && shown && ENGINES[e]->npresets == back[j].n && !strcmp(ENGINES[e]->name, back[j].name));
            set_engine_of(TSEL,e);TSEL->preset=(uint16_t)(back[j].n-1u);
            project_capture(&proj_scratch);
            static project_t fsaved;fsaved=proj_scratch;
            bad+=check("Felucca engine: a saved project restores the engine and its preset",
                       !project_restore_runtime(&fsaved) && TSEL->eng_req==e && TSEL->preset==back[j].n-1u);
        }
        bad+=check("PHYS keeps its part state; PHASE, LOFI, VOICE and TRIO need none",
                   eng_state_size(9)==sizeof(phys_slot_t)*PHYS_POLY && !eng_state_size(2) && !eng_state_size(3) && !eng_state_size(5) && !eng_state_size(6));
        bad+=check("track 3 powers on with LOFI PULSE LD", trk_def_engine(2)==3u && trk_def_preset(2)==0u);
    }
#elif MELODEE_WHEEL
    const uint8_t retired[]={2,5,6,9,11};
    /* WHEEL (7) is restored: selectable, its factory organs, its part state, and an old project's WHEEL track returns */
    bad+=check("WHEEL restored: engine 7 selectable with its presets and part state",
               eng_ok(7) && ENGINES[7]==&ENG_WHEEL && ENGINES[7]->npresets==5 && eng_state_size(7)==sizeof(drw_part_t));
    {
        set_engine_of(TSEL,7);TSEL->preset=2;
        project_capture(&proj_scratch);
        static project_t wsaved;wsaved=proj_scratch;
        bad+=check("WHEEL: a saved project restores engine 7 and its preset",
                   !project_restore_runtime(&wsaved) && TSEL->eng_req==7 && TSEL->preset==2);
    }
#else
    const uint8_t retired[]={2,5,6,7,9,11};
#endif
    for(uint32_t j=0;j<NELEM(retired);j++) {
        uint32_t e=retired[j];
        bad+=check("retired engine absent from browser and DSP resources",!eng_ok(e) && !ENGINES[e]->npresets && !eng_state_size(e));
        favorite_set(e,3,1);
        set_engine_of(TSEL,e);
        TSEL->preset=3;
        for(uint32_t k=0;k<8;k++)TSEL->p[P_E0+k]=(int16_t)(17+k);
        project_capture(&proj_scratch);
        static project_t saved;
        saved=proj_scratch;
        bad+=check("old project keeps retired identity, preset and edit values",!project_restore_runtime(&saved) && TSEL->eng_req==e && TSEL->preset==3 && !memcmp(TSEL->p+P_E0,saved.t[song.sel].p+P_E0,16));
        bad+=check("retired general user patch remains identified by its original engine",!up_store(j,"RETIRED") && up_engine(j)==e);
        int16_t values[P_COUNT];up_values(up_rec(j),values);
        bad+=check("retired user patch retains all native edit values",!memcmp(values+P_E0,TSEL->p+P_E0,16));
        bad+=check("retirement does not clear existing favorite identifiers",favorite_has(e,3));
    }
    for(uint32_t k=0;k<NENG_SHOWN;k++)bad+=check("browser lists only available engines",eng_ok(eng_vis(k)));
    printf("Engine retirement test %s\n",bad?"FAILED":"passed");return !!bad;
}
