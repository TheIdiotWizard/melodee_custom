/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Sequencist (InigoHernandez), MONO2K: https://github.com/InigoHernandez/mono2k */
/* VOX: MONO2K's VOX-6 machine (MONO2K firmware/src/m2k_dsp.c, case M_VOX, GPL-3.0-only), after the Monomachine's VO-6
 * as measured on its OS: a cascade formant voice, a copy a voice (1.3.4). Its sample loop, laws and tables are MONO2K's
 * (vox_tables.h, extracted unchanged):
 *   an impulse a period (the first difference of a band-limited saw: a flat spectrum, its level the same at any pitch)
 *   and white noise, crossfaded by VOIC (0 voiced .. 127 whispered); a 188 Hz high-pass; four resonators in series,
 *   two-pole low-passes unity at DC: F1 on VOC1 (199 .. 702 Hz), F2 on VOC2 (457 .. 2757 Hz), F3 2495 Hz, F4 3527 Hz.
 *   Its pitch in whole hertz, rounded down, as the Monomachine's (TUNE +-2 semitones). Then a compressor, as the
 *   Monomachine's level says: the cascade's mean square followed (25 ms), the gain down at once and up at most 0.23 dB
 *   a ms, from a low gain at a fresh note (its fade-in, MONO2K's 4 x as fast); a legato note keeps the gain, its vowel
 *   gliding from the last (11.6 ms).
 * What changed for Melodee: a machine is a voice (Melodee's ADSR, velocity, glide and LFO play it); its 16-sample blocks
 * run two to Melodee's 32; the track's FLT moves VOC2 and SHP VOC1 (the LFO / ENV -> FLT a wah); VO-6's consonant knobs
 * (CONS CLEN CVOL) and V-SW, still to come in MONO2K, are left out. */
#include "vox_tables.h"

#define VOX_POLY 4u                  /* voices a part */
#define VOX_SUB 16u                  /* MONO2K's control block */
#define VO_INC_Q16 6382652533ULL     /* a phase increment a hertz, Q16 (2^48 / 44100) */
#define VO_R 10                      /* the compressor: the ratio */
#define VO_T 3143                    /* its level where the gain is unity (Q8 log2 of the cascade's rms) */
#define VO_UP 57                     /* its gain up at most 0.23 dB a ms: Q12 log2 a block */
#define VO_DET 475                   /* its detector, a one-pole on the mean square a block, Q15 (25 ms) */
#define VO_G0 (-16806)               /* its gain at a fresh note, Q12 log2 */
#define VO_UPT 228                   /* and up from there 4 x as fast (MONO2K 1.6.2) */
#define VO_GL 32                     /* a legato note's vowel glide: 11.6 ms, blocks */
#define VO_GLS 5
#define VO_NZ 1023                   /* its noise against its voice, Q12 */
#define VO_ZS_MAX (1 << 28)
#define VOX_GAIN 8192                /* its level against Melodee's other voices, Q12 (x 2: near ANALOG4's) */
_Static_assert(FS == 44100, "VO_INC_Q16: the sample rate");

typedef struct { int32_t s1, s2, a1, a2, a3, k; uint32_t r1, r2; } vx_svf_t;   /* a1 a2 a3 Q30, k Q24 */
typedef struct { int32_t s, G; uint32_t r; } vx_op_t;                          /* G Q24 */
typedef struct {
    uint32_t ph, lfsr;
    int32_t ms;                      /* the saw's last sample (the impulse: its difference) */
    vx_svf_t vo[4];                  /* F1 .. F4 in series */
    vx_op_t hp;                      /* the source's high-pass */
    int64_t pw;                      /* the compressor: the cascade's mean square, followed */
    int32_t g;                       /* its gain (Q12 log2) */
    int32_t v[2], f[2];              /* the vowel the last block (VOC1 VOC2, Q8), where a legato note's glides from */
    uint8_t in, gl;                  /* fading in from a fresh note; blocks left of a legato note's glide */
} vox_voice_t;
typedef struct { vox_voice_t v[VOX_POLY]; } vox_part_t;
static vox_part_t *vox_part_of(const track_t *t) { return resource_get(RES_ENGINE0 + (uint32_t)(t - trk), sizeof(vox_part_t)); }
static vox_voice_t *vox_voice_of(track_t *t, voice_t *v)
{
    uint32_t i = (uint32_t)(v - t->v);
    if (t < &trk[0] || t >= &trk[NPART] || i >= VOX_POLY) return 0;
    return &vox_part_of(t)->v[i];
}

/* a b + c d, out of line on the FM-1 (M2K_MUL_FN: its 64-bit multiplies, as MONO2K's FM-1 build) */
M2K_MUL_FN uint64_t vx_dot_x4(int32_t a, int32_t b, int32_t c, int32_t d) { return (uint64_t)((int64_t)a * b + (int64_t)c * d) << 2; }
static inline int64_t vx_dot(int32_t a, int32_t b, int32_t c, int32_t d) { return (int64_t)vx_dot_x4(a, b, c, d) >> 2; }

/* MONO2K's zsvf_set: g, k Q24; one divide, a Newton step to Q30 */
static void vx_svf_set(vx_svf_t *f, uint32_t g, uint32_t k)
{
    int32_t den = (1 << 24) + (int32_t)m2k_mul_sh((int32_t)g, (int32_t)(g + k), 24);
    int32_t a1 = (int32_t)((0x80000000u / (uint32_t)(den >> 10)) << 13);
    int32_t e = (1 << 30) + (int32_t)m2k_mul_sh(-den, a1, 24);
    a1 += (int32_t)m2k_mul_sh(a1, e, 30);
    f->a1 = a1;
    f->a2 = (int32_t)m2k_mul_sh((int32_t)g, a1, 24);
    f->a3 = (int32_t)m2k_mul_sh((int32_t)g, f->a2, 24);
    f->k = (int32_t)k;
}
/* its low-pass (MONO2K's zsvf_tick, the remainders carried) */
static inline int32_t vx_svf_lp(vx_svf_t *f, int32_t v0)
{
    int32_t v3 = v0 - f->s2;
    int64_t p1 = vx_dot(f->a1, f->s1, f->a2, v3) + f->r1;
    int64_t p2 = vx_dot(f->a2, f->s1, f->a3, v3) + f->r2;
    int32_t v1 = (int32_t)(p1 >> 30), d2 = (int32_t)(p2 >> 30);
    f->r1 = (uint32_t)p1 & ((1u << 30) - 1);
    f->r2 = (uint32_t)p2 & ((1u << 30) - 1);
    int32_t v2 = f->s2 + d2;
    f->s1 = M2K_CLAMP(2 * v1 - f->s1, -VO_ZS_MAX, VO_ZS_MAX);
    f->s2 = M2K_CLAMP(2 * v2 - f->s2, -VO_ZS_MAX, VO_ZS_MAX);
    return v2;
}
static inline int32_t vx_op_lp(vx_op_t *f, int32_t x)
{
    int64_t p = m2k_mul_sh(x - f->s, f->G, 0) + f->r;
    int32_t v = (int32_t)(p >> 24);
    f->r = (uint32_t)p & ((1u << 24) - 1);
    int32_t y = v + f->s;
    f->s = M2K_CLAMP(y + v, -VO_ZS_MAX, VO_ZS_MAX);
    return y;
}
/* log2 in Q8 of x (x >= 1) */
static int32_t vx_log2_q8(uint32_t x)
{
    if (!x) return 0;
    int p = 31;
    while (!(x >> p)) p--;
    uint32_t m = p >= 8 ? (x >> (p - 8)) & 255 : (x << (8 - p)) & 255;
    return p * 256 + T_LOG2M[m];
}

/* a fresh note: from near silence, the compressor's gain low (its fade-in); a voice taken from silence starts clean */
static void vox_note_on(track_t *t, voice_t *v)
{
    vox_voice_t *s = vox_voice_of(t, v);
    if (!s) return;
    if (!voice_was) {
        memset(s, 0, sizeof *s);
        s->lfsr = 0x2545F491u ^ (uint32_t)(t - trk) * 7919u ^ (uint32_t)(v - t->v) * 104729u;
    }
    s->g = VO_G0;
    s->in = 1;
    s->gl = 0;
}
/* a legato note (MONO / LEGATO / UNISON): the level kept, the new vowel gliding in from the last */
static void vox_legato(track_t *t, voice_t *v)
{
    vox_voice_t *s = vox_voice_of(t, v);
    if (!s) return;
    s->f[0] = s->v[0];
    s->f[1] = s->v[1];
    s->gl = VO_GL;
}

static void vox_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    vox_voice_t *s = vox_voice_of(t, v);
    const int16_t *p = t->p;
    if (!s) return;
    int32_t q1 = M2K_CLAMP(p[P_E0] * 256 + m->shape - (64 << 8), 0, 127 * 256);   /* VOC1 (+ SHP), Q8 */
    int32_t q2 = M2K_CLAMP(p[P_E1] * 256 + m->cutoff, 0, 127 * 256);             /* VOC2 (+ FLT) */
    int32_t vc = M2K_CLAMP(p[P_E2], 0, 127) * 256;
    uint32_t hz = m2k_mulu_hi(m2k_inc(m->inc, (p[P_E3] - 64) * 2), FS, 32);       /* TUNE: 1/64 st a step, +-2 st */
    if (hz < 8) hz = 8;
    uint32_t inc = (uint32_t)m2k_mul64_sh((int64_t)VO_INC_Q16, (int32_t)hz, 16), inv = 0xFFFFFFFFu / inc;
    int32_t sg = (int32_t)m2k_exp2((vx_log2_q8(130) - vx_log2_q8(hz)) >> 1);      /* Q16: sqrt(130 / hz) */
    int32_t gv = (int32_t)m2k_mul_sh(m2k_tq16(T_VO_GV, vc), sg, 16), gn = (m2k_tq16(T_VO_GN, vc) * VO_NZ) >> 12;
    for (uint32_t b = 0; b < n; b += VOX_SUB) {
        int32_t v1 = q1, v2 = q2;
        if (s->gl) {                 /* a legato note: from the last vowel to this one, straight */
            int32_t k = VO_GL + 1 - s->gl--;
            v1 = s->f[0] + (((v1 - s->f[0]) * k) >> VO_GLS);
            v2 = s->f[1] + (((v2 - s->f[1]) * k) >> VO_GLS);
        }
        s->v[0] = v1;
        s->v[1] = v2;
        vx_svf_set(&s->vo[0], m2k_tq32(T_VO_G1, v1), m2k_tq32(T_VO_K1, v1));
        vx_svf_set(&s->vo[1], m2k_tq32(T_VO_G2, v2), m2k_tq32(T_VO_K2, v2));
        vx_svf_set(&s->vo[2], T_VO_GK34[0], T_VO_GK34[1]);
        vx_svf_set(&s->vo[3], T_VO_GK34[2], T_VO_GK34[3]);
        s->hp.G = VO_HP_G;
        /* the compressor: its target from the cascade's level so far, the gain to it at once down, slewed up */
        int32_t lv = (vx_log2_q8((uint32_t)((s->pw >> 16) | 1)) + 16 * 256) >> 1;
        int32_t gt = ((VO_T - lv) * (VO_R - 1) * 16) / VO_R;
        int32_t up = s->in ? VO_UPT : VO_UP;
        s->g = gt < s->g ? gt : s->g + up < gt ? s->g + up : gt;
        if (s->g == gt) s->in = 0;
        uint32_t cg = m2k_exp2(s->g >> 4);                                     /* Q16 */
        int64_t pw = 0;
        for (uint32_t i = 0; i < VOX_SUB && b + i < n; i++) {
            int32_t sw = m2k_blep_saw(s->ph, inc, inv);
            s->ph += inc;
            int32_t im = sw - s->ms;                                          /* an impulse a period, band-limited */
            s->ms = sw;
            int32_t nz = (int32_t)m2k_noise(&s->lfsr) >> 16;
            int32_t x = (int32_t)m2k_mul_sh(im, gv, 15) + ((nz * gn) >> 15);
            x -= vx_op_lp(&s->hp, x);
            int32_t y = vx_svf_lp(&s->vo[0], x);
            y = vx_svf_lp(&s->vo[1], y);
            y = vx_svf_lp(&s->vo[2], y);
            y = vx_svf_lp(&s->vo[3], y);
            pw += m2k_mul_sh(y, y, 0);
            int32_t o = m2k_sat16((int32_t)m2k_mul_sh(y, (int32_t)cg, 16));
            out[b + i] += voice_amp(m2k_sat16((o * VOX_GAIN) >> 12), m, b + i);
        }
        s->pw = s->pw ? s->pw + m2k_mul64_sh((pw >> 4) - s->pw, VO_DET, 15) : (pw >> 4) | 1;
    }
}

/* {VOC1, VOC2, VOIC, TUNE}: the vowels at the nearest of their formants (Hillenbrand et al., 1995, adult male means:
 * /a/ 768 1333, /E/ 580 1799, /i/ 342 2322, /o/ 497 910, /u/ 378 997 Hz; F1's top is 702) */
static const preset_t VOX_PRESETS[] = {
    {"AAH", {127, 74, 0, 64, 0, 0, 0, 0}, {6, 70, 120, 50}, 0, 0},
    {"EH", {109, 97, 0, 64, 0, 0, 0, 0}, {6, 70, 120, 50}, 0, 0},
    {"EE", {58, 116, 0, 64, 0, 0, 0, 0}, {6, 70, 120, 50}, 0, 0},
    {"OH", {91, 45, 0, 64, 0, 0, 0, 0}, {6, 70, 120, 50}, 0, 0},
    {"OO", {65, 52, 0, 64, 0, 0, 0, 0}, {6, 70, 120, 50}, 0, 0},
    {"WAH LEAD", {100, 40, 10, 64, 0, 0, 0, 0}, {0, 60, 110, 30}, 90, 1},
    {"WHISPER", {91, 80, 110, 64, 0, 0, 0, 0}, {40, 90, 110, 80}, 0, 0},
};

static const engine_t ENG_VOX = {
    .name = "VOX",
    .page_title = {"VOX-6", "-"},
    .edit = {{"VOC1", F_INT, 0, 127, 64, 0, 0}, {"VOC2", F_INT, 0, 127, 64, 0, 0}, {"VOIC", F_INT, 0, 127, 0, 0, 0},
             M2K_BI("TUNE"),
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}},
    .presets = VOX_PRESETS,
    .npresets = NELEM(VOX_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = VOX_POLY,
    .note_on = vox_note_on,
    .render = vox_render,
    .legato = vox_legato,
};
