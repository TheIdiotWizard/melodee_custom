/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
int main(void)
{
    int bad=0; reset(); up_boot();
    bad+=check("engine and native collection IDs remain unchanged",NENGINES==32 && ENGI_MIDI==21 && ENGI_SAW==22 && ENGI_WAV==27 && ENGI_NOIS==28 && ENGI_FMTONE==29 && ENGI_DUAL==30 && ENGI_ANA4==31 && !eng_ok(20) && USER_GENERAL==16 && USER_NATIVE_FM==17 && USER_NATIVE_CZ==18 && ENGI_PROPHET==19 && USER_NATIVE_P5==20);
#if MELODEE_FELUCCA_ENGINES
    /* Felucca's PHASE (2), LOFI (3), VOICE (5), TRIO (6) and PHYS (9) restored at their original numbers; CZ-1 (15)
     * retired (its stored tracks keep the number, play silent); SID's engine 3 plays as LOFI */
#if MELODEE_M2K_ENGINES
    /* Melodee Custom 1.2: PHASE (2), FM6 (12) and PROPHET (19) retired too; MONO2K's engines at 22..28.
     * 1.2.6: VOICE (5), TRIO (6) and MONO2K's STAT PAR DYN (24..26) retired; Mini-Tone's FMTONE DUALVCO ANALOG4 at 29..31.
     * 1.3.6: PULS in STAT's number (24) */
    const uint8_t retired[]={2,6,12,15,19,25,26,28};   /* (1.2.8: NOIS 28 retired, NOISE 11 back; 1.3.5: VOX in 5) */
#else
    const uint8_t retired[]={11,15};
#endif
    {
        static const struct { uint8_t e, n; const char *name; } back[] = {
#if MELODEE_M2K_ENGINES
            {22, 5, "SAW"}, {23, 5, "ENS"}, {24, 5, "PULS"}, {27, 5, "WAV"}, {11, 4, "NOISE"},
            {29, 14, "FMTONE"}, {30, 6, "DUALVCO"}, {31, 5, "ANALOG4"}, {5, 7, "VOX"},
#else
            {2, 6, "PHASE"}, {5, 4, "VOICE"}, {6, 5, "TRIO"},
#endif
            {3, 5, "LOFI"}, {7, 5, "WHEEL"}, {9, 8, "PHYS"}};
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
        bad+=check("PHYS and VOX (5, shipping) keep their part state; PHASE, LOFI, VOICE and TRIO need none",
                   eng_state_size(9)==sizeof(phys_slot_t)*PHYS_POLY && !eng_state_size(2) && !eng_state_size(3) && (MELODEE_M2K_ENGINES ? eng_state_size(5)==sizeof(vox_part_t) : !eng_state_size(5)) && !eng_state_size(6));
        bad+=check("track 3 powers on with LOFI PULSE LD", trk_def_engine(2)==3u && trk_def_preset(2)==0u);
#if MELODEE_M2K_ENGINES
        bad+=check("tracks 1 and 2 power on with SAW SUPERSAW and FMTONE DXBASS (FM BASS's place)",
                   trk_def_engine(0)==ENGI_SAW && trk_def_preset(0)==0u && trk_def_engine(1)==ENGI_FMTONE && trk_def_preset(1)==2u);
        bad+=check("the seven keep their voices as part state; ENS also its chorus",
                   eng_state_size(22)>0 && eng_state_size(23)>eng_state_size(22) && eng_state_size(27)==eng_state_size(22) && eng_state_size(ENGI_PULS)==eng_state_size(22) &&
                   eng_state_size(ENGI_FMTONE)>0 && eng_state_size(ENGI_DUAL)>0 && eng_state_size(ENGI_ANA4)>0);
        {   /* SAVE > USER with each of MONO2K's seven: the slot holds it, LOAD brings it back */
            int ok=1;
            static const uint8_t m2k7[]={ENGI_SAW,ENGI_SAW+1u,ENGI_WAV,11u,ENGI_FMTONE,ENGI_DUAL,ENGI_ANA4};
            for(uint32_t j=0;j<NELEM(m2k7);j++){
                uint32_t e=m2k7[j], k=UP_SLOTS-1u-j;
                set_engine_of(TSEL,(uint8_t)e);apply_preset_to(TSEL,1);
                for(uint32_t i=0;i<3;i++){const param_desc_t *d=param_desc_of(e,P_E0+i);   /* (within each control's range) */
                    TSEL->p[P_E0+i]=(int16_t)(d->min+(int32_t)(5u+i+j)%(d->max-d->min+1));}
                TSEL->p[P_FMODE]=2;TSEL->p[P_FCUT]=(int16_t)(33+j);TSEL->p[P_FRES]=44;   /* (their FILTER too) */
                if(e==ENGI_FMTONE||e==ENGI_ANA4)TSEL->p[P_FM2_DEC]=(int16_t)(61+j);   /* an operator-slot control */
                int16_t want[8];memcpy(want,TSEL->p+P_E0,sizeof want);int16_t want2=TSEL->p[P_FM2_DEC];
                int st=up_store(k,"M2K USER");
                int used=up_used(k), eng=up_engine(k)==e;
                set_engine_of(TSEL,3u);apply_preset_to(TSEL,0);
                int ld=up_load(k);
                if(st||!used||!eng||ld||TSEL->eng_req!=e||memcmp(TSEL->p+P_E0,want,sizeof want)||
                   TSEL->p[P_FMODE]!=2||TSEL->p[P_FCUT]!=(int16_t)(33+j)||TSEL->p[P_FRES]!=44||TSEL->p[P_FM2_DEC]!=want2){
                    printf("persist: MONO2K user preset, engine %u: store %d used %d engine %d load %d now %u fm2dec %d/%d e0 %d/%d e1 %d/%d e2 %d/%d\n",e,st,used,eng,ld,TSEL->eng_req,TSEL->p[P_FM2_DEC],want2,TSEL->p[P_E0],want[0],TSEL->p[P_E1],want[1],TSEL->p[P_E2],want[2]);ok=0;}
                up_put(k,0);
            }
            bad+=check("SAVE > USER keeps a sound of each of the seven, its FILTER and extra controls too; LOAD brings it back",ok);
            memset(&favorites,0,sizeof favorites);
            favorite_set(11u,1,1);favorite_set(11u,3,1);favorite_set(ENGI_SAW,1,1);favorite_set(ENGI_FMTONE,7,1);
            bad+=check("NOISE's stars (1.2.8: row 11's byte 7, the row MONO2K's and Mini-Tone's share) apart from theirs",
                       favorite_has(11u,1)&&favorite_has(11u,3)&&!favorite_has(11u,0)&&!favorite_has(11u,2)&&favorite_has(ENGI_SAW,1)&&
                       !favorite_has(ENGI_SAW,3)&&favorite_has(ENGI_FMTONE,7)&&favorites.factory[11][7]==0x0Au&&favorites.factory[11][0]==0x02u&&
                       (favorite_set(11u,1,0),!favorite_has(11u,1)&&favorite_has(ENGI_SAW,1)));
            set_engine_of(TSEL,ENGI_SAW);apply_preset_to(TSEL,0);
            TSEL->p[P_FMODE]=3;TSEL->p[P_FCUT]=21;TSEL->p[P_FRES]=99;
            project_capture(&proj_scratch);
            static project_t fsaved;fsaved=proj_scratch;
            TSEL->p[P_FMODE]=0;TSEL->p[P_FCUT]=127;TSEL->p[P_FRES]=0;
            bad+=check("a project keeps a MONO2K track's FILTER (MODE CUT RES)",
                       !project_restore_runtime(&fsaved) && TSEL->p[P_FMODE]==3 && TSEL->p[P_FCUT]==21 && TSEL->p[P_FRES]==99);
            set_engine_of(TSEL,ENGI_FMTONE);apply_preset_to(TSEL,6);
            bad+=check("a preset sets its FILTER: FMTONE RESO PLUCK a low-pass, its preset 0 OFF; ANALOG4 ACID resonant",
                       TSEL->p[P_FMODE]==1 && TSEL->p[P_FCUT]==60 && TSEL->p[P_FRES]==90 && (apply_preset_to(TSEL,0),TSEL->p[P_FMODE]==0) &&
                       (set_engine_of(TSEL,ENGI_ANA4),apply_preset_to(TSEL,3),TSEL->p[P_FMODE]==1 && TSEL->p[P_FCUT]==95 && TSEL->p[P_FRES]==110));
        }
#endif
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
    for(uint32_t k=0;k<NENG_SHOWN;k++)bad+=check("no shown engine takes a preset namespace (16..18, 20)",
        eng_vis(k)!=USER_GENERAL&&eng_vis(k)!=USER_NATIVE_FM&&eng_vis(k)!=USER_NATIVE_CZ&&eng_vis(k)!=USER_NATIVE_P5);
    {   /* (1.3.4: VOX as engine 16 read as user slots: PRESETS skipped about and went back and forth to other sounds) */
        uint32_t total,n,ok=1,src,k;
        list_set(LM_ALL);preset_all_pos(&total);
        for(n=0;n<total;n++){
            src=preset_all_at(n,&k);
            if(src==USER_GENERAL)break;                 /* (the user slots: none stored here) */
            preset_go(n);
            uint32_t at=preset_all_pos(&total);
            if(at!=n||TSEL->eng_req!=src||TSEL->preset!=k){printf("  entry %u (%s %u) came back as %u\n",n,ENGINES[src%NENGINES]->name,k,at);ok=0;}
        }
        bad+=check("PRESETS: every factory entry loads its own sound and the browser finds it at the same place",ok&&n>0);
#if MELODEE_M2K_ENGINES
        static const uint8_t order[]={ENGI_FMTONE,ENGI_ANA4,ENGI_SAW,ENGI_PULS,ENGI_SAW+1u,3,ENGI_DUAL,ENGI_WAV,7,ENGI_VOX,9,11,ENGI_DRUM,ENGI_MIDI};
        bad+=check("1.3.6 order: FMTONE ANALOG4 SAW PULS ENS LOFI DUALVCO WAV WHEEL VOX PHYS NOISE DRM, MIDI",
                   NENG_SHOWN==NELEM(order)&&!memcmp(ENGINE_ORDER,order,sizeof order));
        set_engine_of(TSEL,ENGI_SAW);project_capture(&proj_scratch);proj_scratch.t[song.sel].engine=16;proj_scratch.t[song.sel].preset=2;proj_scratch.sum=proj_sum(&proj_scratch);
        bad+=check("a 1.3.4 project's VOX (engine 16) loads as VOX",!project_restore_runtime(&proj_scratch)&&TSEL->eng_req==ENGI_VOX&&TSEL->preset==2);
#endif
    }
    {   /* DRM (1.3.6): a project's DRUM kit of 1.3.5 (P_DRMFMT 0, KIT FM) loads as the MD EFM kit */
        set_engine_of(TSEL,ENGI_DRUM);TSEL->p[P_DRMFMT]=0;TSEL->p[P_E0]=8;
        project_capture(&proj_scratch);static project_t dsaved;dsaved=proj_scratch;
        bad+=check("a project's 1.3.5 FM drum kit loads as DRM's MD EFM",!project_restore_runtime(&dsaved)&&
                   TSEL->eng_req==ENGI_DRUM&&TSEL->p[P_DRMFMT]==1&&TSEL->preset==DRMK_EFM&&drm_model(drum_patch_of(song.sel),0)==DM_BDEFM);
    }
    printf("Engine retirement test %s\n",bad?"FAILED":"passed");return !!bad;
}
