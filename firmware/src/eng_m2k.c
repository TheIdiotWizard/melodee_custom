/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Sequencist (InigoHernandez), MONO2K: https://github.com/InigoHernandez/mono2k */
/* SAW, ENS, STAT, PAR, DYN and WAV: MONO2K's SWAV-SAW, SWAV-ENS, FM+STAT, FM+PAR, FM+DYN and DIGI-WAV machines
 * (MONO2K firmware/src/m2k_dsp.c mach_block, GPL-3.0-only), each voice of a part running its own copy. The machines'
 * sample loops, laws and tables are MONO2K's (m2k_tables.h, extracted unchanged); what changed for Melodee:
 *  - a machine is a voice: Melodee's ADSR, velocity, glide, unison and LFO play it (MONO2K's own track chain, its
 *    filters and amp, is not part of it); its pitch is the voice's (tuning, bend, fine detune), TUNE and the
 *    intervals a ratio on top;
 *  - its 16-sample blocks run two to Melodee's 32 (their per-block laws: MONO2K's own);
 *  - ENS's chorus is the part's (eng_m2k_post: one line over all its voices, CHRW across SPREAD's two sides);
 *  - WAV plays the owner's 64 waves (MONO2K's WAVE bank 64; in the resource region, res.c: silent without it), its
 *    sync master a free phase per voice;
 *  - the FM ratios and ENS's intervals are lists (an index each: the names MONO2K shows);
 *  - FLT (Melodee's LFO / ENV to the filter) and SHP move one control each, named in the table below.
 * The 32 x 32 -> 64 multiplies sit in out-of-line helpers on the device (M2K_MUL_OUT, as MONO2K's FM-1 build). */
#include "m2k_tables.h"

#define M2K_POLY 6u                  /* voices a part (POLY, UNISON) */
#define M2K_ENS_POLY 3u              /* ENS: up to four saw pairs a voice */
#define M2K_SUB 16u                  /* MONO2K's control block */
#define M2K_ENS_LEN 1024u            /* ENS's chorus line: 46 ms at half rate */
#define M2K_ENS_GAIN 7119            /* a voice against the others' levels (MONO2K: calibrated, chain_ens20) */
#define M2K_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
enum { M2K_SAW, M2K_ENS, M2K_STAT, M2K_PAR, M2K_DYN, M2K_WAV };

#if defined(FM1_IRQ_TARGET)
#define M2K_MUL_FN static __attribute__((noinline))
#else
#define M2K_MUL_FN static inline
#endif
M2K_MUL_FN int64_t m2k_mul_sh(int32_t a, int32_t b, int sh) { return ((int64_t)a * b) >> sh; }
M2K_MUL_FN uint32_t m2k_mulu_hi(uint32_t a, uint32_t b, int sh) { return (uint32_t)(((uint64_t)a * b) >> sh); }
M2K_MUL_FN int64_t m2k_mul64_sh(int64_t a, int32_t b, int sh) { return (a * b) >> sh; }
M2K_MUL_FN uint64_t m2k_mul2_q15(int32_t s, int32_t a, int32_t b)
{
    return (uint64_t)(uint32_t)(((int64_t)s * b) >> 15) << 32 | (uint32_t)(((int64_t)s * a) >> 15);
}
static inline int32_t m2k_sat16(int32_t x) { return x > 32767 ? 32767 : x < -32767 ? -32767 : x; }
static inline int32_t m2k_sinp(uint32_t ph)
{
    uint32_t i = ph >> 22, f = (ph >> 6) & 0xFFFFu;
    int32_t a = T_SIN[i], b = T_SIN[i + 1];
    return a + (((b - a) * (int32_t)f) >> 16);
}
static inline uint32_t m2k_noise(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x; return x; }
static inline int32_t m2k_blep_saw(uint32_t ph, uint32_t inc, uint32_t inv)
{
    int32_t s = (int32_t)(ph >> 16) - 32768;
    if (ph < inc) {
        uint32_t t = (ph * inv) >> 16;
        int32_t p = 2 * (int32_t)t - (int32_t)((t * t) >> 16) - 65536;
        s -= p >> 1;
    } else if (~ph < inc) {
        uint32_t u = (~ph * inv) >> 16, w = 65536 - u;
        s -= (int32_t)((w * w) >> 16) >> 1;
    }
    return s;
}
static uint32_t m2k_tq_mid(const uint32_t *T, int32_t q8)
{
    uint32_t a = T[q8 >> 8], b = T[(q8 >> 8) + 1], f = (uint32_t)q8 & 255u;
    return b >= a ? a + m2k_mulu_hi(b - a, f, 8) : a - m2k_mulu_hi(a - b, f, 8);
}
static inline uint32_t m2k_tq32(const uint32_t *T, int32_t q8) { return q8 & 255 ? m2k_tq_mid(T, q8) : T[q8 >> 8]; }
static inline int32_t m2k_tq16(const uint16_t *T, int32_t q8)
{
    int32_t a = T[q8 >> 8], f = q8 & 255;
    return f ? a + ((((int32_t)T[(q8 >> 8) + 1] - a) * f) >> 8) : a;
}
/* 2^(y / 256), Q16 */
static inline uint32_t m2k_exp2(int32_t y)
{
    int32_t i = y >> 8;
    uint32_t f = T_EXP2F[y & 255];
    if (i >= 15) return 0x7FFFFFFFu;
    if (i < -16) return 0;
    return i >= 0 ? f << i : f >> -i;
}
/* the voice's increment moved by o 1/64 semitones (TUNE, an interval, the drift) */
static inline uint32_t m2k_inc(uint32_t inc, int32_t o) { return o ? m2k_mulu_hi(inc, m2k_exp2(o / 3), 16) : inc; }
/* FM+DYN's depth at a point of its law (Q8 steps, 0..256) */
static inline uint32_t m2k_fmd_at(int32_t v8)
{
    int32_t i = v8 >> 8;
    if (i >= 256) return T_FMDV[256];
    return T_FMDV[i] + (((T_FMDV[i + 1] - T_FMDV[i]) * (uint32_t)(v8 & 255)) >> 8);
}

/* a voice's machine state, as MONO2K's track's */
typedef struct {
    uint32_t ph[7];
    int32_t ms[6];
    uint32_t menv[3];                /* Q30 envelopes from the note */
    uint32_t drng;                   /* SAW's drift */
} m2k_voice_t;
typedef struct {                     /* ENS: the part's chorus line, after its voices */
    int32_t line[M2K_ENS_LEN];
    uint32_t w, lph;                 /* the write index, the chorus's sine */
    int32_t acc;                     /* a sample pair's sum (half rate) */
} m2k_chorus_t;
typedef struct {
    m2k_voice_t v[M2K_POLY];
    m2k_chorus_t ch;                 /* (ENS only: eng_state_size) */
} m2k_part_t;
#define M2K_PART_BYTES(kind) ((kind) == M2K_ENS ? sizeof(m2k_part_t) : __builtin_offsetof(m2k_part_t, ch))
static int m2k_kind(const track_t *t);         /* engines.c: the machine of a track's engine */
static m2k_part_t *m2k_part_of(const track_t *t) { return resource_get(RES_ENGINE0 + (uint32_t)(t - trk), M2K_PART_BYTES(m2k_kind(t))); }

/* the controls with their modulation, Q8 (0 .. 127 x 256); FLT moves control flt, SHP control shp */
static void m2k_params(const track_t *t, const vmod_t *m, int32_t *q, int flt, int shp)
{
    for (uint32_t k = 0; k < 8u; k++) q[k] = (int32_t)t->p[P_E0 + k] * 256;
    if (flt >= 0) q[flt] += m->cutoff;
    if (shp >= 0) q[shp] += m->shape - (64 << 8);
    for (uint32_t k = 0; k < 8u; k++) q[k] = M2K_CLAMP(q[k], 0, 127 * 256);
}
#define Q7(k) (q[k] >> 8)
#define QB(k) (q[k] - 64 * 256)

/* SWAV-SAW: UNIL UNIW UNIX DRFT SUBX SUB1 SUB2 TUNE (MONO2K's note: its saw against GND-SIN's sine at 0.158) */
static void m2k_saw(m2k_voice_t *s, const int32_t *q, uint32_t base, int32_t note, int16_t *o, uint32_t seed)
{
    int32_t unil = q[0], uniw = q[1], drift = q[3];
    int32_t off = QB(7) >> 7;
    if (drift) {
        if (!s->drng) s->drng = 0x6C8E9CF5u ^ (seed + 1u) * 0x9E3779B9u;
        s->ms[4] += (((int32_t)(m2k_noise(&s->drng) >> 20) - 2048) * drift) >> 17;
        s->ms[4] = M2K_CLAMP(s->ms[4], -(drift >> 5), drift >> 5);
        off += s->ms[4] >> 6;
    }
    uint32_t inc[5], inv[5];
    inc[0] = m2k_inc(base, off);
    inc[1] = m2k_mulu_hi(inc[0], m2k_tq32(T_SW1LO, uniw), 24);
    inc[2] = m2k_mulu_hi(inc[0], m2k_tq32(T_SW1HI, uniw), 24);
    inc[3] = m2k_mulu_hi(inc[0], m2k_tq32(T_SW2LO, uniw), 24);
    inc[4] = m2k_mulu_hi(inc[0], m2k_tq32(T_SW2HI, uniw), 24);
    int nv = unil ? 5 : 1;
    for (int k = 0; k < nv; k++) inv[k] = 0xFFFFFFFFu / (inc[k] | 1);
    int32_t g1 = 5166 * unil / (127 * 256), g2 = (g1 * q[2]) / (127 * 256);
    int32_t gx = (3369 * m2k_tq16(T_SWSUBX, q[4])) >> 15;
    int32_t gs1 = 7097 * q[5] / (127 * 256), gs2 = 2701 * q[6] / (127 * 256);
    uint32_t ih = inc[0] >> 1, invh = 0xFFFFFFFFu / (ih | 1);
    int32_t a = T_SWHP[M2K_CLAMP(note + (off >> 6), 0, 127)];
    for (uint32_t i = 0; i < M2K_SUB; i++) {
        int32_t x = m2k_blep_saw(s->ph[0], inc[0], inv[0]) * 5166;
        if (unil) {
            x += (m2k_blep_saw(s->ph[1], inc[1], inv[1]) + m2k_blep_saw(s->ph[2], inc[2], inv[2])) * g1;
            if (g2) x += (m2k_blep_saw(s->ph[3], inc[3], inv[3]) + m2k_blep_saw(s->ph[4], inc[4], inv[4])) * g2;
        }
        x >>= 15;
        if (gx) x += ((m2k_blep_saw(s->ph[5], ih, invh) - m2k_blep_saw(s->ph[5] + 0x80000000u, ih, invh)) * gx) >> 15;
        for (int k = 0; k < nv; k++) s->ph[k] += inc[k];
        s->ms[0] = (int32_t)m2k_mul_sh(a, s->ms[0] + x - s->ms[1], 15);
        s->ms[1] = x;
        int32_t y = s->ms[0];
        if (gs1) y += (m2k_sinp(s->ph[5]) * gs1) >> 15;
        if (gs2) y += (m2k_sinp(s->ph[6]) * gs2) >> 15;
        s->ph[5] += ih;
        s->ph[6] += inc[0] >> 2;
        o[i] = (int16_t)m2k_sat16(y);
    }
}

/* SWAV-ENS's voices: PCH2 PCH3 PCH4 WAVE PW CHRL CHRW TUNE (the chorus: m2k_post). The root and up to three voices
 * off its intervals, each two saws an octave under its note, the second PW / 128 of a period on */
static void m2k_ens(m2k_voice_t *s, const int32_t *q, const int16_t *p, uint32_t base, int16_t *o)
{
    int32_t root = (QB(7) >> 7) - 12 * 64;
    int32_t w8 = q[3];
    int32_t wa = 32768 - (w8 * 12124) / (127 * 256), wb = 32768 - (w8 * 53412) / (127 * 256);
    uint32_t off = (uint32_t)q[4] << 17;
    uint32_t inc[4], inv[4];
    for (int k = 0; k < 4; k++) {
        int32_t iv = 0;
        if (k) {                     /* PCH: one of T_ENS_PNAME's 33 (16 OFF) */
            uint32_t x = (uint32_t)M2K_CLAMP(p[P_E0 + k - 1], 0, 32), j = 0;
            while (j < 127u && T_ENS_PIDX[j] < x) j++;
            iv = T_ENS_PCH[j];
        }
        inc[k] = iv == ENS_OFF ? 0 : m2k_inc(base, root + iv);
        inv[k] = 0xFFFFFFFFu / (inc[k] | 1);
    }
    for (uint32_t i = 0; i < M2K_SUB; i++) {
        int32_t x = 0;
        for (int k = 0; k < 4; k++) {
            if (!inc[k]) continue;
            x += (m2k_blep_saw(s->ph[k], inc[k], inv[k]) * wa + m2k_blep_saw(s->ph[k] + off, inc[k], inv[k]) * wb) >> 15;
            s->ph[k] += inc[k];
        }
        o[i] = (int16_t)m2k_sat16((x * M2K_ENS_GAIN) >> 15);
    }
}

/* FM+STAT: 1FRQ 1FIN 1ENV 1FB 2FRQ 2VOL TONE TUNE (1FRQ, 2FRQ: T_FMR24_NAME's 24) */
static void m2k_stat(m2k_voice_t *s, const int32_t *q, const int16_t *p, uint32_t base, int16_t *o)
{
    uint32_t inc = m2k_inc(base, QB(7) >> 7);
    uint32_t r1 = m2k_mulu_hi(T_FMR24[M2K_CLAMP(p[P_E0], 0, 23)], 192 * 256 + (uint32_t)q[1], 16);
    uint32_t r2 = T_FMR24[M2K_CLAMP(p[P_E4], 0, 23)];
    uint32_t i1 = m2k_mulu_hi(inc, r1, 16), i2 = m2k_mulu_hi(inc, r2, 16);
    int32_t env = q[2];
    uint32_t k1 = m2k_tq32(T_FMB, env < 64 * 256 ? env : 64 * 256);
    if (env >= 65 * 256) {
        k1 = (T_FMB[64] * (s->menv[0] >> 15)) >> 15;
        s->menv[0] = m2k_mulu_hi(s->menv[0], m2k_tq32(T_FMEC, env - 64 * 256), 31);
    }
    uint32_t k2 = m2k_tq32(T_FMB, q[5]);
    int32_t lp = m2k_tq16(T_FMLP, q[6]);
    uint32_t kf = m2k_tq32(T_FMFB, q[3]);
    for (uint32_t i = 0; i < M2K_SUB; i++) {
        int32_t m1 = m2k_sinp(s->ph[1] + (uint32_t)((s->ms[0] + s->ms[1]) >> 1) * kf);
        s->ms[1] = s->ms[0];
        s->ms[0] = m1;
        uint64_t d = m2k_mul2_q15(lp, m1 * 256 - s->ms[2], m2k_sinp(s->ph[2]) * 256 - s->ms[4]);
        s->ms[2] += (int32_t)d;
        s->ms[4] += (int32_t)(d >> 32);
        d = m2k_mul2_q15(lp, s->ms[2] - s->ms[3], s->ms[4] - s->ms[5]);
        s->ms[3] += (int32_t)d;
        s->ms[5] += (int32_t)(d >> 32);
        uint32_t pm = (uint32_t)m2k_mul_sh(s->ms[3], (int32_t)k1, 8) + (uint32_t)m2k_mul_sh(s->ms[5], (int32_t)k2, 8);
        o[i] = (int16_t)(m2k_sinp(s->ph[0] + pm) >> 1);
        s->ph[0] += inc;
        s->ph[1] += i1;
        s->ph[2] += i2;
    }
}

/* FM+DYN: 1FRQ 1FEN 1VOL 1VEN 2FRQ 2ENV 2FB TUNE */
static void m2k_dyn(m2k_voice_t *s, const int32_t *q, uint32_t base, int16_t *o)
{
    uint32_t inc = m2k_inc(base, QB(7) >> 7);
    int32_t f1 = q[0], f2 = q[4];
    int32_t r1 = (f1 << 2) + (f1 > 126 * 256 ? (f1 - 126 * 256) << 2 : 0);
    uint32_t r2 = f2 <= 126 * 256 ? ((uint32_t)f2 * (uint32_t)f2) >> 12
                                  : (126u * 126u << 4) + ((508u << 4) * (uint32_t)(f2 - 126 * 256) >> 8);
    int32_t fen = q[1], ven = q[3], env = q[5];
    int64_t b1 = inc;
    if (fen != 64 * 256) {
        int32_t fe = T_FMFEN[fen >> 8];
        if (fen & 255) fe += (int32_t)m2k_mul_sh(T_FMFEN[(fen >> 8) + 1] - fe, fen & 255, 8);
        b1 += m2k_mul_sh(fe, (int32_t)(s->menv[1] >> 15), 15);
        s->menv[1] = m2k_mulu_hi(s->menv[1], m2k_tq32(T_FMDEC, fen > 64 * 256 ? fen - 64 * 256 : 64 * 256 - fen), 31);
    }
    uint32_t i1 = (uint32_t)(int32_t)m2k_mul64_sh(b1, r1, 16), i2 = m2k_mulu_hi(inc, r2, 16);
    int32_t vol8 = q[2];
    int32_t k1 = (int32_t)m2k_fmd_at(vol8);
    if (ven > 64 * 256) {
        int32_t e8 = ((ven < 96 * 256 ? 4 * (ven - 64 * 256) : 128 * 256) * (int32_t)(s->menv[0] >> 15)) >> 15;
        s->menv[0] = m2k_mulu_hi(s->menv[0], m2k_tq32(T_FMDEC, ven - 64 * 256), 31);
        if (vol8 + e8 >= 64 * 256) {
            k1 = (int32_t)m2k_fmd_at(vol8 + e8);
        } else {
            k1 += (int32_t)((T_FMDV[64] * (uint32_t)e8) >> 13);
            if (k1 > (int32_t)T_FMDV[64]) k1 = (int32_t)T_FMDV[64];
        }
    } else if (ven < 64 * 256) {
        int32_t c = ((64 * 256 - ven) * (int32_t)(s->menv[0] >> 15)) >> 13;
        s->menv[0] = m2k_mulu_hi(s->menv[0], m2k_tq32(T_FMDEC, 64 * 256 - ven), 31);
        int32_t f = 32768 - c;
        int32_t v8 = ((f < 0 && vol8 > 64 * 256 ? 64 * 256 : vol8) * f) >> 15;
        if (v8 >= 64 * 256) {
            k1 = (int32_t)m2k_fmd_at(v8);
        } else if (vol8 >= 64 * 256) {
            int32_t h = vol8 - 64 * 256;
            int32_t z = 32 * 256 - (int32_t)m2k_mul_sh(h * h, 2113, 26);
            k1 = ((int32_t)T_FMDV[64] * (v8 - z)) >> 13;
            if (k1 > (int32_t)T_FMDV[64]) k1 = (int32_t)T_FMDV[64];
        } else {
            k1 = (int32_t)m2k_mul_sh(k1, 2 * f - 32768, 15);
        }
    }
    uint32_t k2 = m2k_fmd_at(env < 64 * 256 ? env : 64 * 256);
    if (env >= 65 * 256) {
        k2 = (T_FMDV[64] * (s->menv[2] >> 15)) >> 15;
        s->menv[2] = m2k_mulu_hi(s->menv[2], m2k_tq32(T_FMEC, env - 64 * 256), 31);
    }
    uint32_t kf = m2k_tq32(T_FMFB, q[6]);
    for (uint32_t i = 0; i < M2K_SUB; i++) {
        int32_t m2 = m2k_sinp(s->ph[2] + (uint32_t)((s->ms[0] + s->ms[1]) >> 1) * kf);
        s->ms[1] = s->ms[0];
        s->ms[0] = m2;
        uint32_t pm = (uint32_t)m2k_sinp(s->ph[1]) * (uint32_t)k1 + (uint32_t)m2 * k2;
        o[i] = (int16_t)(m2k_sinp(s->ph[0] + pm) >> 1);
        s->ph[0] += inc;
        s->ph[1] += i1;
        s->ph[2] += i2;
    }
}

/* FM+PAR: 1FRQ 1ENV 2FRQ 2ENV 3FRQ 3ENV TONE TUNE (the FRQs: T_FMR24_NAME's 24) */
static void m2k_par(m2k_voice_t *s, const int32_t *q, const int16_t *p, uint32_t base, int16_t *o)
{
    uint32_t inc = m2k_inc(base, QB(7) >> 7);
    uint32_t ii[3], kk[3];
    for (int j = 0; j < 3; j++) {
        ii[j] = m2k_mulu_hi(inc, T_FMR24[M2K_CLAMP(p[P_E0 + 2 * j], 0, 23)], 16);
        int32_t env = q[2 * j + 1];
        uint32_t k = m2k_tq32(T_FMB, env);
        if (env >= 65 * 256) {
            k = m2k_mulu_hi(k, s->menv[j] >> 15, 15);
            s->menv[j] = m2k_mulu_hi(s->menv[j], m2k_tq32(T_FMEC, env - 64 * 256), 31);
        }
        kk[j] = k;
    }
    int32_t lp = m2k_tq16(T_FMLP, q[6]);
    for (uint32_t i = 0; i < M2K_SUB; i++) {
        uint32_t pm = 0;
        for (int j = 0; j < 3; j++) {
            int32_t *a = &s->ms[2 * j];
            a[0] += (int32_t)m2k_mul_sh(m2k_sinp(s->ph[1 + j]) * 256 - a[0], lp, 15);
            a[1] += (int32_t)m2k_mul_sh(a[0] - a[1], lp, 15);
            pm += (uint32_t)m2k_mul_sh(a[1], (int32_t)kk[j], 8);
            s->ph[1 + j] += ii[j];
        }
        o[i] = (int16_t)(m2k_sinp(s->ph[0] + pm) >> 1);
        s->ph[0] += inc;
    }
}

/* DIGI-WAV on the owner's 64: WAVE SCAN STIM SYNC BITS DTUN MIX2 STEP */
static void m2k_wav(m2k_voice_t *s, const int32_t *q, uint32_t base, int16_t *o)
{
    if (!RES_OK()) {                 /* (res.c: the waves not installed) */
        for (uint32_t i = 0; i < M2K_SUB; i++) o[i] = 0;
        return;
    }
    int32_t env = (int32_t)(s->menv[0] >> 15);
    if (q[2] < 127 * 256) s->menv[0] = m2k_mulu_hi(s->menv[0], m2k_tq32(T_ENV_COEF, q[2]), 31);
    int32_t scan = (QB(1) * 2 * env) >> 15;
    int32_t pos = (q[0] >> 1) + scan * 63 / 128;
    pos = M2K_CLAMP(pos, 0, 63 * 256);
    int ia = pos >> 8, ib = M2K_CLAMP(ia + 1, 0, 63);
    const int16_t *wa = &T_WAVES64[ia * 256], *wb = &T_WAVES64[ib * 256];
    int32_t dca = T_WAVES64_DC[ia], dcb = T_WAVES64_DC[ib], ga = T_WAVES64_G[ia], gb = T_WAVES64_G[ib];
    int32_t dc = dca + (((dcb - dca) * (pos & 255)) >> 8), g = ga + (((gb - ga) * (pos & 255)) >> 8);
    int32_t wf = pos & 255;
    uint32_t inc = base;
    uint32_t sinc = m2k_mulu_hi(inc, 256 + (uint32_t)((q[3] * 14) >> 8), 8);
    uint32_t inc2 = m2k_inc(base, QB(5) >> 5);
    int bits = 16 - (Q7(4) * 14 >> 7);
    int32_t mix2 = m2k_tq16(T_VOL, q[6]);
    int stp = Q7(7) * 6 >> 7;
    uint32_t qmask = stp ? ~((1u << (24 + stp)) - 1) : 0xFFFFFFFFu;
    for (uint32_t i = 0; i < M2K_SUB; i++) {
        uint32_t pm = s->ph[2];
        s->ph[2] += inc;
        if (s->ph[2] < pm) s->ph[0] = 0;
        uint32_t ph = s->ph[0] & qmask;
        int32_t j = (int32_t)(ph >> 24), f = stp ? 0 : (int32_t)((ph >> 16) & 255);
        int32_t a = wa[j] + (((wa[(j + 1) & 255] - wa[j]) * f) >> 8);
        int32_t b = wb[j] + (((wb[(j + 1) & 255] - wb[j]) * f) >> 8);
        int32_t x = a + (((b - a) * wf) >> 8);
        x = ((x - dc) * g) >> 14;
        if (mix2) {
            uint32_t q2 = s->ph[1] & qmask;
            int32_t j2 = (int32_t)(q2 >> 24);
            x += ((((wa[j2] - dca) * ga) >> 14) * mix2) >> 15;
            x = (x * 3) >> 2;
        }
        s->ph[0] += sinc;
        s->ph[1] += inc2;
        if (bits < 16) x = (x >> (16 - bits)) * (1 << (16 - bits));
        o[i] = (int16_t)m2k_sat16(x);
    }
}

/* each machine's level into the voice (Q12: a voice's peak about 0.9 of full scale, no clip) and the
 * controls FLT and SHP move */
static const struct { int16_t gain; int8_t flt, shp; } M2K_ROLE[6] = {
    [M2K_SAW] = {10240, 0, 1},       /* FLT UNIL, SHP UNIW */
    [M2K_ENS] = {6144, 3, 4},        /* FLT WAVE, SHP PW */
    [M2K_STAT] = {7400, 6, 5},       /* FLT TONE, SHP 2VOL */
    [M2K_PAR] = {7400, 6, 1},        /* FLT TONE, SHP 1ENV */
    [M2K_DYN] = {7400, 2, 5},        /* FLT 1VOL, SHP 2ENV */
    [M2K_WAV] = {4500, 1, 0},        /* FLT SCAN, SHP WAVE */
};

static m2k_voice_t *m2k_voice_of(track_t *t, voice_t *v)
{
    uint32_t i = (uint32_t)(v - t->v);
    if (t < &trk[0] || t >= &trk[NPART] || i >= M2K_POLY) return 0;
    return &m2k_part_of(t)->v[i];
}

static void m2k_note_on(track_t *t, voice_t *v)
{
    m2k_voice_t *s = m2k_voice_of(t, v);
    int kind = m2k_kind(t);
    if (!s) return;
    uint32_t keep = s->ph[2], drng = s->drng;
    memset(s, 0, sizeof *s);
    s->menv[0] = s->menv[1] = s->menv[2] = 1u << 30;
    if (kind >= M2K_STAT && kind <= M2K_DYN) s->ph[0] = 1u << 31;   /* FM+'s carrier half a cycle on */
    if (kind == M2K_WAV && voice_was == 2u) s->ph[2] = keep;        /* a steal: the sync's master runs on */
    s->drng = drng;
}

static void m2k_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    m2k_voice_t *s = m2k_voice_of(t, v);
    int kind = m2k_kind(t);
    int32_t q[8];
    int16_t o[M2K_SUB];
    if (!s) return;
    m2k_params(t, m, q, M2K_ROLE[kind].flt, M2K_ROLE[kind].shp);
    int32_t gain = M2K_ROLE[kind].gain, note = m->pitch16 >> 4;
    for (uint32_t b = 0; b < n; b += M2K_SUB) {
        switch (kind) {
        case M2K_SAW: m2k_saw(s, q, m->inc, note, o, (uint32_t)(v - t->v) + (uint32_t)(t - trk) * 16u); break;
        case M2K_ENS: m2k_ens(s, q, t->p, m->inc, o); break;
        case M2K_STAT: m2k_stat(s, q, t->p, m->inc, o); break;
        case M2K_PAR: m2k_par(s, q, t->p, m->inc, o); break;
        case M2K_DYN: m2k_dyn(s, q, m->inc, o); break;
        default: m2k_wav(s, q, m->inc, o); break;
        }
        for (uint32_t i = 0; i < M2K_SUB && b + i < n; i++)
            out[b + i] += voice_amp(m2k_sat16(((int32_t)o[i] * gain) >> 12), m, b + i);
    }
}

/* ENS's chorus over the part (MONO2K's: two taps off a half-rate line, each delayed by a sine at 0.2 Hz swinging
 * +-16.8 c, the second a quarter turn on; CHRL their mix, dry 1 - 0.83 x, wet 0.85 x). With SPREAD its CHRW sets them
 * apart across the two sides as MONO2K's (spread_buf: the left), without it they meet in the middle */
static uint8_t spread_on;             /* (voice.c: SPREAD's part, its left side) */
static int32_t *spread_buf;
static void m2k_ens_post(track_t *t, int32_t *out, uint32_t n, uint32_t nr)
{
    (void)nr;
    if (t < &trk[0] || t >= &trk[NPART]) return;
    m2k_chorus_t *c = &m2k_part_of(t)->ch;
    int32_t chrl = (int32_t)t->p[P_E5] * 256, chrw = (int32_t)t->p[P_E6] * 256;
    int32_t dry = 32768 - (chrl * 27197) / (127 * 256), wet = (chrl * 27853) / (127 * 256);
    wet = (wet * m2k_tq16(T_ENS_G, chrw)) >> 15;
    int32_t wside = (wet * m2k_tq16(T_ENS_W, chrw)) >> 15;
    for (uint32_t i = 0; i < n; i++) {
        int32_t x = out[i];
        c->acc += x >> 1;
        if (i & 1u) { c->line[c->w++ & (M2K_ENS_LEN - 1u)] = c->acc; c->acc = 0; }
        c->lph += ENS_CINC;
        if (!chrl) continue;
        int32_t y[2];
        for (uint32_t k = 0; k < 2u; k++) {
            int32_t d8 = (2 << 8) + ENS_CDEP + ((ENS_CDEP * m2k_sinp(c->lph + (k << 30))) >> 15);
            uint32_t pos = (c->w << 8) + ((i & 1u) << 7) - (uint32_t)d8;
            uint32_t j = pos >> 8;
            int32_t f = (int32_t)(pos & 255u);
            int32_t a = c->line[j & (M2K_ENS_LEN - 1u)], b = c->line[(j + 1u) & (M2K_ENS_LEN - 1u)];
            y[k] = a + (int32_t)m2k_mul_sh(b - a, f, 8);
        }
        int32_t mid = (int32_t)m2k_mul_sh((y[0] + y[1]) >> 1, wet, 15);
        out[i] = (int32_t)m2k_mul_sh(x, dry, 15) + mid;
        if (spread_on) {             /* the left side: the first tap's share up, the second's down */
            int32_t side = (int32_t)m2k_mul_sh(y[0] - y[1], wside, 16);
            spread_buf[i] = (int32_t)m2k_mul_sh(spread_buf[i], dry, 15) + (mid >> 1) + side;
        }
    }
}

/* the controls' lists */
static const char *const N_M2K_PCH[34] = {"-12", "-11", "-10", "-9", "-8", "-3:2", "-7", "-6", "-5", "-4:3", "-4", "-5:4",
    "-6:5", "-3", "-2", "-1", "OFF", "+1", "+2", "+3", "+6:5", "+5:4", "+4", "+4:3", "+5", "+6", "+7", "+3:2", "+8", "+9",
    "+10", "+11", "+12", 0};
static const char *const N_M2K_FR24[25] = {"1/64", "1/32", "1/16", "3/32", "1/8", "5/32", "3/16", "1/4", "5/16", "3/8",
    "7/16", "1/2", "5/8", "3/4", "7/8", "1", "1.25", "1.5", "1.75", "2", "2.5", "3", "3.5", "4", 0};
static const char *const N_M2K_WAVE[65] = {"OS01", "OS02", "OS03", "OS04", "OS05", "OS06", "OS07", "OS08", "OS09",
    "OS10", "OS11", "OS12", "OS13", "OS14", "OS15", "OS16", "OS17", "OS18", "OS19", "OS20", "OS21", "OS22", "OS23", "OS24",
    "OS25", "OS26", "OS27", "OS28", "OS29", "OS30", "OS31", "OS32", "OS33", "OS34", "OS35", "OS36", "OS37", "OS38", "OS39",
    "OS40", "OS41", "OS42", "OS43", "OS44", "OS45", "OS46", "OS47", "OS48", "OS49", "OS50", "OS51", "OS52", "OS53", "OS54",
    "OS55", "OS56", "OS57", "OS58", "OS59", "OS60", "OS61", "OS62", "OS63", "OS64", 0};
#define M2K_P(l) {l, F_INT, 0, 127, 0, 0, 0}
#define M2K_PD(l, d) {l, F_INT, 0, 127, d, 0, 0}
#define M2K_BI(l) {l, F_OFS, 0, 127, 64, 0, 0}     /* (MONO2K's bipolar: 64 the middle, shown -64..+63) */
#define M2K_FR(l, d) {l, F_ENUM, 0, 23, d, N_M2K_FR24, 0}
#define M2K_PCH(l) {l, F_ENUM, 0, 32, 16, N_M2K_PCH, 0}

/* presets: MONO2K's controls at their values (a ratio its step of 24, an interval its place of 33), the ADSR Melodee's */
static const preset_t M2K_SAW_PRESETS[] = {
    {"SUPERSAW", {100, 40, 64, 20, 0, 40, 0, 64}, {0, 64, 110, 50}, 0, 0},
    {"BIG PAD", {127, 70, 100, 40, 0, 60, 20, 64}, {60, 80, 120, 80}, 0, 0},
    {"MONO SAW", {0, 0, 0, 0, 60, 80, 0, 64}, {0, 50, 100, 30}, 0, 1},
    {"DRIFT LD", {60, 20, 30, 90, 0, 0, 0, 64}, {2, 60, 110, 40}, 0, 1},
    {"SUB BASS", {0, 0, 0, 10, 100, 127, 60, 64}, {0, 40, 90, 20}, 0, 1},
};
static const preset_t M2K_ENS_PRESETS[] = {
    {"MAJOR", {22, 26, 32, 0, 64, 80, 64, 64}, {10, 70, 110, 60}, 0, 0},
    {"MINOR", {19, 26, 32, 0, 64, 80, 64, 64}, {10, 70, 110, 60}, 0, 0},
    {"FIFTHS", {26, 32, 16, 40, 64, 60, 90, 64}, {0, 64, 110, 50}, 0, 0},
    {"OCT PULS", {32, 16, 16, 127, 32, 50, 40, 64}, {0, 60, 100, 40}, 0, 1},
    {"STRINGS", {26, 32, 22, 20, 90, 110, 110, 64}, {70, 80, 120, 90}, 0, 0},
};
static const preset_t M2K_STAT_PRESETS[] = {
    {"FM BELL", {19, 0, 90, 0, 22, 30, 100, 64}, {0, 90, 0, 80}, 0, 0},
    {"FM BASS", {15, 0, 50, 30, 15, 0, 70, 64}, {0, 50, 60, 20}, 0, 1},
    {"E PIANO", {15, 10, 80, 0, 23, 20, 90, 64}, {0, 80, 30, 60}, 0, 0},
    {"FEEDBACK", {15, 0, 40, 90, 19, 10, 110, 64}, {0, 60, 100, 40}, 0, 1},
};
static const preset_t M2K_PAR_PRESETS[] = {
    {"PAR KEYS", {15, 50, 19, 80, 21, 0, 90, 64}, {0, 70, 50, 50}, 0, 0},
    {"PAR ORGN", {11, 40, 15, 30, 19, 20, 70, 64}, {0, 30, 120, 30}, 0, 0},
    {"METALLIC", {17, 90, 22, 85, 23, 75, 110, 64}, {0, 90, 0, 70}, 0, 0},
    {"PAR BASS", {15, 70, 11, 40, 0, 0, 60, 64}, {0, 50, 70, 20}, 0, 1},
};
static const preset_t M2K_DYN_PRESETS[] = {
    {"DYN PLUCK", {64, 64, 40, 100, 64, 90, 0, 64}, {0, 60, 0, 40}, 0, 0},
    {"DYN BRASS", {64, 80, 50, 90, 32, 40, 20, 64}, {20, 70, 100, 50}, 0, 1},
    {"DYN BELL", {90, 64, 60, 110, 101, 100, 0, 64}, {0, 90, 0, 80}, 0, 0},
    {"DYN BASS", {32, 100, 50, 64, 64, 0, 40, 64}, {0, 50, 60, 20}, 0, 1},
};
static const preset_t M2K_WAV_PRESETS[] = {
    {"WAVE PAD", {26, 80, 60, 0, 0, 64, 0, 0}, {50, 70, 110, 70}, 0, 0},
    {"SCAN LEAD", {60, 110, 40, 0, 0, 70, 40, 0}, {0, 60, 100, 40}, 0, 1},
    {"SYNC LEAD", {10, 64, 127, 70, 0, 64, 0, 0}, {0, 60, 100, 40}, 0, 1},
    {"LOFI KEYS", {90, 64, 127, 0, 70, 64, 0, 40}, {0, 70, 40, 50}, 0, 0},
    {"DIGI BASS", {120, 40, 50, 20, 0, 64, 60, 0}, {0, 50, 70, 20}, 0, 1},
};

static const engine_t ENG_M2K_SAW = {
    .name = "SAW",
    .page_title = {"UNISON", "SUBS"},
    .edit = {M2K_PD("UNIL", 100), M2K_PD("UNIW", 40), M2K_PD("UNIX", 64), M2K_P("DRFT"), M2K_P("SUBX"), M2K_P("SUB1"),
             M2K_P("SUB2"), M2K_BI("TUNE")},
    .presets = M2K_SAW_PRESETS, .npresets = NELEM(M2K_SAW_PRESETS),
    .knob = {P_E0, P_E1, P_E5, P_REL},
    .poly = M2K_POLY,
    .note_on = m2k_note_on, .render = m2k_render,
};
/* ENS's voice is up to four saw pairs (host: about 1.2 x SAW's five saws): three budget points, the rest two */
static uint32_t m2k_ens_units(const track_t *t) { (void)t; return 3u; }
static const engine_t ENG_M2K_ENS = {
    .name = "ENS",
    .page_title = {"CHORD", "CHORUS"},
    .edit = {M2K_PCH("PCH2"), M2K_PCH("PCH3"), M2K_PCH("PCH4"), M2K_P("WAVE"), M2K_PD("PW", 64), M2K_PD("CHRL", 80),
             M2K_PD("CHRW", 64), M2K_BI("TUNE")},
    .presets = M2K_ENS_PRESETS, .npresets = NELEM(M2K_ENS_PRESETS),
    .knob = {P_E3, P_E4, P_E5, P_REL},
    .poly = M2K_ENS_POLY, .units = m2k_ens_units,
    .note_on = m2k_note_on, .render = m2k_render, .post = m2k_ens_post,
};
static const engine_t ENG_M2K_STAT = {
    .name = "STAT",
    .page_title = {"OP 1", "OP 2"},
    .edit = {M2K_FR("1FRQ", 19), M2K_PD("1FIN", 64), M2K_PD("1ENV", 90), M2K_P("1FB"), M2K_FR("2FRQ", 22), M2K_P("2VOL"),
             M2K_PD("TONE", 100), M2K_BI("TUNE")},
    .presets = M2K_STAT_PRESETS, .npresets = NELEM(M2K_STAT_PRESETS),
    .knob = {P_E0, P_E2, P_E6, P_REL},
    .poly = M2K_POLY,
    .note_on = m2k_note_on, .render = m2k_render,
};
static const engine_t ENG_M2K_PAR = {
    .name = "PAR",
    .page_title = {"OPS", "TONE"},
    .edit = {M2K_FR("1FRQ", 15), M2K_PD("1ENV", 50), M2K_FR("2FRQ", 19), M2K_PD("2ENV", 80), M2K_FR("3FRQ", 21), M2K_P("3ENV"),
             M2K_PD("TONE", 90), M2K_BI("TUNE")},
    .presets = M2K_PAR_PRESETS, .npresets = NELEM(M2K_PAR_PRESETS),
    .knob = {P_E1, P_E3, P_E6, P_REL},
    .poly = M2K_POLY,
    .note_on = m2k_note_on, .render = m2k_render,
};
static const engine_t ENG_M2K_DYN = {
    .name = "DYN",
    .page_title = {"OP 1", "OP 2"},
    .edit = {M2K_PD("1FRQ", 64), M2K_BI("1FEN"), M2K_PD("1VOL", 40), M2K_BI("1VEN"), M2K_PD("2FRQ", 64), M2K_P("2ENV"),
             M2K_P("2FB"), M2K_BI("TUNE")},
    .presets = M2K_DYN_PRESETS, .npresets = NELEM(M2K_DYN_PRESETS),
    .knob = {P_E1, P_E2, P_E5, P_REL},
    .poly = M2K_POLY,
    .note_on = m2k_note_on, .render = m2k_render,
};
static const engine_t ENG_M2K_WAV = {
    .name = "WAV",
    .page_title = {"WAVE", "SHAPE"},
    .edit = {{"WAVE", F_INT, 0, 127, 26, N_M2K_WAVE, 0}, M2K_BI("SCAN"), M2K_PD("STIM", 60), M2K_P("SYNC"), M2K_P("BITS"),
             M2K_BI("DTUN"), M2K_P("MIX2"), M2K_P("STEP")},
    .presets = M2K_WAV_PRESETS, .npresets = NELEM(M2K_WAV_PRESETS),
    .knob = {P_E0, P_E1, P_E3, P_REL},
    .poly = M2K_POLY,
    .note_on = m2k_note_on, .render = m2k_render,
};
