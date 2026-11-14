/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The FILTER page (EDIT > FILTER: P_FMODE P_FCUT P_FRES P_FDIST) for the engines that have it: MONO2K's SAW PULS ENS
 * WAV (eng_m2k.c) and Mini-Tone's FMTONE DUALVCO ANALOG4 (eng_mt.c), after the engine's oscillators and before the
 * voice's amp. Since 1.3.8 both modes are MONO2K's chain (m2k_dsp.c track_block and chain_filter, Sequencist's,
 * GPL-3.0-only): DIST (P_FDIST, less 64: its law and each machine's ceiling) into LP1, a resonant two-pole clipped at
 * that ceiling (the scream), the HP's two-pole (its states held by the ceiling too), its one-pole and LP2's two; the
 * cutoffs follow the note as MONO2K's. LP: CUT its WDTH (BASE 0), RES its LPQ. HP: CUT its BASE (WDTH 127 above it),
 * RES its HPQ. CUT plus what moves it (the LFO / ENV -> FLT, an engine's own filter envelope), a step a step. The
 * engine's voice scaled back to the machine's level in (MONO2K's), out at SAW's level of it (M2K_FOUT), its peaks
 * rounded over 16000. Its laws MONO2K's tables (m2k_tables.h; T_FG and T_FG1 every fourth entry, read between them).
 * TRIO's filter (uflt_coef / uflt_run) stays for ANALOG4's filter 2. Until 1.3.7: MODE OFF LP BP HP NOT (TRIO's) and
 * M2K (1.3.7: the chain's LP), uflt_mode. */
#include "m2k_tables.h"
#define M2K_FOUT 10240                 /* the chain's machine sample out as a voice's (Q12, every engine: SWAV-SAW's 2.5 x) */
typedef struct { int32_t s1, s2; uint32_t r1, r2; } m2kz_t;          /* a two-pole's states, its remainders */
typedef struct { int32_t s; uint32_t r; } m2ko_t;                    /* a one-pole's */
typedef struct { m2kz_t lp1, hp2; m2ko_t hp1, lp2a, lp2b; } uflt_m2k_st_t;   /* (lp1's s1 s2 first: TRIO's two states) */
#define UFLT_ST ((int)(sizeof(uflt_m2k_st_t) / sizeof(int32_t)))    /* a voice's FILTER state, words */
typedef struct {
    int32_t a1, a2, a3, ma, mb, mc;  /* TRIO's (M2K: LP1's a1 a2 a3) */
    int32_t b1, b2, b3, hk;          /* M2K: the HP two-pole's, its damping */
    int32_t g1, g2, dh, ceil, gin, gout;   /* HP1's and LP2's G, DIST's gain, the ceiling, the machine's level in and out */
    uint8_t m2k;
} uflt_t;
enum { UF_OFF, UF_LP, UF_HP };      /* MODE (1.3.8: LP and HP MONO2K's chain; until 1.3.7 OFF LP BP HP NOT M2K, TRIO's four) */

/* the engines with the FILTER page (ui.c page_visible, midi_control.c's CC 74 / 71) */
static int uflt_engine(uint32_t e)
{
    return e == ENGI_SAW || e == ENGI_SAW + 1u || e == ENGI_PULS || e == ENGI_WAV ||   /* (NOISE has its own filter; NOIS retired, 1.2.8) */
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
    f->m2k = 0;
}

/* MONO2K's chain (MODE M2K): a Q24 coefficient at a filter position (1/96 octave over MIDI note 0, Q8), between the
 * quarter tables' entries (T_FG4, T_FG14: rising) */
static uint32_t m2k_fg_at(const uint32_t *T, int32_t pos)
{
    if (pos <= 0) return T[0];
    if (pos >= F_TOP * 256) pos = F_TOP * 256;
    int32_t i = pos >> 10;
    uint32_t a = T[i], b = T[i + (i * 4 < F_TOP)];
    uint32_t span = (uint32_t)((i + 1) * 4 <= F_TOP ? 1024 : (F_TOP - i * 4) * 256);
    return a + (uint32_t)(((uint64_t)(b - a) * (uint32_t)(pos - i * 1024)) / span);
}
/* g, k Q24 -> a1 a2 a3 Q30 (MONO2K's zsvf_set: one division and a Newton step) */
static void m2k_zset(int32_t *a, uint32_t g, uint32_t k)
{
    int32_t den = (1 << 24) + (int32_t)(((int64_t)(int32_t)g * (int32_t)(g + k)) >> 24);
    int32_t a1 = (int32_t)((0x80000000u / (uint32_t)(den >> 10)) << 13);
    int32_t e = (1 << 30) + (int32_t)(((int64_t)-den * a1) >> 24);
    a1 += (int32_t)(((int64_t)a1 * e) >> 30);
    a[0] = a1;
    a[1] = (int32_t)(((int64_t)(int32_t)g * a1) >> 24);
    a[2] = (int32_t)(((int64_t)(int32_t)g * a[1]) >> 24);
}
/* this block's: cut8 WDTH in steps << 8 (and what moves it), res its LPQ 0..127, dist DIST 0..127, pitch16 the voice's
 * pitch (1/16 semitone), mach MONO2K's machine (its DIST ceiling: T_DIST_M), gain the engine's voice level a machine
 * sample (Q12: SAW's 10240, 2.5 x), gin the input's (4096: the machine's own sample, MONO2K's engines) */
static __attribute__((noinline)) void uflt_m2k_coef(uflt_t *f, int32_t cut8, int32_t res, int32_t dist, int32_t pitch16,
                                                    uint32_t mach, int32_t gin, int32_t gain, int hp)
{
    int32_t a[3];
    res = clamp(res, 0, 127); dist = clamp(dist, 0, 127);
    int32_t key = pitch16 * 128;                                   /* note x 8 x 256 */
    int32_t lq = hp ? 0 : res, hq = hp ? res : 0;                  /* LP: WDTH and LPQ (BASE 0); HP: BASE and HPQ (WDTH 127) */
    int32_t sb = hp ? clamp(cut8, 0, 127 << 8) : 0, sl = hp ? sb + (127 << 8) : clamp(cut8, 0, 127 << 8);
    int32_t ph = key + 12 * (sb - 16 * 256), pl = key + 12 * (sl - 16 * 256);
    m2k_zset(a, m2k_fg_at(T_FG4, pl + T_LPC[lq]), T_LPK[lq]);
    f->a1 = a[0]; f->a2 = a[1]; f->a3 = a[2];
    m2k_zset(a, m2k_fg_at(T_FG4, ph + T_HPC[hq]), T_HPK[hq]);
    f->b1 = a[0]; f->b2 = a[1]; f->b3 = a[2]; f->hk = (int32_t)T_HPK[hq];
    f->g2 = (int32_t)m2k_fg_at(T_FG14, pl + F_LP2 * 256);
    f->g1 = (int32_t)m2k_fg_at(T_FG14, ph + F_HP1 * 256);
    f->dh = (int32_t)T_DIST_H[dist];
    f->ceil = (int32_t)(((int64_t)T_DIST_C[dist] * T_DIST_M[mach]) >> 12);
    f->gin = (int32_t)((4096u << 16) / (uint32_t)gin);
    f->gout = M2K_FOUT;
    f->m2k = 1;
}
static inline void m2k_ztick(m2kz_t *z, int32_t a1, int32_t a2, int32_t a3, int32_t v0, int32_t *lp, int32_t *bp)
{
    int32_t v3 = v0 - z->s2;
    int64_t p1 = (int64_t)a1 * z->s1 + (int64_t)a2 * v3 + z->r1;
    int64_t p2 = (int64_t)a2 * z->s1 + (int64_t)a3 * v3 + z->r2;
    int32_t v1 = (int32_t)(p1 >> 30), d2 = (int32_t)(p2 >> 30);
    z->r1 = (uint32_t)p1 & ((1u << 30) - 1u);
    z->r2 = (uint32_t)p2 & ((1u << 30) - 1u);
    int32_t v2 = z->s2 + d2;
    z->s1 = clamp(2 * v1 - z->s1, -(1 << 28), 1 << 28);
    z->s2 = clamp(2 * v2 - z->s2, -(1 << 28), 1 << 28);
    *lp = v2; *bp = v1;
}
static inline int32_t m2k_olp(m2ko_t *o, int32_t g, int32_t x)
{
    int64_t p = (int64_t)(x - o->s) * g + o->r;
    int32_t v = (int32_t)(p >> 24);
    o->r = (uint32_t)p & ((1u << 24) - 1u);
    int32_t y = v + o->s;
    o->s = clamp(y + v, -(1 << 28), 1 << 28);
    return y;
}
static __attribute__((noinline)) int32_t uflt_m2k_run(const uflt_t *f, int32_t *st, int32_t x)
{
    uflt_m2k_st_t *z = (uflt_m2k_st_t *)st;
    int32_t b = (int32_t)(((int64_t)x * f->gin) >> 16), lp, bp, ceil = f->ceil;   /* the machine's sample */
    b = b > 32767 ? 32767 : b < -32767 ? -32767 : b;
    int32_t v = (int32_t)(((int64_t)b * f->dh) >> 24);                         /* DIST (MONO2K's dist_in) */
    v = clamp(v, -(1 << 24), 1 << 24);
    m2k_ztick(&z->lp1, f->a1, f->a2, f->a3, v, &lp, &bp);
    v = clamp(lp, -ceil, ceil);                                                /* the clip */
    m2k_ztick(&z->hp2, f->b1, f->b2, f->b3, v, &lp, &bp);
    z->hp2.s1 = clamp(z->hp2.s1, -ceil, ceil);
    z->hp2.s2 = clamp(z->hp2.s2, -ceil, ceil);
    v = v - (int32_t)(((int64_t)f->hk * bp) >> 24) - lp;                        /* its high-pass */
    v -= m2k_olp(&z->hp1, f->g1, v);
    v = m2k_olp(&z->lp2b, f->g2, m2k_olp(&z->lp2a, f->g2, v));
    v = soft_knee((int32_t)(((int64_t)v * f->gout) >> 12), 16000);    /* (its ceiling's peaks: round, never flat) */
    return v > 32767 ? 32767 : v < -32767 ? -32767 : v;
}

/* MODE as it plays: a store of 1.3.7 or before (OFF LP BP HP NOT M2K) the nearest of today's: BP and HP HP, NOT and
 * M2K LP (eng_drum.c drm_upgrade puts it in the track, at every load) */
static inline int32_t uflt_mode(int32_t m) { return m == 3 ? UF_HP : m >= 4 ? UF_LP : m; }

/* this block's: MONO2K's chain, LP or HP (the voice's pitch, the machine, the level in and out) */
static void uflt_set(uflt_t *f, const int16_t *p, int32_t cut8, int32_t pitch16, uint32_t mach, int32_t gin, int32_t gain,
                     int32_t *drift, int32_t *nst, uint32_t seed)
{
    (void)drift; (void)nst; (void)seed;
    uflt_m2k_coef(f, cut8, p[P_FRES], p[P_FDIST] + 64, pitch16, mach, gin, gain, uflt_mode(p[P_FMODE]) == UF_HP);
}

/* one sample through it: st its two states (a voice's, a side's) */
static inline int32_t uflt_run(const uflt_t *f, int32_t *st, int32_t x)
{
    if (f->m2k) return uflt_m2k_run(f, st, x);
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
    static const struct { uint8_t eng, preset, mode, cut, res; int8_t dist; uint8_t glide; } F[] = {
        {ENGI_SAW, 0, UF_LP, 27, 104, 20, 0},   /* MONO2K's SAW and PULS sounds (1.3.6): their chains, DIST less 64 */
        {ENGI_SAW, 1, UF_LP, 27, 24, 0, 0},
        {ENGI_SAW, 2, UF_LP, 28, 24, 26, 0},
        {ENGI_SAW, 3, UF_LP, 33, 16, 0, 0},
        {ENGI_SAW, 4, UF_LP, 8, 20, 20, 0},
        {ENGI_SAW, 5, UF_LP, 44, 10, 0, 0},
        {ENGI_SAW, 6, UF_LP, 32, 20, 0, 0},
        {ENGI_PULS, 0, UF_LP, 24, 20, 0, 0},
        {ENGI_PULS, 1, UF_LP, 33, 20, 0, 30},  /* SQUARE: gliding */
        {ENGI_PULS, 2, UF_LP, 44, 16, 0, 0},
        {ENGI_PULS, 3, UF_LP, 45, 12, 0, 0},
        {ENGI_FMTONE, 6, 1, 33, 54, 0, 0},     /* RESO PLUCK: a plucked lead through a resonant low-pass the envelope opens */
        {ENGI_FMTONE, 7, 1, 42, 66, 0, 0},    /* FIFTH RING: a resonant low-pass ringing above the bell */
        {ENGI_FMTONE, 0, 1, 120, 0, 5, 0},
        {ENGI_FMTONE, 2, 1, 45, 68, 10, 0},
        {ENGI_FMTONE, 8, 1, 39, 38, 44, 0},
        {ENGI_FMTONE, 10, 1, 29, 15, 3, 0},
        {ENGI_FMTONE, 12, 2, 9, 112, -2, 0},
        {ENGI_FMTONE, 13, 1, 105, 34, 54, 0},
        {ENGI_FMTONE, 14, 1, 25, 6, -6, 0},
        {ENGI_FMTONE, 16, 2, 98, 0, 1, 0},
        {ENGI_FMTONE, 20, 1, 91, 5, -1, 0},
        {ENGI_FMTONE, 21, 1, 14, 47, 30, 0},   /* FMTONE's MONO2K sounds (1.3.8): their chains, fitted */
        {ENGI_DUAL, 5, 1, 33, 54, 0, 0},  /* (1.3.8: TRIO's cutoffs as steps over C3, its RES x 0.6) */       /* RESO LEAD */
        {ENGI_ANA4, 0, 1, 56, 18, 0, 0},      /* SAW BASS (filter 1 as Mini-Tone's, on this filter) */
        {ENGI_ANA4, 1, 1, 62, 12, 0, 0},      /* PWM LEAD */
        {ENGI_ANA4, 2, 1, 56, 15, 0, 0},      /* SYNC LEAD */
        {ENGI_ANA4, 3, 1, 53, 66, 0, 0},      /* ACID */
        {ENGI_ANA4, 4, 1, 48, 12, 0, 0},       /* WARM PAD */
    };
    for (uint32_t k = 0; k < NELEM(F); k++)
        if (F[k].eng == t->eng_req && F[k].preset == pi) {
            t->p[P_FMODE] = F[k].mode; t->p[P_FCUT] = F[k].cut; t->p[P_FRES] = F[k].res; t->p[P_FDIST] = F[k].dist;
            if (F[k].glide) t->p[P_GLIDE] = F[k].glide;
        }
}
