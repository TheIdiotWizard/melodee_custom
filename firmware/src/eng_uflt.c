/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The FILTER page (EDIT > FILTER, P_FMODE P_FCUT P_FRES and ENV's P_ED_FLT): TRIO's filter (eng_trio.c's tsvf and mix,
 * Felucca's) as one filter for the engines that have the page: MONO2K's SAW ENS WAV NOIS (eng_m2k.c, 1.2.4) and
 * Mini-Tone's FMTONE DUALVCO ANALOG4 (eng_mt.c, 1.2.6), after the engine's oscillators and before the voice's amp.
 * MODE OFF LP BP HP NOT, CUT (plus what moves it: the track's LFO / ENV -> FLT, an engine's own filter envelope), RES
 * (damping 2 .. 0.25: no self-oscillation); the cutoff wanders a little, block by block; the input saturates above
 * 16000, the output is linear to half scale, as TRIO's. MODE OFF: nothing (the engine as it was). */
typedef struct { int32_t a1, a2, a3, ma, mb, mc; } uflt_t;

/* the engines with the FILTER page (ui.c page_visible, midi_control.c's CC 74 / 71) */
static int uflt_engine(uint32_t e)
{
    return e == ENGI_SAW || e == ENGI_SAW + 1u || e == ENGI_WAV ||   /* (NOISE has its own filter; NOIS retired, 1.2.8) */
           (e >= ENGI_FMTONE && e <= ENGI_ANA4);
}

/* this block's coefficients: mode 1..4 (LP BP HP NOT), cut8 the cutoff in steps << 8 (CUT << 8 and what moves it),
 * res 0..127; drift and nst a voice's wander and its noise (seed: a nonzero start, the voice's own) */
static __attribute__((noinline)) void uflt_coef(uflt_t *f, uint32_t mode, int32_t cut8, int32_t res, int32_t *drift, int32_t *nst, uint32_t seed)
{
    static const int16_t MIX[4][3] = {{0, 0, 1024}, {0, 1400, 0}, {1024, 0, -1024}, {1024, 0, 0}};   /* TRIO_MIX */
    int32_t k = 8192 - clamp(res, 0, 127) * 7168 / 127;
    tsvf_t c;
    mode = (mode - 1u) & 3u;
    if (!*nst) *nst = (int32_t)(seed | 1u);
    *drift += (((int32_t)(noise32(nst) >> 22) - 512) - *drift) >> 3;
    tsvf_coef_k(&c, clamp(cut8 + *drift, 0, 127 << 8), k);
    f->a1 = c.a1; f->a2 = c.a2; f->a3 = c.a3;
    f->ma = MIX[mode][0]; f->mb = mode >= 2u ? -(k >> 2) : MIX[mode][1]; f->mc = MIX[mode][2];
}

/* one sample through it: st its two states (a voice's, a side's) */
static inline int32_t uflt_run(const uflt_t *f, int32_t *st, int32_t x)
{
    int32_t ic1 = st[0], ic2 = st[1], v1, v2, v3, a, y;
    x = soft_knee(x, 16000) >> 1;
    v3 = x - ic2;
    v1 = (f->a1 * ic1 + f->a2 * v3) >> 13;
    v2 = ic2 + ((f->a2 * ic1 + f->a3 * v3) >> 13);
    ic1 = 2 * v1 - ic1;
    ic2 = clamp(2 * v2 - ic2, -150000, 150000);
    a = clamp(ic1, -49152, 49152);
    st[0] = a + ((ic1 - a) >> 2);
    st[1] = ic2;
    y = soft_knee((x * f->ma + v1 * f->mb + v2 * f->mc) >> 10, 16000) << 1;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

/* each factory preset's FILTER (ui.c apply_preset_to, after the common defaults set MODE OFF, CUT 127, RES 0) */
static void uflt_preset_loaded(track_t *t, uint32_t pi)
{
    static const struct { uint8_t eng, preset, mode, cut, res; } F[] = {
        {ENGI_FMTONE, 6, 1, 60, 90},     /* RESO PLUCK: a plucked lead through a resonant low-pass the envelope opens */
        {ENGI_FMTONE, 7, 2, 76, 110},    /* FIFTH RING: a band-pass ringing above the bell */
        {ENGI_DUAL, 8, 1, 60, 90},       /* RESO LEAD */
        {ENGI_ANA4, 0, 1, 100, 30},      /* SAW BASS (filter 1 as Mini-Tone's, on this filter) */
        {ENGI_ANA4, 1, 1, 110, 20},      /* PWM LEAD */
        {ENGI_ANA4, 2, 1, 100, 25},      /* SYNC LEAD */
        {ENGI_ANA4, 3, 1, 95, 110},      /* ACID */
        {ENGI_ANA4, 4, 1, 85, 20},       /* WARM PAD */
        {ENGI_ANA4, 5, 1, 127, 0},       /* KICK */
        {ENGI_ANA4, 6, 1, 100, 0},       /* ZAP TOM */
        {ENGI_ANA4, 7, 3, 100, 0},       /* NOISE HAT: a high-pass */
    };
    for (uint32_t k = 0; k < NELEM(F); k++)
        if (F[k].eng == t->eng_req && F[k].preset == pi) {
            t->p[P_FMODE] = F[k].mode; t->p[P_FCUT] = F[k].cut; t->p[P_FRES] = F[k].res;
        }
}
