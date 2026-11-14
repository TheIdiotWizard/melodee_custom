/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Stephane Schmitz (Mini-Tone: https://github.com/eldontyrellcorp/Mini-Tone)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FMTONE, DUALVCO and ANALOG4: Mini-Tone's engines (eng_fmtone.c, eng_dual.c, eng_ana4.c at 6803c6c), Melodee Custom's
 * since 1.2.6, engines 29..31. Their sound is Mini-Tone's, with these changes:
 *  - their filters are the FILTER page (eng_uflt.c, TRIO's filter, MODE OFF by default) instead of Mini-Tone's resonant
 *    filter (dsp.c rsvf): FMTONE's and DUALVCO's voice filter and ANALOG4's filter 1 (its CUT1 RES1 TYPE gone; its
 *    filter envelope F1 ENV and KEY move the page's cutoff). ANALOG4's filter 2 runs on the same filter (F2MD CUT2 RES2);
 *  - the track's ENV / LFO -> FLT move the FILTER's cutoff when it is on; with it OFF they move what they moved in
 *    Mini-Tone without a filter (FMTONE: its modulation levels);
 *  - each part's state is in its engine memory (resources.c resource_get), as MONO2K's; pitch through tuned_pitch_inc
 *    (the A4 tuning);
 *  - the presets keep Mini-Tone's sounds (their FX sends and pattern hints, which Melodee has not, go).
 * FMTONE and ANALOG4 keep their controls past EDIT 2 in the operator slots P_FM1_ATK .. P_FM4_LEVEL (PHASE's, retired):
 * engine_t has no tdesc here, so mt_tdesc names them (params.c track_desc, param_desc_of) and their pages show for their
 * engine only (params.c PAGES' own). */

/* ------------------------------------------------------------------------------------------------ part memory --- */
typedef struct { int32_t f[1][UFLT_ST], drift, nst; } mt_vflt_t;   /* a voice's FILTER: its states (M2K: all), the wander */
#define MT_M2K_FM 3u                  /* FILTER's M2K: FMTONE as MONO2K's FM+STAT, DUALVCO and ANALOG4 as SWAV-SAW (their DIST */
#define MT_M2K_VCO 5u                 /* ceilings) */
#ifndef MT_M2K_GAIN
#define MT_M2K_GAIN 5900              /* a voice's level a machine sample (Q12: FMTONE's carrier against FM+'s, half scale) */
#endif
#define MT_VFLT_SEED(t, k) (0x9E3779B9u ^ (uint32_t)((t) - trk) * 7919u ^ (uint32_t)(k) * 104729u)
static void *mt_mem(const track_t *t, uint32_t bytes)
{
    return t >= &trk[0] && t < &trk[NPART] ? resource_get(RES_ENGINE0 + (uint32_t)(t - trk), bytes) : 0;
}

/* ===================================================================================================== FMTONE === */
/* Four-operator FM in the manner of the FM TONE machine of Elektron's Digitone II (Mini-Tone's own implementation, no
 * Elektron code or data). Operators C (the carrier), A, and B1 / B2 (the B group, whose ratios are linked); A and B
 * have an envelope each (delay, attack, decay to an end level) and a level, and a key tracking; the track's ENV is C's
 * amplitude. EDIT 1 (OPS): ALGO 1..8, RAT C, RAT A, RAT B (B1; B2 an octave above, at most 16). EDIT 2 (TONE): HARM
 * (+ A's and B1's waves, - C's), DTN (A up, B2 down), FB (B2's feedback), MIX (the other carriers, algorithms 5..8).
 * A ENV / B ENV: ATK DEC END LEV; DEL TRK: DEL A, DEL B, TRK A, TRK B; FINE: FT A, FT B1, FT B2.
 * Algorithms: 1 B2>B1>A>C  2 (B1+B2)>A>C  3 B1>A>C, B2>C  4 B2>B1, (A+B1)>C  5 B2>B1>out, A>C>out  6 B2>C,A,B1 out
 * 7 B2>B1 out, A out, C out  8 all four out. Each voice's sum passes a DC blocker (~15 Hz). */
#define FMT_POLY 6
#define FMT_NRATIO 23
static const uint16_t FMT_RATIO_Q8[FMT_NRATIO] = {64, 128, 192, 256, 320, 384, 512, 640, 768, 896, 1024, 1280,
                                                  1536, 1792, 2048, 2304, 2560, 2816, 3072, 3328, 3584, 3840, 4096};
static const char *const N_FMT_RATIO[] = {"0.25", "0.5", "0.75", "1", "1.25", "1.5", "2", "2.5", "3", "3.5", "4", "5",
                                          "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};
static const char *const N_FMT_ALGO[] = {"1", "2", "3", "4", "5", "6", "7", "8"};
typedef struct {
    uint32_t ph[4];              /* C, A, B1, B2 */
    int32_t fb1, fb2;            /* B2's last two samples */
    int32_t env[2];              /* the envelopes of A and B, Q24 */
    int32_t ia, ib;              /* the modulation index of A and B at the end of the last block, Q8 */
    uint16_t del[2];             /* the delay ticks left */
    uint8_t st[2];               /* 0 delay, 1 attack, 2 decay / end */
    int32_t dx1, dy1;            /* the DC blocker's last input, and its output as Q8 */
    uint8_t init;                /* 0 until the first block: ia, ib start at its values (no ramp from 0) */
    mt_vflt_t flt;               /* the FILTER page */
} fmt_slot_t;
#define FMT_BYTES (sizeof(fmt_slot_t) * FMT_POLY)

/* the operator slots as FMTONE names them (A, B: ATK DEC END DEL LEV; TRK A / TRK B; FTO A / B1 / B2). P_FM3_SUS,
 * P_FM3_LEVEL and P_FM4_SUS .. P_FM4_LEVEL (Mini-Tone's voice filter: the FILTER page here) are unused */
static const param_desc_t FMT_TD[P_FM4_LEVEL - P_FM1_ATK + 1] = {
    [P_FM1_ATK - P_FM1_ATK] = {"ATK", F_TIME, 0, 127, 0, 0, 0},
    [P_FM1_DEC - P_FM1_ATK] = {"DEC", F_TIME, 0, 127, 0, 0, 0},
    [P_FM1_SUS - P_FM1_ATK] = {"END", F_PCT, 0, 127, 127, 0, 0},
    [P_FM1_REL - P_FM1_ATK] = {"DEL A", F_TIME, 0, 127, 0, 0, 0},
    [P_FM1_LEVEL - P_FM1_ATK] = {"LEV A", F_PCT, 0, 127, 127, 0, 0},
    [P_FM2_ATK - P_FM1_ATK] = {"ATK", F_TIME, 0, 127, 0, 0, 0},
    [P_FM2_DEC - P_FM1_ATK] = {"DEC", F_TIME, 0, 127, 0, 0, 0},
    [P_FM2_SUS - P_FM1_ATK] = {"END", F_PCT, 0, 127, 127, 0, 0},
    [P_FM2_REL - P_FM1_ATK] = {"DEL B", F_TIME, 0, 127, 0, 0, 0},
    [P_FM2_LEVEL - P_FM1_ATK] = {"LEV B", F_PCT, 0, 127, 127, 0, 0},
    [P_FM3_ATK - P_FM1_ATK] = {"TRK A", F_PCT, 0, 127, 0, 0, 0},
    [P_FM3_DEC - P_FM1_ATK] = {"TRK B", F_PCT, 0, 127, 0, 0, 0},
    [P_FM3_REL - P_FM1_ATK] = {"FT A", F_INT, -64, 63, 0, 0, "ct"},
    [P_FM4_ATK - P_FM1_ATK] = {"FT B1", F_INT, -64, 63, 0, 0, "ct"},
    [P_FM4_DEC - P_FM1_ATK] = {"FT B2", F_INT, -64, 63, 0, 0, "ct"},
};

static fmt_slot_t *fmt_slot_of(track_t *t, voice_t *v)
{
    uint32_t i = (uint32_t)(v - t->v);
    fmt_slot_t *S = i < FMT_POLY ? mt_mem(t, FMT_BYTES) : 0;
    return S ? S + i : 0;
}

/* an envelope's delay in control ticks (32 samples): the DEL time (F_TIME: 1 ms .. 10 s), 0 = none */
static uint32_t fmt_del_ticks(int32_t del)
{
    uint32_t k;
    if (del <= 0)
        return 0;
    k = (TIME_MS_X10[del > 127 ? 127 : del] * 141u) >> 10;       /* ms x 10 x 44.1 / 32 / 10 */
    return k ? (k > 65535u ? 65535u : k) : 1u;
}

static void fmt_note_on(track_t *t, voice_t *v)
{
    fmt_slot_t *S = fmt_slot_of(t, v);
    uint32_t g;
    if (!S)
        return;
    if (!(v->env || v->env_out)) {                    /* from silence: phase 0, a clean feedback and filter */
        S->ph[0] = S->ph[1] = S->ph[2] = S->ph[3] = 0;
        S->fb1 = S->fb2 = 0;
        S->dx1 = S->dy1 = 0;
        memset(S->flt.f, 0, sizeof S->flt.f);
    }
    for (g = 0; g < 2u; g++) {                        /* the envelopes (re)start: after their delay */
        uint32_t d = fmt_del_ticks(t->p[P_FM1_REL + g * 5u]);
        S->env[g] = 0;
        S->del[g] = (uint16_t)d;
        S->st[g] = d ? 0 : 1;
    }
    S->init = 0;
}

/* one envelope for one control tick (32 samples): delay, attack to 1.0, decay to the end level, held. pr: ATK DEC END
 * DEL of the group (P_FMg_ATK ..); returns the envelope, Q24 */
static int32_t fmt_env_tick(const int16_t *pr, int32_t *e, uint16_t *del, uint8_t *st)
{
    if (*st == 0) {
        if (*del > 1u) {
            --*del;
            *e = 0;
            return 0;
        }
        *del = 0;
        *st = 1;
    }
    if (*st == 1) {
        if (!pr[0]) {
            *e = 1 << 24;
            *st = 2;
        } else {
            *e += (int32_t)ENV_LIN[pr[0] & 127];
            if (*e >= (1 << 24)) {
                *e = 1 << 24;
                *st = 2;
            }
        }
    } else {
        int32_t tg = pr[2] >= 127 ? 1 << 24 : (int32_t)pr[2] << 17;
        if (!pr[1])
            *e = tg;
        else
            *e += mulq16(tg - *e, ENV_EXP[pr[1] & 127]);
    }
    *e = clamp(*e, 0, 1 << 24);
    return *e;
}

/* an operator's phase increment: the voice's (base) x ratio r (Q8) x ct cents; kept below Nyquist */
static inline uint32_t fmt_inc(uint32_t base, uint32_t r, int32_t ct)
{
    uint32_t b = base >> 8, inc;
    if (b <= 0x73000u) {                               /* (r <= 4096: below 0x73000000, no divide) */
        inc = b * r;
    } else {
        uint32_t lim = 0x73000000u / r;
        inc = b > lim ? 0x73000000u : b * r;
    }
    return inc + (uint32_t)((int32_t)(inc >> 12) * (ct * 2367 / 1000));
}

#define FMT_PM(x, i) ((uint32_t)((x) * (i)) * 8u)         /* phase modulation: x Q15, index i Q8 */
#define FMT_WT(x, w) ((((x) >> 1) * (w)) >> 14)            /* the other carriers' sum x w (Q15), halved first */
/* an operator's wave: a sine, or at HARM h (1..63) a self-modulated one, towards a saw-like spectrum */
static inline int32_t fmt_w(uint32_t ph, int32_t h)
{
    return h ? sine_i(ph + ((uint32_t)(sine_i(ph) * h) << 9)) : sine_i(ph);
}

static void fmt_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    fmt_slot_t *S = fmt_slot_of(t, v);
    static const uint8_t NOTH[8] = {0, 0, 0, 0, 1, 2, 2, 3};      /* carriers other than C, per algorithm */
    uint32_t alg = (uint32_t)p[P_E0] & 7u, i, rc, ra, rb1, rb2;
    int32_t harm = p[P_E4], hp = harm > 0 ? harm : 0, hn = harm < 0 ? (-harm > 63 ? 63 : -harm) : 0;
    int32_t dtn = p[P_E5], fbi = p[P_E6] << 8, w = p[P_E7] * 258, la, lb, sa, sb, ia1, ib1, ia, ib, dia, dib;
    int32_t pr, norm, fb1, fb2, adj, dx1, dy1;
    uint32_t pc, pa, p1, p2, ic, iaa, i1, i2, fon = p[P_FMODE] != 0;
    uflt_t rf;
    if (!S)
        return;
    if (n > CTL)
        n = CTL;
    rc = FMT_RATIO_Q8[clamp(p[P_E1], 0, FMT_NRATIO - 1)];
    ra = FMT_RATIO_Q8[clamp(p[P_E2], 0, FMT_NRATIO - 1)];
    rb1 = FMT_RATIO_Q8[clamp(p[P_E3], 0, FMT_NRATIO - 1)];
    rb2 = rb1 * 2u > 4096u ? 4096u : rb1 * 2u;                     /* B2 an octave above B1, at most 16 */
    ic = fmt_inc(m->inc, rc, 0);
    iaa = fmt_inc(m->inc, ra, p[P_FM3_REL] + dtn);                 /* FTO A and DTN: A up */
    i1 = fmt_inc(m->inc, rb1, p[P_FM4_ATK]);                       /* FTO B1 */
    i2 = fmt_inc(m->inc, rb2, p[P_FM4_DEC] - dtn);                 /* FTO B2 and DTN: B2 down */
    /* key tracking of the modulation: from C4 (pitch16 960) to C8 (1728) it falls linearly, to none at C8 for TRK 127 */
    pr = m->pitch16 > 960 ? m->pitch16 - 960 : 0;
    sa = 32767 - ((pr * p[P_FM3_ATK] * 344) >> 10);
    sb = 32767 - ((pr * p[P_FM3_DEC] * 344) >> 10);
    sa = sa < 0 ? 0 : sa;
    sb = sb < 0 ? 0 : sb;
    /* the track's SHP (and FLT with the FILTER OFF) move the levels; with it on, FLT moves its cutoff */
    adj = ((fon ? 0 : m->cutoff) + m->shape - (64 << 8)) >> 8;
    la = clamp(p[P_FM1_LEVEL] + adj, 0, 127);
    lb = clamp(p[P_FM2_LEVEL] + adj, 0, 127);
    {   /* the envelopes' tick, and the indices (Q8) at the end of this block */
        int32_t ea = fmt_env_tick(&p[P_FM1_ATK], &S->env[0], &S->del[0], &S->st[0]) >> 9;     /* Q15 */
        int32_t eb = fmt_env_tick(&p[P_FM2_ATK], &S->env[1], &S->del[1], &S->st[1]) >> 9;
        ia1 = ((((la << 8) * sa) >> 15) * ea) >> 15;
        ib1 = ((((lb << 8) * sb) >> 15) * eb) >> 15;
    }
    if (!S->init) {
        S->ia = ia1;
        S->ib = ib1;
        S->init = 1;
    }
    ia = S->ia;
    ib = S->ib;
    dia = (ia1 - ia) >> CTL_LOG2;
    dib = (ib1 - ib) >> CTL_LOG2;
    norm = NOTH[alg] ? (int32_t)((32767u << 15) / (uint32_t)(32768 + w * (int32_t)NOTH[alg])) : 32767;
    pc = S->ph[0];
    pa = S->ph[1];
    p1 = S->ph[2];
    p2 = S->ph[3];
    fb1 = S->fb1;
    fb2 = S->fb2;
    dx1 = S->dx1;
    dy1 = S->dy1;
    if (fon)
        uflt_set(&rf, p, (p[P_FCUT] << 8) + m->cutoff, m->pitch16, MT_M2K_FM, MT_M2K_GAIN, MT_M2K_GAIN, &S->flt.drift, &S->flt.nst,
                 MT_VFLT_SEED(t, v - t->v));
    for (i = 0; i < n; i++) {
        int32_t o1, o2, o3, o4, s, ga, gb;
        ia += dia;
        ib += dib;
        o4 = sine_i(p2 + FMT_PM((fb1 + fb2) >> 1, fbi));           /* B2, with feedback */
        fb2 = fb1;
        fb1 = o4;
        ga = ia + (ia >> 7);                                       /* a carrier's level, Q15 */
        gb = ib + (ib >> 7);
        switch (alg) {
        case 0:
            o3 = fmt_w(p1 + FMT_PM(o4, ib), hp);
            o2 = fmt_w(pa + FMT_PM(o3, ib), hp);
            s = fmt_w(pc + FMT_PM(o2, ia), hn);
            break;
        case 1:
            o3 = fmt_w(p1, hp);
            o2 = fmt_w(pa + FMT_PM((o3 + o4) >> 1, ib), hp);
            s = fmt_w(pc + FMT_PM(o2, ia), hn);
            break;
        case 2:
            o3 = fmt_w(p1, hp);
            o2 = fmt_w(pa + FMT_PM(o3, ib), hp);
            s = fmt_w(pc + FMT_PM(o2 >> 1, ia) + FMT_PM(o4 >> 1, ib), hn);
            break;
        case 3:
            o3 = fmt_w(p1 + FMT_PM(o4, ib), hp);
            o2 = fmt_w(pa, hp);
            s = fmt_w(pc + FMT_PM(o2 >> 1, ia) + FMT_PM(o3 >> 1, ib), hn);
            break;
        case 4:
            o3 = fmt_w(p1 + FMT_PM(o4, ib), hp);
            o2 = fmt_w(pa, hp);
            o1 = fmt_w(pc + FMT_PM(o2, ia), hn);
            s = mulq15(o1 + FMT_WT(mulq15(o3, gb), w), norm);
            break;
        case 5:
            o3 = fmt_w(p1 + FMT_PM(o4, ib), hp);
            o2 = fmt_w(pa + FMT_PM(o4, ib), hp);
            o1 = fmt_w(pc + FMT_PM(o4, ib), hn);
            s = mulq15(o1 + FMT_WT(mulq15(o2, ga) + mulq15(o3, gb), w), norm);
            break;
        case 6:
            o3 = fmt_w(p1 + FMT_PM(o4, ib), hp);
            o2 = fmt_w(pa, hp);
            o1 = fmt_w(pc, hn);
            s = mulq15(o1 + FMT_WT(mulq15(o2, ga) + mulq15(o3, gb), w), norm);
            break;
        default:
            o3 = fmt_w(p1, hp);
            o2 = fmt_w(pa, hp);
            o1 = fmt_w(pc, hn);
            s = mulq15(o1 + FMT_WT(mulq15(o2, ga) + mulq15(o3, gb) + mulq15(o4, gb), w), norm);
            break;
        }
        pc += ic;
        pa += iaa;
        p1 += i1;
        p2 += i2;
        {   /* DC blocker (~15 Hz): y = x - x1 + 0.9979 y1, the state with 8 fractional bits */
            dy1 = mulq16(dy1, 65396u) + ((s - dx1) << 8);
            dx1 = s;
            s = (dy1 + 128) >> 8;
        }
        if (fon)
            s = uflt_run(&rf, S->flt.f[0], s);
        out[i] += voice_amp(s, m, i);                              /* full-scale sines: no << 1 */
    }
    S->ia = ia1;
    S->ib = ib1;
    S->ph[0] = pc;
    S->ph[1] = pa;
    S->ph[2] = p1;
    S->ph[3] = p2;
    S->fb1 = fb1;
    S->fb2 = fb2;
    S->dx1 = dx1;
    S->dy1 = dy1;
}

/* name, {ALGO, RAT C, RAT A, RAT B, HARM, DTN, FB, MIX}, {A D S R}, fenv, mono: Mini-Tone's BRASS, PLUCK LEAD, RESO PLUCK and
 * FIFTH RING; MONO2K's FM sounds (its library's, FM+STAT / FM+PAR / FM+DYN, 1.3.6) fitted to its renders on FMTONE: EPIANO,
 * FMBELL, DXBASS and GLASPAD in TINE EP's, BELL's, FM BASS's and GLASS PAD's places (a project keeps its sound: it
 * stores the values), the six after them appended */
static const preset_t FMT_PRESETS[] = {
    {"EPIANO", {4, 3, 3, 3, -35, 3, 0, 124}, {0, 94, 0, 93}, 40, 0},
    {"FMBELL", {2, 3, 9, 6, 0, 4, 0, 114}, {0, 104, 1, 102}, 0, 0},
    {"DXBASS", {7, 3, 4, 6, 63, 1, 0, 127}, {0, 85, 3, 82}, 60, 1},
    {"BRASS", {1, 3, 3, 3, 10, 0, 20, 127}, {40, 70, 100, 55}, 0, 0},
    {"GLASPAD", {3, 3, 3, 6, 55, 0, 81, 59}, {77, 107, 0, 117}, 47, 0},
    {"PLUCK LEAD", {2, 3, 6, 4, 0, 4, 15, 127}, {0, 75, 60, 40}, 0, 1},
    {"RESO PLUCK", {2, 3, 6, 4, 0, 4, 15, 127}, {0, 75, 60, 40}, 40, 1},   /* (its FILTER: eng_uflt.c) */
    {"FIFTH RING", {0, 3, 3, 3, 0, 0, 0, 127}, {0, 90, 70, 60}, 0, 0},
    {"GROWL", {1, 2, 1, 15, -64, 16, 10, 101}, {0, 77, 0, 15}, -15, 1},
    {"GLASSKY", {0, 3, 6, 8, 45, 0, 2, 127}, {0, 82, 0, 81}, 13, 0},
    {"FMPAD", {2, 3, 3, 11, -29, 14, 11, 65}, {88, 123, 43, 97}, 63, 0},
    {"DRONE", {1, 3, 3, 3, 0, 3, 0, 127}, {112, 127, 127, 105}, 0, 1},
    {"BUBBLES", {0, 3, 3, 6, 0, 0, 0, 127}, {0, 90, 127, 30}, 0, 0},
    {"ZAP", {0, 3, 6, 8, 0, 0, 20, 127}, {0, 70, 0, 35}, 0, 0},
};
typedef struct {
    int8_t a[5], b[5];           /* operator A / B: ATK DEC END DEL LEV */
    int8_t k[2], f[3];           /* TRK A, TRK B; FT A, FT B1, FT B2 */
    uint8_t more;                /* 1: the track's LFO, DRIVE and ENV -> PIT / SHP below (MONO2K's sounds), 0: as loaded */
    int8_t lfo[6];               /* RATE WAVE, -> PIT SHP AMP, TRIG */
    int8_t drv[2];               /* DRIVE, SHAPE */
    int8_t ed[2];                /* ENV -> PIT, -> SHP */
} fmt_x_t;
static const fmt_x_t FMT_X[NELEM(FMT_PRESETS)] = {
    {{0, 89, 13, 0, 36}, {52, 91, 29, 46, 22}, {127, 6}, {8, 11, 46}, 1, {60, 0, 0, 0, 0, 0}, {0, 0}, {0, 28}},   /* EPIANO (MONO2K's): warm tine e-piano */
    {{0, 94, 0, 21, 27}, {0, 14, 28, 0, 25}, {103, 0}, {6, 17, 4}, 1, {60, 0, 0, 0, 0, 0}, {0, 0}, {0, 5}},   /* FMBELL (MONO2K's): clear bell, long ring */
    {{28, 7, 127, 18, 0}, {120, 64, 57, 0, 47}, {5, 49}, {51, -46, -33}, 1, {60, 0, 0, 0, 0, 0}, {68, 0}, {19, 63}},   /* DXBASS (MONO2K's): punchy slap bass */
    {{70, 0, 100, 0, 46}, {62, 0, 100, 0, 16}, {10, 0}, {0, 0, 0}},    /* BRASS: the modulation swells in */
    {{23, 15, 41, 14, 50}, {10, 48, 11, 32, 21}, {97, 127}, {4, -59, 28}, 1, {60, 0, 0, 0, 0, 0}, {0, 0}, {0, -64}},   /* GLASPAD (MONO2K's): shimmering bell pad */
    {{0, 55, 30, 0, 32}, {0, 45, 0, 0, 40}, {25, 0}, {0, 0, 0}},       /* PLUCK LEAD */
    {{0, 55, 30, 0, 32}, {0, 45, 0, 0, 40}, {25, 0}, {0, 0, 0}},       /* RESO PLUCK */
    {{0, 90, 20, 0, 24}, {0, 80, 0, 0, 14}, {0, 0}, {0, 0, 0}},        /* FIFTH RING */
    {{5, 18, 86, 0, 76}, {57, 121, 123, 0, 28}, {0, 41}, {46, 14, -52}, 1, {86, 0, 0, 63, 0, 0}, {31, 3}, {0, -50}},   /* GROWL (MONO2K's): growling bass */
    {{0, 89, 1, 0, 39}, {0, 5, 3, 0, 25}, {4, 124}, {-33, -44, 11}, 1, {60, 0, 0, 0, 0, 0}, {0, 0}, {0, 34}},   /* GLASSKY (MONO2K's): glassy mallet keys */
    {{0, 77, 42, 8, 54}, {27, 56, 0, 24, 127}, {87, 52}, {-8, -24, -46}, 1, {60, 1, 0, -4, 0, 1}, {0, 0}, {0, 16}},   /* FMPAD (MONO2K's): slow-breathing pad */
    {{118, 0, 127, 0, 48}, {120, 0, 127, 0, 28}, {0, 0}, {0, 0, 0}, 1, {25, 0, 0, 12, 0, 0}, {40, 0}, {0, 0}},   /* DRONE (MONO2K's): dark swelling drone */
    {{0, 20, 40, 0, 60}, {0, 10, 20, 0, 40}, {0, 0}, {0, 0, 0}, 1, {102, 4, 40, 30, 127, 0}, {0, 0}, {0, 0}},   /* BUBBLES (MONO2K's): bubbling drops */
    {{0, 60, 0, 0, 50}, {0, 50, 0, 0, 30}, {0, 0}, {0, 0, 0}, 1, {60, 0, 0, 0, 0, 0}, {0, 0}, {60, 0}},   /* ZAP (MONO2K's): quick falling zap */
};

static const engine_t ENG_FMTONE = {
    .name = "FMTONE",
    .page_title = {"OPS", "TONE"},
    .edit = {
        {"ALGO", F_ENUM, 0, 7, 0, N_FMT_ALGO, 0},
        {"RAT C", F_ENUM, 0, FMT_NRATIO - 1, 3, N_FMT_RATIO, 0},
        {"RAT A", F_ENUM, 0, FMT_NRATIO - 1, 3, N_FMT_RATIO, 0},
        {"RAT B", F_ENUM, 0, FMT_NRATIO - 1, 3, N_FMT_RATIO, 0},
        {"HARM", F_BIPCT, -64, 63, 0, 0, 0},
        {"DTN", F_INT, 0, 50, 0, 0, "ct"},
        {"FB", F_PCT, 0, 127, 0, 0, 0},
        {"MIX", F_PCT, 0, 127, 127, 0, 0},
    },
    .presets = FMT_PRESETS,
    .npresets = NELEM(FMT_PRESETS),
    .note_on = fmt_note_on,
    .render = fmt_render,
    .knob = {P_FM1_LEVEL, P_FM2_LEVEL, P_E6, P_E4},
    .poly = FMT_POLY,
};

/* =================================================================================================== DUALVCO === */
/* After the SY DUAL VCO machine of Elektron's Syntakt (Mini-Tone's own implementation, no Elektron code or data): two
 * oscillators, each with its own decay, mixed, ring modulated or frequency modulated, with a fixed-time pitch envelope
 * and an overdrive. EDIT 1 (OSC): TUNE (osc 1, -24..+24), DET (osc 2 against osc 1, -48..+48), DEC1, DEC2 (each
 * oscillator's decay; the track's ADSR gates and releases the voice). EDIT 2 (MIX): CNFG (80 values: [*] osc 1, mode,
 * osc 2: osc 1 S sine, Z sinesaw; osc 2 S sine, K skewed sine, T triangle, Z sinesaw, A saw; mode + mix, R ring, FM,
 * F+R; a * first: the phases restart at each note), BEND (a fixed-time pitch envelope up to +24 semitones, and a short
 * transient with it), BAL (the oscillators' balance), OVER (the overdrive). Voice: ph[0] osc 1, ph[1] osc 2, s[0] s[1]
 * the oscillators' decays, s[2] the pitch envelope (Q24). */
#define DUAL_POLY 4
static const char *const N_DUAL_CNFG[] = {
    "S+S", "S+K", "S+T", "S+Z", "S+A", "Z+S", "Z+K", "Z+T", "Z+Z", "Z+A",
    "SRS", "SRK", "SRT", "SRZ", "SRA", "ZRS", "ZRK", "ZRT", "ZRZ", "ZRA",
    "SFMS", "SFMK", "SFMT", "SFMZ", "SFMA", "ZFMS", "ZFMK", "ZFMT", "ZFMZ", "ZFMA",
    "SF+RS", "SF+RK", "SF+RT", "SF+RZ", "SF+RA", "ZF+RS", "ZF+RK", "ZF+RT", "ZF+RZ", "ZF+RA",
    "*S+S", "*S+K", "*S+T", "*S+Z", "*S+A", "*Z+S", "*Z+K", "*Z+T", "*Z+Z", "*Z+A",
    "*SRS", "*SRK", "*SRT", "*SRZ", "*SRA", "*ZRS", "*ZRK", "*ZRT", "*ZRZ", "*ZRA",
    "*SFMS", "*SFMK", "*SFMT", "*SFMZ", "*SFMA", "*ZFMS", "*ZFMK", "*ZFMT", "*ZFMZ", "*ZFMA",
    "*SF+RS", "*SF+RK", "*SF+RT", "*SF+RZ", "*SF+RA", "*ZF+RS", "*ZF+RK", "*ZF+RT", "*ZF+RZ", "*ZF+RA",
};
#define DUAL_RESET_FROM 40u              /* CNFG from here restarts the phases */
#define DUAL_BEND_K 1580u                /* the pitch envelope's step a block, Q16: a time constant of ~30 ms */
#define DUAL_BYTES (sizeof(mt_vflt_t) * NVOICE)

/* the band-limited saw (dsp.c's polyBLEP), as Mini-Tone's dsp.c osc_saw / osc_pulse */
static inline int32_t mt_saw(uint32_t ph, uint32_t inc) { return ((int32_t)(ph >> 16) - 32768) - blep(ph, inc); }
static inline int32_t mt_pulse(uint32_t ph, uint32_t inc, uint32_t pw) { return (mt_saw(ph, inc) - mt_saw(ph + pw, inc)) >> 1; }

static inline int32_t dual_asym(uint32_t ph)
{
    uint32_t a = 0x60000000u, t = ph < a ? ph + ph / 3u : 0x80000000u + (ph - a) / 5u * 4u;
    return sine_i(t);
}
static inline int32_t dual_w1(uint32_t w, uint32_t ph, uint32_t inc)
{
    return w ? (sine_i(ph) - mt_saw(ph, inc)) >> 1 : sine_i(ph);
}
static inline int32_t dual_w2(uint32_t w, uint32_t ph, uint32_t inc)
{
    switch (w) {
    case 1: return dual_asym(ph);
    case 2: return osc_tri(ph);
    case 3: return (sine_i(ph) - mt_saw(ph, inc)) >> 1;
    case 4: return mt_saw(ph, inc);
    default: return sine_i(ph);
    }
}

static mt_vflt_t *dual_flt_of(track_t *t, voice_t *v)
{
    mt_vflt_t *f = mt_mem(t, DUAL_BYTES);
    return f ? f + (uint32_t)(v - t->v) % NVOICE : 0;
}

static void dual_note_on(track_t *t, voice_t *v)
{
    mt_vflt_t *f = dual_flt_of(t, v);
    if ((uint32_t)t->p[P_E4] >= DUAL_RESET_FROM)
        v->ph[0] = v->ph[1] = 0;
    v->s[0] = v->s[1] = v->s[2] = 1 << 24;
    if (f && !(v->env || v->env_out))                     /* the filter from rest (a retrigger keeps it) */
        memset(f->f, 0, sizeof f->f);
}

static void dual_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t cf = (uint32_t)p[P_E4] % 80u, mode = (cf % 40u) / 10u, w1 = (cf % 10u) / 5u, w2 = cf % 5u, i;
    int32_t tune = p[P_E0], det = p[P_E1], bend = p[P_E5], bal = p[P_E6], drive = p[P_E7];
    int32_t e1o = v->s[0], e2o = v->s[1], beo = v->s[2], e1n, e2n, ben;
    int32_t b1 = 16384, b2 = 16384, l1, l2, d1, d2, bo, bn, tr;
    int32_t p1o, p1n, p2o, p2n;
    uint32_t inc1, inc2, ph0 = v->ph[0], ph1 = v->ph[1];
    int32_t di1, di2, in1, in2;
    mt_vflt_t *fs = dual_flt_of(t, v);
    uint32_t fon = p[P_FMODE] != 0 && fs;
    uflt_t rf;
    if (!n)
        return;
    e1n = e1o - mulq16(e1o, ENV_EXP[p[P_E2] & 127]);
    e2n = e2o - mulq16(e2o, ENV_EXP[p[P_E3] & 127]);
    ben = beo - mulq16(beo, DUAL_BEND_K);
    bo = (int32_t)((uint32_t)(beo >> 9) * (uint32_t)(bend * 3) >> 15);     /* the bend in 1/16 semitone, this block's ends */
    bn = (int32_t)((uint32_t)(ben >> 9) * (uint32_t)(bend * 3) >> 15);
    if (bal > 0)                                          /* oscillator 1 fades to the right */
        b1 = 16384 - bal * 256;
    else if (bal < 0)                                     /* oscillator 2 to the left */
        b2 = 16384 + bal * 256;
    b1 = b1 < 0 ? 0 : b1;
    b2 = b2 < 0 ? 0 : b2;
    p1o = clamp(m->pitch16 + tune * 16 + bo, 0, 2047);
    p1n = clamp(m->pitch16 + tune * 16 + bn, 0, 2047);
    p2o = clamp(m->pitch16 + (tune + det) * 16 + bo, 0, 2047);
    p2n = clamp(m->pitch16 + (tune + det) * 16 + bn, 0, 2047);
    in1 = (int32_t)tuned_pitch_inc((uint32_t)p1o);
    in2 = (int32_t)tuned_pitch_inc((uint32_t)p2o);
    di1 = ((int32_t)tuned_pitch_inc((uint32_t)p1n) - in1) / (int32_t)n;
    di2 = ((int32_t)tuned_pitch_inc((uint32_t)p2n) - in2) / (int32_t)n;
    l1 = e1o >> 9;
    l2 = e2o >> 9;
    d1 = ((e1n >> 9) - l1) / (int32_t)n;
    d2 = ((e2n >> 9) - l2) / (int32_t)n;
    tr = 32768 + mulq15((beo >> 9), bend * 258);          /* the transient: up to twice the level at the start of a bend */
    if (fon)
        uflt_set(&rf, p, (p[P_FCUT] << 8) + m->cutoff, m->pitch16, MT_M2K_VCO, MT_M2K_GAIN, MT_M2K_GAIN, &fs->drift, &fs->nst,
                 MT_VFLT_SEED(t, v - t->v));
    for (i = 0; i < n; i++) {
        int32_t a1 = mulq15(l1, b1), a2 = mulq15(l2, b2), s, c;
        inc1 = (uint32_t)in1;
        inc2 = (uint32_t)in2;
        if (mode >= 2u) {                                 /* FM: oscillator 2 modulates oscillator 1's phase */
            int32_t mo = dual_w2(w2, ph1, inc2);
            c = dual_w1(w1, ph0 + ((uint32_t)mulq15(mo, a2 << 1) << 17), inc1);
            c = mulq15(c, a1 << 1);
            s = mode == 3u ? mulq15(c, mulq15(dual_w2(w2, ph1, inc2), a2)) << 2 : c;
        } else {
            int32_t o1 = dual_w1(w1, ph0, inc1), o2 = dual_w2(w2, ph1, inc2);
            s = mode == 1u ? mulq15(mulq15(o1, a1), mulq15(o2, a2)) << 2 : mulq15(o1, a1) + mulq15(o2, a2);
        }
        s = mulq15(s, tr) * 3 >> 2;                       /* (-2.5 dB: two oscillators at 50 % each are a full-scale sum) */
        if (drive)
            s = softclip(((s >> 2) * (int32_t)(256 + drive * 50)) >> 6);
        else
            s = clamp(s, -32767, 32767);
        if (fon)
            s = uflt_run(&rf, fs->f[0], s);
        out[i] += voice_amp(s, m, i);
        ph0 += inc1;
        ph1 += inc2;
        in1 += di1;
        in2 += di2;
        l1 += d1;
        l2 += d2;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = e1n;
    v->s[1] = e2n;
    v->s[2] = ben;
}

/* name, {TUNE, DET, DEC1, DEC2, CNFG, BEND, BAL, OVER}, {A D S R}, fenv, mono (Mini-Tone's) */
static const preset_t DUAL_PRESETS[] = {
    {"FM BELL", {0, 19, 100, 70, 60, 0, 0, 0}, {0, 64, 127, 40}, 0, 0},
    {"SAW+SINE BASS", {0, -12, 127, 127, 4, 0, 0, 20}, {0, 64, 127, 25}, 0, 1},
    {"DUAL LEAD", {0, 12, 127, 127, 9, 0, 0, 10}, {0, 64, 127, 40}, 0, 1},
    {"FM BASS", {0, 0, 127, 60, 22, 20, 0, 0}, {0, 64, 127, 25}, 0, 1},
    {"F+R GLASS", {0, 7, 110, 90, 30, 0, 0, 0}, {10, 64, 127, 60}, 0, 0},
    {"RESO LEAD", {0, 12, 127, 127, 9, 0, 0, 10}, {0, 64, 127, 40}, 30, 1},   /* (its FILTER: eng_uflt.c) */
};

static const engine_t ENG_DUAL = {
    .name = "DUALVCO",
    .page_title = {"OSC", "MIX"},
    .edit = {
        {"TUNE", F_SEMI, -24, 24, 0, 0, 0},
        {"DET", F_SEMI, -48, 48, 7, 0, 0},
        {"DEC1", F_TIME, 0, 127, 100, 0, 0},
        {"DEC2", F_TIME, 0, 127, 80, 0, 0},
        {"CNFG", F_ENUM, 0, 79, 0, N_DUAL_CNFG, 0},
        {"BEND", F_PCT, 0, 127, 0, 0, 0},
        {"BAL", F_BIPCT, -64, 63, 0, 0, 0},
        {"OVER", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = DUAL_PRESETS,
    .npresets = NELEM(DUAL_PRESETS),
    .note_on = dual_note_on,
    .render = dual_render,
    .knob = {P_E1, P_E4, P_E5, P_E7},
    .poly = DUAL_POLY,
};

/* =================================================================================================== ANALOG4 === */
/* A two-oscillator subtractive voice in the manner of Elektron's Analog Four (Mini-Tone's digital imitation, from its
 * published description; no Elektron code or data): two oscillators with a sub oscillator and noise, two filters (in
 * series or in parallel), a filter envelope and a second, one-shot envelope. EDIT 1 (OSC 1): WAV1 (SIN TRI SAW PLS),
 * PW1, TUN1, SUB (a sine an octave below); EDIT 2 (OSC 2): WAV2, PW2, TUN2, FIN2. MIX: LEV1 LEV2 NOIS SYNC. FILTER (the
 * page, eng_uflt.c): filter 1. F1 ENV: FATK FDEC FSUS FREL, its amount F1EN (on F1 MOD with DRV, the drive into the
 * filters): the cutoff is CUT at the envelope's top and falls by F1EN x (1 - envelope). FLT 2: F2MD (LP / BP / HP, S in
 * series after filter 1, P in parallel), CUT2, RES2, KEY (both filters' key tracking, from C4). ENV 2: E2AT E2DC, its
 * amount E2AM and destination E2DS (OFF, PTCH+, PTCH-: both pitches up to two octaves and back, PW, F2: filter 2's
 * cutoff). The amplitude is the track's ADSR. Voice: ph[0] ph[1] ph[2] the oscillators and the sub, s[4] the filter
 * envelope and s[5] ENV 2 (Q24), s[6] their stages, s[7] the noise; the filters' states in the part's memory. */
#define A4_POLY 4
#define AP_FA P_FM1_ATK          /* the filter envelope: A D S R in this order (a4_adsr reads them as an array) */
#define AP_FD P_FM1_DEC
#define AP_FS P_FM1_SUS
#define AP_FR P_FM1_REL
#define AP_LEV1 P_FM1_LEVEL
#define AP_GA P_FM2_ATK          /* ENV 2: ATK DEC, AMT, DST */
#define AP_GD P_FM2_DEC
#define AP_GAM P_FM2_SUS
#define AP_GDS P_FM2_REL
#define AP_LEV2 P_FM2_LEVEL
#define AP_NOIS P_FM3_ATK
#define AP_SYNC P_FM3_DEC
#define AP_F1EN P_FM3_SUS        /* (P_FM3_REL and P_FM3_LEVEL, Mini-Tone's RES1 and CUT1: the FILTER page's) */
#define AP_DRV P_FM4_ATK
#define AP_F2MD P_FM4_DEC
#define AP_KEY P_FM4_SUS         /* the filters' key tracking */
#define AP_RES2 P_FM4_REL
#define AP_CUT2 P_FM4_LEVEL
_Static_assert(AP_FS == AP_FA + 2 && AP_FR == AP_FA + 3 && AP_GDS == AP_GA + 3, "a4_adsr reads A D S R as an array");

static const char *const N_A4_WAVE[] = {"SIN", "TRI", "SAW", "PLS"};
static const char *const N_A4_SYNC[] = {"OFF", "ON"};
static const char *const N_A4_F2MD[] = {"LP-S", "BP-S", "HP-S", "LP-P", "BP-P", "HP-P"};
static const char *const N_A4_E2DS[] = {"OFF", "PTCH+", "PTCH-", "PW", "F2"};
#define A4_E2_UP 1u
#define A4_E2_DOWN 2u
#define A4_E2_PW 3u
#define A4_E2_F2 4u

static const param_desc_t A4_TD[P_FM4_LEVEL - P_FM1_ATK + 1] = {
    [AP_FA - P_FM1_ATK] = {"FATK", F_TIME, 0, 127, 0, 0, 0},
    [AP_FD - P_FM1_ATK] = {"FDEC", F_TIME, 0, 127, 0, 0, 0},
    [AP_FS - P_FM1_ATK] = {"FSUS", F_PCT, 0, 127, 127, 0, 0},
    [AP_FR - P_FM1_ATK] = {"FREL", F_TIME, 0, 127, 0, 0, 0},
    [AP_LEV1 - P_FM1_ATK] = {"LEV1", F_PCT, 0, 127, 127, 0, 0},
    [AP_GA - P_FM1_ATK] = {"E2AT", F_TIME, 0, 127, 0, 0, 0},
    [AP_GD - P_FM1_ATK] = {"E2DC", F_TIME, 0, 127, 0, 0, 0},
    [AP_GAM - P_FM1_ATK] = {"E2AM", F_PCT, 0, 127, 127, 0, 0},
    [AP_GDS - P_FM1_ATK] = {"E2DS", F_ENUM, 0, 4, 0, N_A4_E2DS, 0},
    [AP_LEV2 - P_FM1_ATK] = {"LEV2", F_PCT, 0, 127, 127, 0, 0},
    [AP_NOIS - P_FM1_ATK] = {"NOIS", F_PCT, 0, 127, 0, 0, 0},
    [AP_SYNC - P_FM1_ATK] = {"SYNC", F_ENUM, 0, 1, 0, N_A4_SYNC, 0},
    [AP_F1EN - P_FM1_ATK] = {"F1EN", F_PCT, 0, 127, 127, 0, 0},
    [AP_DRV - P_FM1_ATK] = {"DRV", F_PCT, 0, 127, 0, 0, 0},
    [AP_F2MD - P_FM1_ATK] = {"F2MD", F_ENUM, 0, 5, 0, N_A4_F2MD, 0},
    [AP_KEY - P_FM1_ATK] = {"KEY", F_PCT, 0, 127, 127, 0, 0},
    [AP_RES2 - P_FM1_ATK] = {"RES2", F_PCT, 0, 127, 0, 0, 0},
    [AP_CUT2 - P_FM1_ATK] = {"CUT2", F_CUTOFF, 0, 127, 127, 0, 0},
};

typedef struct {
    uint32_t cnt;                /* the notes started: the noise's seed */
    mt_vflt_t f[NVOICE][2];      /* each voice's filters 1 and 2 (f[k][1].f[0]: filter 2's states) */
} a4_part_t;
#define A4_BYTES sizeof(a4_part_t)

/* an ADSR (a[0..3] = A D S R) or, with ad, only its A and D (it falls to 0 after the attack), one control tick; st: 0
 * attack, 1 decay / sustain, 2 release. Q24 */
static int32_t a4_adsr(const int16_t *a, int32_t e, uint32_t *st, uint32_t gate, uint32_t ad)
{
    if (!gate && !ad)
        *st = 2;
    if (*st == 0) {
        if (!a[0]) {
            e = 1 << 24;
        } else {
            e += (int32_t)ENV_LIN[a[0] & 127];
        }
        if (e >= (1 << 24)) {
            e = 1 << 24;
            *st = 1;
        }
    } else if (*st == 1) {
        int32_t tg = ad ? 0 : a[2] >= 127 ? 1 << 24 : (int32_t)a[2] << 17;
        e += mulq16(tg - e, ENV_EXP[a[1] & 127]);
    } else {
        e -= mulq16(e, ENV_EXP[a[3] & 127]);
    }
    return clamp(e, 0, 1 << 24);
}

static void a4_note_on(track_t *t, voice_t *v)
{
    a4_part_t *P = mt_mem(t, A4_BYTES);
    uint32_t i, k = (uint32_t)(v - t->v) % NVOICE;
    if (!(v->env || v->env_out)) {                    /* a voice from silence: everything from zero */
        v->ph[0] = v->ph[1] = v->ph[2] = 0;
        for (i = 0; i < 6u; i++)
            v->s[i] = 0;
        if (P)
            memset(P->f[k], 0, sizeof P->f[k]);
        v->s[7] = (int32_t)((((v->note + 1u) * 0x9E3779B9u) ^ ((P ? ++P->cnt : 1u) * 0xC2B2AE35u)) | 1u);
    }
    v->s[6] = 0;                                      /* both envelopes attack (a retrigger starts from where they are) */
    if (!v->s[7])
        v->s[7] = 0x2545F491;
}

/* the oscillator waves: SIN TRI SAW PLS */
static inline int32_t a4_osc(uint32_t w, uint32_t ph, uint32_t inc, uint32_t pw)
{
    switch (w) {
    case 1: return osc_tri(ph);
    case 2: return mt_saw(ph, inc);
    case 3: return mt_pulse(ph, inc, pw);
    default: return sine_i(ph);
    }
}

static inline int32_t a4_xamt(int32_t e15, int32_t am) { return (e15 * am) >> 7; }   /* ENV 2's amount, 0..32512 */

/* filter 1's cutoff: CUT is where the envelope's top (1.0, which FSUS 127 holds) puts it, the envelope closing it by
 * (1 - envelope) x F1EN, up to the whole range (127 steps); then the key and the track's FLT routing */
static inline int32_t a4_cut(int32_t knob, int32_t env15, int32_t amt, int32_t key, int32_t mod)
{
    return (knob << 8) - (((32767 - env15) * amt) >> 7) + key + mod;
}

static void a4_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    a4_part_t *P = mt_mem(t, A4_BYTES);
    uint32_t w1 = (uint32_t)p[P_E0] & 3u, w2 = (uint32_t)p[P_E4] & 3u, i, st = (uint32_t)v->s[6], vk;
    uint32_t fst = st & 3u, xst = (st >> 4) & 3u, f2m = (uint32_t)p[AP_F2MD] % 6u, f2ty = f2m % 3u, par = f2m >= 3u;
    uint32_t ds = (uint32_t)p[AP_GDS] % 5u, sync = (uint32_t)p[AP_SYNC] & 1u, xst2, f1on, f2on;
    int32_t fe, xe, fe15, xe15, zo = 0, zn = 0, pwx = 0, c2x = 0, key, cut2;
    int32_t pw12, l1 = p[AP_LEV1] * 258, l2 = p[AP_LEV2] * 258, ls = p[P_E3] * 258, ln = p[AP_NOIS] * 258;
    int32_t drv = p[AP_DRV], drive = 32768 + drv * 512, ct = p[P_E7] * 2367 / 1000, rng = v->s[7];
    int32_t p1o, p1n, p2o, p2n, in1, in2, di1, di2;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2], pw;
    uflt_t f1, f2;
    mt_vflt_t *s1, *s2;
    if (!n || !P)
        return;
    vk = (uint32_t)(v - t->v) % NVOICE;
    s1 = &P->f[vk][0];
    s2 = &P->f[vk][1];
    fe = a4_adsr(&p[AP_FA], v->s[4], &fst, v->gate, 0);
    xe = a4_adsr(&p[AP_GA], v->s[5], &xst, 1, 1);
    xst2 = xst;
    fe15 = fe >> 9;
    xe15 = xe >> 9;
    if (ds == A4_E2_PW)
        pwx = a4_xamt(xe15, p[AP_GAM]) >> 3;           /* 0..4064: the 12-bit width */
    else if (ds == A4_E2_F2)
        c2x = a4_xamt(xe15, p[AP_GAM]);                /* up to the whole range of cutoffs */
    key = (((m->pitch16 - 960) * 19) >> 1) * p[AP_KEY] / 127;   /* KEY 100 %: half a step a semitone */
    pw12 = clamp(p[P_E1] * 32 + pwx + ((m->shape - (64 << 8)) >> 3), 16, 4080);
    pw = (uint32_t)pw12 << 20;
    f1on = p[P_FMODE] != 0;
    if (f1on)
        uflt_set(&f1, p, a4_cut(p[P_FCUT], fe15, p[AP_F1EN], p[P_FMODE] == UF_M2K ? 0 : key, m->cutoff), m->pitch16,
                 MT_M2K_VCO, MT_M2K_GAIN, MT_M2K_GAIN, &s1->drift, &s1->nst, MT_VFLT_SEED(t, vk));   /* (M2K: its own key) */
    cut2 = (p[AP_CUT2] << 8) + key + m->cutoff + c2x;
    f2on = par || f2ty || p[AP_RES2] || cut2 < (127 << 8);   /* (an open low-pass in series: nothing to do) */
    if (f2on)
        uflt_coef(&f2, f2ty + 1u, cut2, p[AP_RES2], &s2->drift, &s2->nst, MT_VFLT_SEED(t, vk + NVOICE));
    if (ds == A4_E2_UP || ds == A4_E2_DOWN) {          /* the zap falls over the block */
        zo = a4_xamt(xe15, p[AP_GAM]) * 384 >> 15;     /* up to two octaves, in 1/16 semitone */
        zn = a4_xamt(a4_adsr(&p[AP_GA], xe, &xst2, 1, 1) >> 9, p[AP_GAM]) * 384 >> 15;
        if (ds == A4_E2_DOWN) {
            zo = -zo;
            zn = -zn;
        }
    }
    p1o = clamp(m->pitch16 + p[P_E2] * 16 + zo, 0, 2047);
    p1n = clamp(m->pitch16 + p[P_E2] * 16 + zn, 0, 2047);
    p2o = clamp(m->pitch16 + p[P_E6] * 16 + zo, 0, 2047);
    p2n = clamp(m->pitch16 + p[P_E6] * 16 + zn, 0, 2047);
    in1 = (int32_t)tuned_pitch_inc((uint32_t)p1o);
    in2 = (int32_t)tuned_pitch_inc((uint32_t)p2o);
    di1 = ((int32_t)tuned_pitch_inc((uint32_t)p1n) - in1) / (int32_t)n;
    di2 = ((int32_t)tuned_pitch_inc((uint32_t)p2n) - in2) / (int32_t)n;
    in1 += (in1 >> 12) * m->fine;                      /* UNISON, TUNE and bend below 1/16 semitone */
    in2 += (in2 >> 12) * (m->fine + ct);
    for (i = 0; i < n; i++) {
        int32_t s, o1, o2, o3 = 0, nz = 0, acc, y;
        uint32_t oph = ph0;
        o1 = a4_osc(w1, ph0, (uint32_t)in1, pw);
        ph0 += (uint32_t)in1;
        if (sync && ph0 < oph)
            ph1 = 0;                                   /* osc 2 restarts with every cycle of osc 1 */
        o2 = a4_osc(w2, ph1, (uint32_t)in2, pw);
        ph1 += (uint32_t)in2;
        if (ls) {
            o3 = sine_i(ph2);
            ph2 += (uint32_t)in1 >> 1;                 /* one octave below oscillator 1 */
        }
        if (ln)
            nz = (int32_t)noise32(&rng) >> 16;
        acc = mulq15(o1, l1) + mulq15(o2, l2) + mulq15(o3, ls) + mulq15(nz, ln);
        s = clamp(acc * 5 >> 3, -65535, 65535);        /* two full oscillators peak at 1.25 */
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);   /* (Mini-Tone: halved into its filters, doubled out) */
        s = clamp(s, -32767, 32767);
        y = f1on ? uflt_run(&f1, s1->f[0], s) : s;
        if (par)
            y = ((y + uflt_run(&f2, s2->f[0], s)) * 23) >> 5;   /* (-3 dB: two paths) */
        else if (f2on)
            y = uflt_run(&f2, s2->f[0], y);
        out[i] += voice_amp(y, m, i);
        in1 += di1;
        in2 += di2;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[4] = fe;
    v->s[5] = xe;
    v->s[6] = (int32_t)(fst | (xst << 4));
    v->s[7] = rng;
}

/* name, {WAV1, PW1, TUN1, SUB, WAV2, PW2, TUN2, FIN2}, {A D S R}, fenv, mono (Mini-Tone's; their filter 1 on the FILTER
 * page: eng_uflt.c) */
static const preset_t A4_PRESETS[] = {
    {"SAW BASS", {2, 64, 0, 0, 2, 64, -12, 6}, {0, 60, 100, 25}, 0, 1},
    {"PWM LEAD", {3, 40, 0, 0, 3, 90, 0, -7}, {0, 70, 100, 40}, 0, 1},
    {"SYNC LEAD", {2, 64, 0, 0, 2, 64, 7, 0}, {0, 70, 100, 35}, 0, 1},
    {"ACID", {2, 64, 0, 0, 2, 64, 0, 0}, {0, 50, 70, 20}, 0, 1},
    {"WARM PAD", {2, 64, 0, 40, 3, 70, 0, 9}, {70, 90, 110, 85}, 0, 0},
};
typedef struct {
    int8_t fa, fd, fs, fr, lev1, lev2, ga, gd, gam, gds, nois, sync, f1en, drv, f2md, key, res2, cut2;
} a4_x_t;
static const a4_x_t A4_X[NELEM(A4_PRESETS)] = {
    /* fa fd fs fr lev1 lev2 ga gd gam gds nois sync f1en drv f2md key res2 cut2 */
    {0, 55, 30, 30, 127, 100, 0, 0, 127, 0, 0, 0, 70, 10, 0, 127, 0, 127},       /* SAW BASS */
    {0, 70, 127, 40, 127, 127, 0, 0, 127, 0, 0, 0, 0, 0, 0, 127, 0, 127},        /* PWM LEAD */
    {0, 60, 80, 40, 127, 100, 0, 0, 127, 0, 0, 1, 40, 0, 0, 127, 0, 127},        /* SYNC LEAD */
    {0, 50, 0, 25, 127, 127, 0, 0, 127, 0, 0, 0, 90, 20, 0, 127, 0, 127},        /* ACID */
    {40, 80, 100, 80, 90, 90, 0, 0, 127, 0, 5, 0, 25, 0, 0, 127, 0, 127},        /* WARM PAD */
};

static const engine_t ENG_ANA4 = {
    .name = "ANALOG4",
    .page_title = {"OSC 1", "OSC 2"},
    .edit = {
        {"WAV1", F_ENUM, 0, 3, 2, N_A4_WAVE, 0},
        {"PW1", F_PCT, 0, 127, 64, 0, 0},
        {"TUN1", F_INT, -24, 24, 0, 0, "st"},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"WAV2", F_ENUM, 0, 3, 2, N_A4_WAVE, 0},
        {"PW2", F_PCT, 0, 127, 64, 0, 0},
        {"TUN2", F_INT, -24, 24, 0, 0, "st"},
        {"FIN2", F_INT, -64, 63, 0, 0, "ct"},
    },
    .presets = A4_PRESETS,
    .npresets = NELEM(A4_PRESETS),
    .note_on = a4_note_on,
    .render = a4_render,
    .knob = {P_E0, P_E1, P_E4, P_E7},
    .poly = A4_POLY,
    .keep = 0xF0,                /* the envelopes' levels and stages (s4 s5 s6), the noise (s7) */
};

/* ===================================================================================================== shared === */
/* the operator slots' descriptors of an engine here (params.c track_desc, param_desc_of), 0: TP's */
static const param_desc_t *mt_tdesc(uint32_t e, uint32_t id)
{
    const param_desc_t *d;
    if (id < P_FM1_ATK || id > P_FM4_LEVEL)
        return 0;
    d = e == ENGI_FMTONE ? &FMT_TD[id - P_FM1_ATK] : e == ENGI_ANA4 ? &A4_TD[id - P_FM1_ATK] : 0;
    return d && d->label ? d : 0;
}

/* the part memory an engine here keeps (engines.c eng_state_size), 0: none */
static uint32_t mt_state_size(uint32_t e)
{
    return e == ENGI_FMTONE ? FMT_BYTES : e == ENGI_DUAL ? DUAL_BYTES : e == ENGI_ANA4 ? A4_BYTES : 0;
}

/* the preset a track just loaded (ui.c apply_preset_to, tests/hostsim.c): its operator slots (the common part of a
 * sound load has set them to TP's defaults) */
static void mt_track_loaded(track_t *t)
{
    uint32_t k;
    if (t->eng_req == ENGI_FMTONE) {
        const fmt_x_t *x = &FMT_X[(uint32_t)t->preset % NELEM(FMT_X)];
        for (k = 0; k < 5u; k++) {
            t->p[P_FM1_ATK + k] = x->a[k];
            t->p[P_FM2_ATK + k] = x->b[k];
        }
        t->p[P_FM3_ATK] = x->k[0];
        t->p[P_FM3_DEC] = x->k[1];
        t->p[P_FM3_REL] = x->f[0];
        t->p[P_FM4_ATK] = x->f[1];
        t->p[P_FM4_DEC] = x->f[2];
        if (x->more) {
            static const uint8_t LP[6] = {P_LRATE, P_LWAVE, P_LD_PIT, P_LD_SHP, P_LD_AMP, P_LTRIG};
            for (k = 0; k < 6u; k++)
                t->p[LP[k]] = x->lfo[k];
            t->p[P_DRIVE] = x->drv[0];
            t->p[P_DSHAPE] = x->drv[1];
            t->p[P_ED_PIT] = x->ed[0];
            t->p[P_ED_SHP] = x->ed[1];
        }
    } else if (t->eng_req == ENGI_ANA4) {
        const a4_x_t *x = &A4_X[(uint32_t)t->preset % NELEM(A4_X)];
        t->p[AP_FA] = x->fa; t->p[AP_FD] = x->fd; t->p[AP_FS] = x->fs; t->p[AP_FR] = x->fr;
        t->p[AP_LEV1] = x->lev1; t->p[AP_LEV2] = x->lev2;
        t->p[AP_GA] = x->ga; t->p[AP_GD] = x->gd; t->p[AP_GAM] = x->gam; t->p[AP_GDS] = x->gds;
        t->p[AP_NOIS] = x->nois; t->p[AP_SYNC] = x->sync; t->p[AP_F1EN] = x->f1en; t->p[AP_DRV] = x->drv;
        t->p[AP_F2MD] = x->f2md; t->p[AP_KEY] = x->key; t->p[AP_RES2] = x->res2; t->p[AP_CUT2] = x->cut2;
    }
}
