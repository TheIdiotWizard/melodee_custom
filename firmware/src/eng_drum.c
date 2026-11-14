/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 DEADACTIVE (Hortator: the drum models in drm/) */
/* DRM (engine 10, DRUM's number): Hortator's drum models on eight lanes, the whole kit on the part's one voice.
 * A lane (KICK SNARE CLAP HATCL HATOP TOM RIM BELL, the grid's and the GM map's) plays one model at a time: its next
 * hit takes it over, the old one's last sample easing out (~1 ms) so a retrigger never clicks (SKIN struck while its
 * head rings goes on ringing). A closed hat fades the open one out, a recorded hit's length ends its drum (2.9 ms).
 * The kit lives in the part's drum_patch_t as it always was stored (16 x 4 signed bytes, 54 bytes on the wire): lane
 * l in sounds 2l and 2l + 1, its model and seven of the model's knobs (drm_get); its LEVEL is the track's P_LN0 + l.
 * P_DRMFMT 1 marks a DRM kit; 0 is a DRUM kit of 1.3.5 or before (808 909 SID CR78 FM), drm_upgrade turns it into the
 * nearest DRM kit with its tunings. The track's own knobs: TUNE (every lane), DECY (every lane's DECAY), ACC (how much
 * velocity matters), DRV (into the soft clip). Models with eight knobs keep seven: DRM_DROP's is fixed at its default.
 * The models (drm/, from Hortator: Plaits ports, MIT; the TRX and EFM models measured from a classic digital drum
 * machine, GPL) read their knobs from the lane (drm_lane_t: k, nt) as Hortator's read a track's. */
enum { DV_KICK, DV_SNARE, DV_CLAP, DV_HATC, DV_HATO, DV_TOM, DV_RIM, DV_BELL, DV_NLANE };
enum { DVT_PUNCH, DVT_ROUND, DVT_SNARE, DVT_CLAP, DVT_HATC, DVT_HATO, DVT_TOM, DVT_CONGA,
       DVT_RIM, DVT_CLAVE, DVT_BELL, DVT_CYM, DVT_COUNT };
static const uint8_t DV_TYPE_LANE[DVT_COUNT] = {
    DV_KICK, DV_KICK, DV_SNARE, DV_CLAP, DV_HATC, DV_HATO, DV_TOM, DV_TOM, DV_RIM, DV_RIM, DV_BELL, DV_BELL,
};

/* a recorded hit's own pitch (semitones) and length (samples, 0: its drum's own), set around its note-on
 * (recording.c, seq.c) */
static int32_t drum_event_pitch;
static uint32_t drum_event_length;

/* ---------------------------------------------------------------- the models --- */
typedef struct {                 /* SKIN's head (drm/dm_memb.c): it rings on through a restrike */
    px_modal_t m;
    px_modal_blk_t k;
    int32_t last, gain;          /* the last sample out; the velocity gain now */
} memb_body_t;
typedef struct drm_lane {        /* what a model reads (Hortator's track): */
    int16_t k[8];                /* its knobs, P_E0.. as Hortator's (TUNE in semitones, the rest 0..127 or a list) */
    int8_t nt;                   /* the hit's semitones from the lane's note (the GM map, a recorded hit's PITCH) */
    uint8_t ring;                /* SKIN: struck while ringing (the lane eases nothing out) */
} drm_lane_t;
#include "drm/dm_state.h"
#define PD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}   /* (as params.c's, which comes later) */
#define PE(l, n, df) {l, F_ENUM, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df, n, 0}
typedef struct {
    const char *name;            /* <= 5 characters */
    uint8_t voices, choke;       /* (Hortator's; a DRM lane is one voice, the hats choke by lane) */
    param_desc_t edit[8];        /* the knobs: TUNE DECAY + its own */
    void (*trigger)(drm_lane_t *t, dvoice_t *v);
    void (*render)(drm_lane_t *t, dvoice_t *v, int32_t *out, uint32_t n);
    uint8_t weight;
} dmodel_t;
#include "drm/dm_dsp.c"
#include "drm/dm_kick_plaits.c"   /* BOOM PUNCH */
#include "drm/dm_snare_plaits.c"  /* SNAP CRACK */
#include "drm/dm_metal_plaits.c"  /* METAL HISS */
#include "drm/dm_memb.c"          /* SKIN: a struck drum head (PHYS's modal core) */
#include "drm/dm_fm.c"            /* FM: 2-operator FM percussion */
#include "drm/dm_ktrx.c"          /* BDTRX */
#include "drm/dm_sdtrx.c"         /* SDTRX */
#include "drm/dm_xttrx.c"         /* TMTRX CGTRX */
#include "drm/dm_trxn.c"          /* CPTRX SHTRX */
#include "drm/dm_rstrx.c"         /* RSTRX */
#include "drm/dm_trxm.c"          /* CHTRX OHTRX CYTRX */
#include "drm/dm_trxcb.c"         /* CBTRX */
#include "drm/dm_cltrx.c"         /* CLTRX */
#include "drm/dm_b2trx.c"         /* B2TRX */
#include "drm/dm_bdefm.c"         /* BDEFM TMEFM */
#include "drm/dm_efmn.c"          /* SDEFM CPEFM RSEFM */
#include "drm/dm_efmm.c"          /* CBEFM HHEFM CYEFM */

/* the model numbers a kit stores: never reorder, append only. By role: kicks, snares, claps, rims, hats, cymbals,
 * toms, bells and percussion */
enum { DM_BDTRX, DM_B2TRX, DM_BDEFM, DM_BOOM, DM_PUNCH, DM_SDTRX, DM_SDEFM, DM_SNAP, DM_CRACK, DM_CPTRX, DM_CPEFM,
       DM_RSTRX, DM_RSEFM, DM_CHTRX, DM_OHTRX, DM_HHEFM, DM_METAL, DM_HISS, DM_SHTRX, DM_CYTRX, DM_CYEFM, DM_TMTRX,
       DM_TMEFM, DM_CGTRX, DM_SKIN, DM_CBTRX, DM_CBEFM, DM_CLTRX, DM_FM, DM_N };
static const dmodel_t DMODELS[DM_N] = {
    DM_KTRX_DEF, DM_B2TRX_DEF, DM_BDEFM_DEF, DM_KBOOM_DEF, DM_KPUNC_DEF, DM_SDTRX_DEF, DM_SDEFM_DEF, DM_SSNAP_DEF,
    DM_SCRAK_DEF, DM_CPTRX_DEF, DM_CPEFM_DEF, DM_RSTRX_DEF, DM_RSEFM_DEF, DM_CHTRX_DEF, DM_OHTRX_DEF, DM_HHEFM_DEF,
    DM_HMETL_DEF, DM_HNOIS_DEF, DM_MATRX_DEF, DM_CYTRX_DEF, DM_CYEFM_DEF, DM_XTTRX_DEF, DM_XTEFM_DEF, DM_XCTRX_DEF,
    DM_MEMB_DEF, DM_CBTRX_DEF, DM_CBEFM_DEF, DM_CLTRX_DEF, DM_FM_DEF,
};
/* the knob a kit does not store (seven fit), fixed at its default: 7 is the eighth, "-" on most models */
static const uint8_t DRM_DROP[DM_N] = {
    [DM_BDTRX] = 6 /* SUB */, [DM_B2TRX] = 3 /* HOLD */, [DM_BDEFM] = 3 /* RDEC */, [DM_SHTRX] = 3 /* REV */,
    [DM_HHEFM] = 6 /* MDEC */, [DM_SDEFM] = 6 /* MDEC */, [DM_CPEFM] = 6 /* MDEC */,
    [DM_BOOM] = 7, [DM_PUNCH] = 7, [DM_SDTRX] = 7, [DM_SNAP] = 7, [DM_CRACK] = 7, [DM_CPTRX] = 7, [DM_RSTRX] = 7,
    [DM_RSEFM] = 7, [DM_CHTRX] = 7, [DM_OHTRX] = 7, [DM_METAL] = 7, [DM_HISS] = 7, [DM_CYTRX] = 7, [DM_CYEFM] = 7,
    [DM_TMTRX] = 7, [DM_TMEFM] = 7, [DM_CGTRX] = 7, [DM_SKIN] = 7, [DM_CBTRX] = 7, [DM_CBEFM] = 7, [DM_CLTRX] = 7,
    [DM_FM] = 7,
};
/* each model's makeup gain (Q12): at its defaults, velocity 100 and LEVEL 127, a lane peaks where the 808's did
 * (kicks ~46000, the rest ~40000 out; tests/drm_test.c) */
static const uint32_t DRM_GAIN[DM_N] = {3839, 4284, 5764, 8729, 6209, 8483, 96233, 5890, 4972, 9199, 28744, 11541, 18164, 65535, 58978, 38532, 6412, 5606, 76354, 25552, 27279, 13626, 1700, 10928, 3829, 13316, 7349, 12772, 3049};
static const char *const N_DRM_MODEL[DM_N] = {
    "BDTRX", "B2TRX", "BDEFM", "BOOM", "PUNCH", "SDTRX", "SDEFM", "SNAP", "CRACK", "CPTRX", "CPEFM", "RSTRX", "RSEFM",
    "CHTRX", "OHTRX", "HHEFM", "METAL", "HISS", "SHTRX", "CYTRX", "CYEFM", "TMTRX", "TMEFM", "CGTRX", "SKIN", "CBTRX",
    "CBEFM", "CLTRX", "FM",
};
static const param_desc_t DRM_MODEL_DESC = {"MODEL", F_ENUM, 0, DM_N - 1, 0, N_DRM_MODEL, 0};

/* ------------------------------------------------------ the kit in drum_patch_t --- */
/* lane l: sounds 2l (TUNE, DECAY - 64, knob 2 - 64, knob 3) and 2l + 1 (model - 24, knob 4 - 64, knob 5 - 64, knob 6):
 * inside drum_patch_valid's and the wire's ranges (6-bit -24..39, 7-bit -64..63, 7-bit 0..127), so a DRM kit is
 * stored, sent and checked as a DRUM kit always was */
#define DRM_FMT 1
static uint32_t drm_model(const drum_patch_t *d, uint32_t l)
{
    int32_t m = d->c[2u * (l & 7u) + 1u][0] + 24;
    return m >= 0 && m < DM_N ? (uint32_t)m : 0u;
}
static int32_t drm_get(const drum_patch_t *d, uint32_t l, uint32_t j)   /* stored knob j (0..6) of lane l */
{
    const int8_t *a = d->c[2u * (l & 7u)], *b = d->c[2u * (l & 7u) + 1u];
    switch (j) {
    case 0: return a[0];
    case 1: return a[1] + 64;
    case 2: return a[2] + 64;
    case 3: return a[3];
    case 4: return b[1] + 64;
    case 5: return b[2] + 64;
    default: return b[3];
    }
}
static void drm_put(drum_patch_t *d, uint32_t l, uint32_t j, int32_t v)
{
    int8_t *a = d->c[2u * (l & 7u)], *b = d->c[2u * (l & 7u) + 1u];
    switch (j) {
    case 0: a[0] = (int8_t)clamp(v, -24, 24); break;
    case 1: a[1] = (int8_t)(clamp(v, 0, 127) - 64); break;
    case 2: a[2] = (int8_t)(clamp(v, 0, 127) - 64); break;
    case 3: a[3] = (int8_t)clamp(v, 0, 127); break;
    case 4: b[1] = (int8_t)(clamp(v, 0, 127) - 64); break;
    case 5: b[2] = (int8_t)(clamp(v, 0, 127) - 64); break;
    default: b[3] = (int8_t)clamp(v, 0, 127); break;
    }
}
/* the model's knob of stored knob j, and back (the dropped one has none: 7 past the last) */
static uint32_t drm_knob(uint32_t m, uint32_t j) { return j < DRM_DROP[m % DM_N] ? j : j + 1u; }
static const param_desc_t *drm_knob_desc(uint32_t m, uint32_t j)
{
    uint32_t i = drm_knob(m, j);
    return i < 8u ? &DMODELS[m % DM_N].edit[i] : 0;
}
/* a lane's model, every stored knob at the model's default */
static void drm_set_model(drum_patch_t *d, uint32_t l, uint32_t m)
{
    m %= DM_N;
    d->c[2u * (l & 7u) + 1u][0] = (int8_t)((int32_t)m - 24);
    for (uint32_t j = 0; j < 7u; j++) {
        const param_desc_t *e = drm_knob_desc(m, j);
        drm_put(d, l, j, e ? e->def : 0);
    }
}

/* ------------------------------------------------------------------ the kits --- */
/* {model, its seven stored knobs (drm_knob's order), LEVEL} for KICK SNARE CLAP HATCL HATOP TOM RIM BELL */
typedef struct { uint8_t m; int8_t k[7]; uint8_t lv; } drm_lane_def_t;
#include "drm/drm_kits.h"
#define DRM_NKITS (sizeof DRM_KITS / sizeof DRM_KITS[0])

static void drm_kit_put(track_t *t, uint32_t kit)
{
    drum_patch_t *d = drum_patch_of((uint32_t)(t - trk));
    kit %= DRM_NKITS;
    for (uint32_t l = 0; l < DV_NLANE; l++) {
        const drm_lane_def_t *L = &DRM_KITS[kit][l];
        d->c[2u * l + 1u][0] = (int8_t)((int32_t)L->m - 24);
        for (uint32_t j = 0; j < 7u; j++)
            drm_put(d, l, j, L->k[j]);
        t->p[P_LN0 + l] = L->lv;
    }
    t->p[P_DRMFMT] = DRM_FMT;
}

/* a DRUM kit of 1.3.5 or before (P_DRMFMT 0): the nearest DRM kit (808: SUB, 909: HOUSE, FM: MD EFM, SID and CR78:
 * LO-FI), each lane's TUNE and DECAY moved by its drum's own (the 16 sounds' KICK SNARE CLAP HAT C HAT O TOM M RIM,
 * BELL / RIDE), quieter as its LEVEL was; DECY as it was (P_E3 then), TUNE 0, ACC 100, DRV off (their numbers moved).
 * Its patterns, steps and lane levels stay. Main loop, the audio IRQ off (the kit is the audio's) */
static void drm_upgrade(track_t *t)
{
    static const uint8_t SOUND[DV_NLANE] = {0, 1, 6, 7, 8, 3, 5, 10};
    uint32_t k = (uint32_t)(t - trk);
    if (t->p[P_FMODE] > 2)                         /* (also any track's FILTER MODE of 1.3.7 or before, BP HP NOT M2K:
                                                    * HP, HP, LP, LP: eng_uflt.c uflt_mode) */
        t->p[P_FMODE] = (int16_t)(t->p[P_FMODE] == 3 ? 2 : 1);
    if (k >= NPART || t->eng_req != ENGI_DRUM || t->p[P_DRMFMT] == DRM_FMT)
        return;
    drum_patch_t old = *drum_patch_of(k);
    int32_t kit = t->p[P_E0], decy = t->p[P_E3];
    uint32_t to = kit == 5 ? DRMK_HOUSE : kit == 8 ? DRMK_EFM : kit == 6 || kit == 7 ? DRMK_LOFI : DRMK_SUB;
    int16_t lv[DV_NLANE];
    for (uint32_t l = 0; l < DV_NLANE; l++)
        lv[l] = t->p[P_LN0 + l];
    drm_kit_put(t, to);
    for (uint32_t l = 0; l < DV_NLANE; l++) {
        const int8_t *c = old.c[SOUND[l]];
        drum_patch_t *d = drum_patch_of(k);
        drm_put(d, l, 0, drm_get(d, l, 0) + c[0]);
        drm_put(d, l, 1, drm_get(d, l, 1) + c[1] / 2);
        t->p[P_LN0 + l] = (int16_t)clamp(t->p[P_LN0 + l] * clamp(lv[l], 0, 127) / 127 * (127 - clamp(c[3], 0, 127)) / 127,
                                          0, 127);
    }
    t->p[P_E0] = 0;
    t->p[P_E1] = (int16_t)clamp(decy, 0, 127);
    t->p[P_E2] = 100;
    t->p[P_E3] = 0;
    for (uint32_t i = 4; i < 8u; i++)
        t->p[P_E0 + i] = 0;
    t->preset = (uint8_t)to;
}
static void drm_upgrade_all(void)
{
    for (uint32_t k = 0; k < NPART; k++)
        drm_upgrade(&trk[k]);
}

/* ------------------------------------------------------------ the GM map --- */
/* General MIDI notes 35..81 -> the drum (DVT_*: its lane) and semitones from the lane's note */
static const int8_t DRUM_GM[47][2] = {
    {DVT_PUNCH, -2}, {DVT_PUNCH, 0}, {DVT_RIM, 0}, {DVT_SNARE, 0}, {DVT_CLAP, 0}, {DVT_SNARE, 2},     /* 35 */
    {DVT_TOM, -7}, {DVT_HATC, 0}, {DVT_TOM, -4}, {DVT_HATC, -2}, {DVT_TOM, 0}, {DVT_HATO, 0},         /* 41 */
    {DVT_TOM, 3}, {DVT_TOM, 5}, {DVT_CYM, 0}, {DVT_TOM, 8}, {DVT_CYM, -3}, {DVT_CYM, 2},              /* 47 */
    {DVT_BELL, 5}, {DVT_HATC, 5}, {DVT_CYM, 4}, {DVT_BELL, 0}, {DVT_CYM, 1}, {DVT_CLAVE, -12},        /* 53 */
    {DVT_CYM, -2}, {DVT_CONGA, 5}, {DVT_CONGA, 2}, {DVT_CONGA, 0}, {DVT_CONGA, 0}, {DVT_CONGA, -5},   /* 59 */
    {DVT_TOM, 10}, {DVT_TOM, 7}, {DVT_BELL, 7}, {DVT_BELL, 3}, {DVT_HATC, 3}, {DVT_HATC, 7},          /* 65 */
    {DVT_CLAVE, 7}, {DVT_CLAVE, 5}, {DVT_HATC, -4}, {DVT_HATC, -6}, {DVT_CLAVE, 0}, {DVT_CLAVE, -4},  /* 71 */
    {DVT_CLAVE, -7}, {DVT_CONGA, 7}, {DVT_CONGA, 3}, {DVT_BELL, 12}, {DVT_BELL, 12},                  /* 77 */
};
static uint32_t drum_gm(const int16_t *p, uint32_t note, int32_t *st)
{
    uint32_t n = note >= 35u && note <= 81u ? note : 36u + (note + 120u - 36u) % 12u;
    (void)p;
    *st = DRUM_GM[n - 35u][1];
    return (uint32_t)DRUM_GM[n - 35u][0];
}

/* ----------------------------------------------------- the grid's lanes --- */
/* A step's lane hits (step_t.hit / acc, the DRUM grid: SEQ > STEP on a DRUM track) play these GM notes, on any
 * engine: DRM strikes its lanes, a synth the pitches. Each is the designed pitch of its lane (st 0 in DRUM_GM) */
static const uint8_t DRUM_LANE_NOTE[NLANE] = {36, 38, 39, 42, 46, 45, 37, 56};

static uint32_t drum_lane(uint32_t note)            /* the lane a GM note strikes */
{
    uint32_t n = note >= 35u && note <= 81u ? note : 36u + (note + 120u - 36u) % 12u;
    return DV_TYPE_LANE[DRUM_GM[n - 35u][0]];
}
static const char *drum_lane_name(const track_t *t, uint32_t l)   /* (5 characters at most) */
{
    static const char *const N[NLANE] = {"KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL"};
    (void)t;
    return N[l & (NLANE - 1u)];
}
static const char *drum_lane_abbr(const track_t *t, uint32_t l)   /* .. in two letters (the grid's lane column) */
{
    static const char *const N[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    (void)t;
    return N[l & (NLANE - 1u)];
}

/* a "sound" (DRUM SOUND, DRUM HIT, the pads) is a lane: its model's name, its note */
static uint32_t drum_sound_count(const track_t *t) { (void)t; return DV_NLANE; }
static uint32_t drum_sound_note(const track_t *t, uint32_t sound) { (void)t; return DRUM_LANE_NOTE[sound & 7u]; }
static uint32_t drum_sound_of(const track_t *t, uint32_t note) { (void)t; return drum_lane(note); }
static const char *drum_sound_name(const track_t *t, uint32_t sound)
{
    return N_DRM_MODEL[drm_model(drum_patch_of((uint32_t)(t - trk)), sound)];
}
static const char *drum_sound_abbr(const track_t *t, uint32_t sound) { return drum_lane_abbr(t, sound); }
static uint32_t drum_sound_step(const track_t *t, uint32_t sound, int32_t delta)
{
    (void)t;
    return (uint32_t)clamp((int32_t)(sound & 7u) + delta, 0, DV_NLANE - 1);
}
/* MPC pads 01..16, left to right from the bottom row: the eight lanes, then the toms and congas at their GM pitches,
 * the crash and ride on BELL */
static uint32_t drum_mpc_note(const track_t *t, uint32_t pad)
{
    static const uint8_t PAD[16] = {36, 38, 39, 37, 42, 46, 56, 49, 41, 45, 50, 51, 64, 62, 63, 75};
    (void)t;
    return PAD[pad & 15u];
}

/* the lanes step s strikes: its hits, and its notes on their lanes (a NOTE step only) */
static uint32_t step_lanes(const step_t *s)
{
    uint32_t m = 0, i;
    if (s->time != ST_NOTE)
        return 0;
    for (i = 0; i < s->n && i < 4u; i++)
        m |= 1u << drum_lane(s->note[i]);
    return m | s->hit;
}
/* .. and which of them are accented */
static uint32_t step_accents(const step_t *s) { return s->flags & SF_ACCENT ? step_lanes(s) : s->hit ? s->acc & step_lanes(s) : 0u; }

/* the note step s strikes drum `sound` with (DRUM HIT): one of its notes, else its lane's hit; -1: none */
static int32_t step_sound_note(const track_t *t, const step_t *s, uint32_t sound)
{
    uint32_t i, l = sound & 7u;
    for (i = 0; i < s->n && i < 4u; i++)
        if (drum_sound_of(t, s->note[i]) == l)
            return s->note[i];
    return (s->hit >> l) & 1u ? DRUM_LANE_NOTE[l] : -1;
}

/* a step's notes that are a lane's note become that lane's hits (the same note, the same velocity: nothing
 * sounds different). Other notes (a low tom 41, a crash 49) stay notes, shown on their lane. A step accent
 * of a step left with hits only becomes the hits' accents. Pattern loads into a DRUM track, projects of
 * before the grid, the grid's edits */
static void step_to_grid(step_t *s)
{
    if (!s->hit) s->acc = 0;                       /* recorded synth gate is not a drum accent */
    uint32_t i, k = 0;
    if (s->time != ST_NOTE)
        return;
    for (i = 0; i < s->n && i < 4u; i++) {
        uint32_t l = drum_lane(s->note[i]);
        if (s->note[i] == DRUM_LANE_NOTE[l])
            s->hit |= (uint8_t)(1u << l);
        else
            s->note[k++] = s->note[i];
    }
    for (i = k; i < 4u; i++)
        s->note[i] = 0;
    s->n = (uint8_t)k;
    if (!k && (s->flags & SF_ACCENT)) {
        s->acc |= s->hit;
        s->flags &= (uint8_t)~SF_ACCENT;
    }
}

/* lanes hit since the UI last looked, a bit per lane and track: Stage flashes their names (ui_stage.c) */
static volatile uint8_t drum_flash[NTRK];

/* --------------------------------------------------------------- the lanes --- */
#define DRM_EASE 5                               /* a retrigger's step eases out: >> 5 a sample (0.7 ms) */
#define DRM_FADE 128                             /* a choked open hat, a recorded hit's end: samples of fade */
typedef struct {
    dvoice_t v;                                  /* the model's voice */
    drm_lane_t L;                                /* what it reads */
    int32_t g;                                   /* the lane's gain now (Q15; -1: from its first sample) */
    int32_t left, fade;                          /* a recorded hit's samples to its end; samples of fade left */
    int32_t last, ease;                          /* the lane's last sample; a retrigger's step, easing out */
    uint8_t m, on;                               /* the model playing; sounding */
} drm_ch_t;
typedef struct drm_part {
    drm_ch_t ch[DV_NLANE];
    uint32_t age;                                /* hits: the voices' ages and noise seeds */
    uint8_t ready;
} drm_part_t;
static drm_part_t *drm_part(uint32_t part);      /* engines.c: the part's runtime pool */
static uint32_t drum_bytes(const track_t *t) { (void)t; return sizeof(drm_part_t); }
static drm_part_t *drm_of(const track_t *t)
{
    drm_part_t *K = t >= &trk[0] && t < &trk[NPART] ? drm_part((uint32_t)(t - trk)) : 0;
    if (K && !K->ready) {
        __typeof__(&memset) volatile clear = memset;
        clear(K, 0, sizeof *K);
        K->ready = 1;
    }
    return K;
}

/* the knobs lane l's model reads now: its stored ones, the dropped one's default, TUNE and DECY on top */
static void drm_knobs(const track_t *t, const int16_t *p, uint32_t l, drm_lane_t *L, uint32_t m)
{
    const drum_patch_t *d = drum_patch_of((uint32_t)(t - trk));
    const dmodel_t *M = &DMODELS[m];
    for (uint32_t i = 0; i < 8u; i++)
        L->k[i] = M->edit[i].def;
    for (uint32_t j = 0; j < 7u; j++) {
        uint32_t i = drm_knob(m, j);
        if (i < 8u)
            L->k[i] = (int16_t)clamp(drm_get(d, l, j), M->edit[i].min, M->edit[i].max);
    }
    L->k[0] = (int16_t)clamp(L->k[0] + p[P_E0], -24, 24);
    L->k[1] = (int16_t)clamp(L->k[1] + p[P_E1] - 64, 0, 127);
}

/* a hit on note at velocity vel: its lane's model, the hit's velocity squeezed by ACC (0: every hit full) */
static void drm_hit(track_t *t, uint32_t note, uint32_t vel)
{
    drm_part_t *K = drm_of(t);
    int32_t st;
    if (!K || t->p[P_DRMFMT] != DRM_FMT)
        return;
    uint32_t lane = DV_TYPE_LANE[drum_gm(t->p, note, &st)], m = drm_model(drum_patch_of((uint32_t)(t - trk)), lane);
    drm_ch_t *c = &K->ch[lane];
    int32_t acc = clamp(t->p[P_E2], 0, 127), v = 127 - acc * (127 - (int32_t)(vel > 127u ? 127u : vel)) / 127;
    if (lane == DV_HATC && K->ch[DV_HATO].on && !K->ch[DV_HATO].fade)
        K->ch[DV_HATO].fade = DRM_FADE;
    c->L.nt = (int8_t)clamp(st + drum_event_pitch, -24, 24);
    c->L.ring = 0;
    drm_knobs(t, t->p, lane, &c->L, m);
    if (c->on && !c->fade)
        c->ease = c->last;                       /* the old drum's last sample eases out .. */
    if (m == DM_SKIN && c->m == DM_SKIN && c->on) {   /* .. SKIN's head rings on through the restrike */
        memb_body_t keep = c->v.ms.mb;
        memset(&c->v, 0, sizeof c->v);
        c->v.ms.mb = keep;
    } else {
        memset(&c->v, 0, sizeof c->v);
    }
    c->v.active = 1;
    c->v.vel = (uint8_t)clamp(v, 1, 127);
    c->v.age = ++K->age;
    c->v.rng = (int32_t)(K->age * 2654435761u) | 1;
    DMODELS[m].trigger(&c->L, &c->v);
    if (c->L.ring)
        c->ease = 0;                             /* .. unless SKIN's head rings on */
    c->g = -1;
    c->m = (uint8_t)m;
    c->fade = 0;
    c->left = (int32_t)drum_event_length;
    c->on = 1;
}

/* 0 once every lane has gone quiet (drum_amp, once per control tick) */
static int drm_tick(track_t *t)
{
    drm_part_t *K = drm_of(t);
    if (!K)
        return 0;
    for (uint32_t k = 0; k < DV_NLANE; k++)
        if (K->ch[k].on || K->ch[k].ease)
            return 1;
    return 0;
}

static int32_t drm_level(const int16_t *p, uint32_t lane)   /* LEVEL (P_LN): square law, Q15 */
{
    int32_t lv = clamp(p[P_LN0 + (lane & 7u)], 0, 127);
    return lv * lv * 32767 / (127 * 127);
}

/* the kit, one block: each lane's model at its level, the steps easing out, DRV, the output stage */
typedef struct { int32_t x[CTL]; } dr_scratch_t;
static dr_scratch_t dr_scratch;
static __attribute__((noinline)) void drm_render(track_t *t, int32_t *out, uint32_t n, const vmod_t *m,
                                                 const int16_t *p, dr_scratch_t *scr)
{
    drm_part_t *K = drm_of(t);
    int32_t y[CTL] = {0}, *x = scr->x, drv = p[P_E3], g = 0, mk = 0;
    uint32_t i, k;
    if (!K)
        return;
    for (k = 0; k < DV_NLANE; k++) {
        drm_ch_t *c = &K->ch[k];
        int32_t g0, g1, s = 0;
        if (!c->on && !c->ease)
            continue;
        for (i = 0; i < n; i++)
            x[i] = 0;
        if (c->on) {
            drm_knobs(t, p, k, &c->L, c->m);     /* (live: a knob turned while it rings) */
            DMODELS[c->m].render(&c->L, &c->v, x, n);
            if (!c->v.active)
                c->on = 0;
            if (c->on && c->left > 0 && (c->left -= (int32_t)n) <= 0 && !c->fade)
                c->fade = DRM_FADE;              /* a recorded hit's end */
        }
        g1 = (int32_t)(((int64_t)drm_level(p, k) * DRM_GAIN[c->m]) >> 12);
        g0 = c->g < 0 ? g1 : c->g;
        c->g = g1;
        for (i = 0; i < n; i++) {
            s = (int32_t)(((int64_t)x[i] * (g0 + (((g1 - g0) * (int32_t)i) >> CTL_LOG2))) >> 15);
            if (c->fade)
                s = c->fade > (int32_t)i ? s * (c->fade - (int32_t)i) / DRM_FADE : 0;
            s += c->ease;
            c->ease -= c->ease >> DRM_EASE;
            y[i] += s;
        }
        if (c->ease > -(1 << DRM_EASE) && c->ease < (1 << DRM_EASE))
            c->ease = 0;
        if (c->fade && (c->fade -= (int32_t)n) <= 0) {
            c->fade = 0;
            c->on = 0;
            c->v.active = 0;
        }
        c->last = c->on || c->ease ? s : 0;
    }
    if (drv > 0) {                               /* DRV: x1..x4 into the soft clip, the level kept (Q12) */
        g = 4096 + drv * 3 * 4096 / 127;
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * g) >> 12));
    }
    for (i = 0; i < n; i++) {                    /* x1.25 and the knee below */
        int32_t s = clamp(y[i], -131071, 131071);
        if (g)
            s = (softclip((s * g) >> 12) * mk) >> 15;
        out[i] += voice_amp(soft_knee(s + (s >> 2), 24000), m, i) << 1;
    }
}

/* ------------------------------------------------------------- the engine --- */
static void drum_note_on(track_t *t, voice_t *v)
{
    drum_flash[(uint32_t)(t - trk) % NTRK] |= (uint8_t)(1u << drum_lane(v->note));
    v->s[3] = (int32_t)drum_event_length;
    v->s[4] = drum_event_pitch;
    v->s[5] = 0;
    v->s[2] = 1;
    v->env_out = 32767;
    drm_hit(t, v->note, v->vel);
}
/* Different notes on a lane, and the lanes, share the kit's one voice */
static voice_t *drum_reuse(track_t *t, uint32_t note)
{
    (void)note;
    return t->v[0].active ? &t->v[0] : 0;
}
/* the voice's amplitude: on while any lane sounds (held at full; the hits' own levels are the lanes') */
static int32_t drum_amp(track_t *t, voice_t *v, int32_t adsr)
{
    (void)adsr;
    if (v->active && drm_tick(t)) {
        v->env = 1 << 24;
        return 32767 * 127 / (v->vel ? v->vel : 1);
    }
    v->active = v->gate = v->stage = 0;
    v->env = 0;
    return 0;
}
static void drum_render_into(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m, dr_scratch_t *scratch,
                             const int16_t *p)
{
    if (v == &t->v[0])
        drm_render(t, out, n > CTL ? CTL : n, m, p, scratch);
}
static void drum_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    drum_render_into(t, v, out, n, m, &dr_scratch, t->p);
}

/* the GM drum map, the first C (key 7) is the kick (C2, 36) */
static int32_t drum_keys(const track_t *t, uint32_t k)
{
    return clamp(29 + 12 * input_octave(t) + (int32_t)k, 0, 127);
}
static uint32_t drum_cap(const track_t *t) { (void)t; return 1u; }
/* the kit's one voice in the voice budget: 4 points, two synth voices (a beat rings two or three lanes at once; the
 * eight together cost about four to ten, the overload shedding's; four drum parts still fit the budget of 16; GLO >
 * WEIGHTS DRUM sets it) */
static uint32_t drum_units(const track_t *t) { (void)t; return 4u; }

static const engine_t ENG_DRUM = {
    .name = "DRM",
    .page_title = {"KIT", "-"},
    .edit = {
        {"TUNE", F_SEMI, -12, 12, 0, 0, 0},
        {"DECY", F_PCT, 0, 127, 64, 0, 0},
        {"ACC", F_PCT, 0, 127, 100, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = DRUM_PRESETS,
    .npresets = NELEM(DRUM_PRESETS),
    .note_on = drum_note_on,
    .render = drum_render,
    .amp = drum_amp,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = DV_NLANE,
    .cap = drum_cap,
    .units = drum_units,
    .oneshot = 1,
    .keys = drum_keys,
};
