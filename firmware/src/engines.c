/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol) and the parts'
 * sounds at power-on. */
#include "dsp.c"
#include "audio_batch.h"
#include "eng_prophet_test.c"    /* (without MELODEE_PROPHET: its patches alone, the tracks silent) */
#include "eng_phase.c"
#include "eng_cz.c"
#include "eng_sid.c"
#if MELODEE_FELUCCA_ENGINES
#include "eng_fphase.c"         /* PHASE: Felucca's, with FOLD (eng_phase.c is the CZ-1's core) */
#include "eng_lofi.c"           /* LOFI: Felucca's chip voice */
#endif
#if MELODEE_LEGACY_EXTRAS || MELODEE_FELUCCA_ENGINES
#include "eng_formant.c"
#endif
#include "eng_trio.c"
#include "eng_wheel.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_drum.c"           /* DRUM: synthesized 808 circuits (drum_808.c), the 909, a C64's SID (drum_sid.c), CR78, FM */
#if MELODEE_LEGACY_EXTRAS
#include "eng_noise.c"
#endif
#include "eng_fm6.c"            /* FM6: 6-operator FM rendered as Dexed renders it (fm6_core.c) */
#include "eng_midi.c"
#if MELODEE_SLICE
#include "eng_slice.c"
#else
static void slice_gone_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void slice_gone_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
static const engine_t ENG_RETIRED = {         /* 13 without MELODEE_SLICE: reserved, never offered (eng_ok) */
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {{"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}},
    .note_on = slice_gone_note_on,
    .render = slice_gone_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};
#endif

/* Each track owns only its selected engine's working state. Simple engines
 * use voice_t alone. State is released after the old engine's switch fade. */
#include "resources.c"
#include "eng_m2k.c"          /* SAW ENS STAT PAR DYN WAV: MONO2K's machines, a copy a voice */
static uint32_t eng_state_size(uint32_t e)
{
    switch (e) {
#if MELODEE_PROPHET
    case ENGI_PROPHET: return sizeof(p5_part_t);
#if MELODEE_PROPHET_PROTOTYPE
    case 0: return sizeof(p5_part_t);
#endif
#endif
#if MELODEE_LEGACY_EXTRAS || MELODEE_FELUCCA_ENGINES
    case ENGI_PHYS: return sizeof(phys_slot_t) * PHYS_POLY;
#endif
#if MELODEE_LEGACY_EXTRAS || MELODEE_WHEEL
    case 7: return sizeof(drw_part_t);              /* WHEEL (eng_wheel.c) */
#endif
#if MELODEE_LEGACY_EXTRAS && !MELODEE_FELUCCA_ENGINES
    case 2:                                         /* (PHASE: CZ-1's core) */
#endif
#if MELODEE_CZ
    case ENGI_CZ: return sizeof(cz_part_t);
#endif
#if MELODEE_SID
    case 3: return sizeof(sid_part_t);              /* SID (eng_sid.c) */
#endif                                              /* (Felucca's PHASE, LOFI, VOICE and TRIO: voice_t alone) */
    case ENGI_DRUM: return sizeof(drum909_part_t);
#if !MELODEE_M2K_ENGINES                            /* (retired with MONO2K's six: no part state) */
    case ENGI_FM6: return sizeof(fm6_part_t);
#endif
    case ENGI_SAW: case ENGI_SAW + 2u: case ENGI_SAW + 3u: case ENGI_SAW + 4u: case ENGI_WAV:
        return M2K_PART_BYTES(M2K_SAW);
    case ENGI_SAW + 1u: return M2K_PART_BYTES(M2K_ENS);
#if MELODEE_SLICE
    case ENGI_SLICE: return sizeof(slc_rb_t);
#endif
    default: return 0;
    }
}
static void eng_state_clear(uint32_t part)
{
    if (part < NPART) resource_release(RES_ENGINE0 + part);
}
static void eng_state_reset(void) { for (uint32_t k = 0; k < NPART; k++) eng_state_clear(k); }
static int eng_state_prepare(const track_t *t)
{
    uint32_t size = t->engine==ENGI_DRUM?drum_bytes(t):eng_state_size(t->engine);
    return !size || resource_get(RES_ENGINE0 + (uint32_t)(t - trk), size) != 0;
}
static phys_slot_t *phys_slots(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(phys_slot_t) * PHYS_POLY); }
static drum909_part_t *drum909_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(drum909_part_t)); }
static drum_lane_t *drum_kit_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(drum_lane_t) * DV_NLANE); }
static dsk_part_t *dsk_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(dsk_part_t)); }
static dlk_part_t *dlk_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(dlk_part_t)); }
static drw_part_t *drw_of(const track_t *t) { return resource_get(RES_ENGINE0 + (uint32_t)(t - trk), sizeof(drw_part_t)); }
static fm6_part_t *fm6_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(fm6_part_t)); }
static cz_part_t *cz_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(cz_part_t)); }
static sid_part_t *sid_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(sid_part_t)); }
#if MELODEE_PROPHET
static p5_part_t *p5_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(p5_part_t)); }
#endif
#if MELODEE_SLICE
static int16_t (*slc_rbuf(uint32_t part))[SLC_RB] { return resource_get(RES_ENGINE0 + part % NPART, sizeof(slc_rb_t)); }
#endif

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
#if MELODEE_PROPHET_PROTOTYPE
    &ENG_P5_TEST,                /* test firmware only: RAM patches, NEVER persist */
#else
    &ENG_RETIRED,                /* 0: retired ANALOG; stored ID reserved */
#endif
    &ENG_RETIRED,                /* 1: retired DIGITAL; stored ID reserved */
#if MELODEE_M2K_ENGINES
    &ENG_RETIRED,                /* 2: PHASE, retired again (MELODEE_M2K_ENGINES); stored ID reserved */
#elif MELODEE_FELUCCA_ENGINES
    &ENG_FPHASE,                 /* 2: PHASE, Felucca's (restored with its original number) */
#elif MELODEE_LEGACY_EXTRAS
    &ENG_PHASE,                  /* 2 */
#else
    &ENG_RETIRED,                /* 2: retired PHASE; stored ID reserved (its core: CZ-1's) */
#endif
#if MELODEE_SID
    &ENG_SID,                    /* 3: SID replaces LOFI without reusing a stored ID */
#else
    &ENG_LOFI,                   /* 3: LOFI, Felucca's (restored; SID's tracks while it shipped play as LOFI) */
#endif
    &ENG_RETIRED,                /* 4: retired SAMPLE; reserved to preserve stored indices */
#if MELODEE_LEGACY_EXTRAS || MELODEE_FELUCCA_ENGINES
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
#else
    &ENG_RETIRED,                /* 5: retired VOICE; stored ID reserved */
#endif
#if MELODEE_LEGACY_EXTRAS || MELODEE_FELUCCA_ENGINES
    &ENG_TRIO,
#else
    &ENG_RETIRED,                /* 6: retired TRIO; stored ID reserved */
#endif
#if MELODEE_LEGACY_EXTRAS || MELODEE_WHEEL
    &ENG_WHEEL,                  /* 7: WHEEL, Felucca's tonewheel organ (restored) */
#else
    &ENG_RETIRED,                /* 7: retired WHEEL; stored ID reserved */
#endif
    &ENG_RETIRED,                /* 8: retired GRAIN; stored index reserved */
#if MELODEE_LEGACY_EXTRAS || MELODEE_FELUCCA_ENGINES
    &ENG_PHYS,
#else
    &ENG_RETIRED,                /* 9: retired PHYS; stored ID reserved */
#endif
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
#if MELODEE_LEGACY_EXTRAS
    &ENG_NOISE,                  /* 11 */
#else
    &ENG_RETIRED,                /* 11: retired NOISE; stored ID reserved */
#endif
#if MELODEE_M2K_ENGINES
    &ENG_RETIRED,                /* 12: FM6, retired (MELODEE_M2K_ENGINES); stored ID reserved */
#else
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
#endif
#if MELODEE_SLICE
    &ENG_SLICE,                  /* 13 (ENGI_SLICE) */
#else
    &ENG_RETIRED,             /* 13: reserved (MELODEE_SLICE=0 builds without SLICE) */
#endif
    &ENG_RETIRED,                /* 14: retired OBXF; stored index reserved */
#if MELODEE_CZ
    &ENG_CZ,                     /* 15: native Casio CZ-1 */
#else
    &ENG_RETIRED,                /* 15: retired CZ-1; stored ID reserved */
#endif
    &ENG_RETIRED, &ENG_RETIRED, &ENG_RETIRED, /* 16..18 reserved */
    &ENG_P5_TEST,                /* 19: Prophet (ANALOG's ID 0 stays reserved) */
    &ENG_RETIRED,                /* 20: Prophet user-preset namespace */
    &ENG_MIDI,                   /* 21: USB MIDI Track */
    &ENG_M2K_SAW,                /* 22: SAW, MONO2K's SWAV-SAW */
    &ENG_M2K_ENS,                /* 23: ENS, MONO2K's SWAV-ENS */
    &ENG_M2K_STAT,               /* 24: STAT, MONO2K's FM+STAT */
    &ENG_M2K_PAR,                /* 25: PAR, MONO2K's FM+PAR */
    &ENG_M2K_DYN,                /* 26: DYN, MONO2K's FM+DYN */
    &ENG_M2K_WAV,                /* 27: WAV, MONO2K's DIGI-WAV */
};
static int m2k_kind(const track_t *t) { uint32_t e = t->engine; return e >= ENGI_SAW && e <= ENGI_WAV ? (int)(e - ENGI_SAW) : M2K_SAW; }

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* preset k (< npresets) of e: its row, or one made on demand (Prophet, CZ-1: their names live in the native data).
 * Made presets take turns in two slots: two can be held at once (main loop only) */
static const preset_t *eng_preset(const engine_t *e, uint32_t k)
{
    static preset_t made[2];
    static char made_name[2][21];
    static uint8_t turn;
    if (e->presets)
        return &e->presets[k];
    turn ^= 1u;
    memset(&made[turn], 0, sizeof made[turn]);
    made_name[turn][0] = 0;
    made[turn].name = made_name[turn];
#if MELODEE_PROPHET || MELODEE_CZ                 /* (the engines that make their presets) */
    e->preset_make(k, &made[turn], made_name[turn]);
#else
    (void)k;
#endif
    return &made[turn];
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the editor's list):
 * engine indices, never a retired one. The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
#if MELODEE_M2K_ENGINES
    ENGI_SAW, ENGI_SAW + 1, ENGI_SAW + 2, ENGI_SAW + 3, ENGI_SAW + 4, ENGI_WAV,   /* SAW ENS STAT PAR DYN WAV */
#else
    ENGI_PROPHET,                /* Prophet replaces ANALOG in new-sound browsing */
    12,                          /* FM6 */
#endif
#if MELODEE_FELUCCA_ENGINES
#if !MELODEE_M2K_ENGINES
    2,                           /* PHASE */
#endif
    3, 5, 6,                     /* LOFI VOICE TRIO */
#if MELODEE_WHEEL || MELODEE_LEGACY_EXTRAS
    7,                           /* WHEEL */
#endif
    9,                           /* PHYS */
#if MELODEE_LEGACY_EXTRAS
    11,                          /* NOISE */
#endif
#elif MELODEE_LEGACY_EXTRAS
    2, ENGI_CZ, 3, 5,             /* PHASE CZ-1 SID VOICE */
    6, 7, 9,
    11,                          /* NOISE */
#else
    ENGI_CZ, 3,                   /* CZ-1 SID */
#if MELODEE_WHEEL
    7,                           /* WHEEL */
#endif
#endif
#if MELODEE_SLICE
    13,                          /* SLICE */
#endif
    10,                          /* DRUM */
    ENGI_MIDI,
};

/* Retired synth records (ANALOG and DIGITAL since 2.0 too) retain their stored identity and render silence.
 * Never relabel retired records FM6. */
static int eng_extra_retired(uint32_t e)
{
#if !MELODEE_PROPHET_PROTOTYPE
    if (e == ENGI_ANALOG) return 1;
#endif
    if (!MELODEE_CZ && e == ENGI_CZ) return 1;
    if (MELODEE_M2K_ENGINES && (e == ENGI_PROPHET || e == ENGI_FM6 || e == 2u)) return 1;   /* their sounds: silent */
    if (MELODEE_FELUCCA_ENGINES && (e == 2u || e == 5u || e == 6u || e == 9u)) return 0;
    return e == ENGI_DIGITAL || (!MELODEE_LEGACY_EXTRAS && (e == 2u || e == 5u || e == 6u || (e == 7u && !MELODEE_WHEEL) || e == 9u || e == 11u));
}
/* the engines one can pick, in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e)
{
    return e < NENGINES && e != USER_NATIVE_P5 && !(e >= 16u && e <= 18u) && !eng_extra_retired(e) && e != 4u && e != 8u && e != 14u && (MELODEE_SLICE || e != ENGI_SLICE);
}
static uint32_t eng_sound_idx(uint32_t e)
{
    return eng_extra_retired(e) || eng_ok(e) ? e : MELODEE_M2K_ENGINES ? ENGI_SAW : ENGI_FM6;
}
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = MELODEE_M2K_ENGINES ? ENGI_SAW : ENGI_FM6;   /* (a retired or reserved engine: the first's place) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* the parts at power-on and in a new song (engine, preset; the steps empty): poly, bass, lead, drums. IT'S A
 * PROPHET 5, FM6 SOLID BASS, CZ-1 SYNTH.LEAD 1, 808 KIT (until 2026-10-16: PROPHET FUNK BASS II, FM6 PAD, SID INIT
 * SID, 808 KIT) */
#if MELODEE_M2K_ENGINES                          /* SAW SUPERSAW, STAT FM BASS, LOFI PULSE LD, 808 KIT */
static const uint8_t TRK_DEF[NPART][2] = {{ENGI_SAW, 0}, {ENGI_SAW + 2, 1}, {3, 0}, {ENGI_DRUM, 0}};
#elif MELODEE_CZ
static const uint8_t TRK_DEF[NPART][2] = {{ENGI_PROPHET, 1}, {ENGI_FM6, 10}, {ENGI_CZ, 52}, {ENGI_DRUM, 0}};
#else                                            /* (without the CZ-1: LOFI PULSE LD, Melodee 1.0's lead) */
static const uint8_t TRK_DEF[NPART][2] = {{ENGI_PROPHET, 1}, {ENGI_FM6, 10}, {3, 0}, {ENGI_DRUM, 0}};
#endif
/* track k's power-on engine and preset: TRK_DEF's, a MIDI-only track the MIDI Track's */
static uint32_t trk_def_engine(uint32_t k) { return k < NPART ? TRK_DEF[k][0] : ENGI_MIDI; }
static uint32_t trk_def_preset(uint32_t k) { return k < NPART ? TRK_DEF[k][1] : 0u; }
