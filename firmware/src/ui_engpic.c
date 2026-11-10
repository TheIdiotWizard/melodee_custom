/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The engines' pictures on their EDIT pages (1.3.0, in LOOM-8's way: a picture of what the engine does, drawn from
 * its real values): what the engine plays, worked out with the engine's own tables and wave functions.
 *   WAV      the wave WAVE points at (T_WAVES64, blended into the next as the engine does, BITS and STEP applied),
 *            its neighbours in the table behind it
 *   SAW      the centre saw and the sub (SUBX's square, SUB1 / SUB2's sines); under them the unison's five saws by
 *            their detune (T_SW1LO .. T_SW2HI at UNIW) and level (UNIL, UNIX)
 *   ENS      one oscillator's wave (WAVE, PW: the two saws m2k_ens adds) and the chord the four play (PCH2..4)
 *   NOISE    the spectrum: COLR's slope through the low-pass / band-pass at FREQ and RES
 *   FMTONE   the algorithm's chart (fmt_render's routing): the carriers filled, the modulators' levels, the ratios,
 *            B2's feedback
 *   DUALVCO  the two oscillators' waves (dual_w1, dual_w2) under their decays, how CNFG joins them
 *   ANALOG4  the two oscillators (a4_osc at PW1), osc 2 at its tuning, the sub
 *   LOFI     the chip wave (PLS at DUTY, TRI, SAW, NOIS, a WRAM table) at its bit depth
 *   WHEEL    the nine drawbars (drw_level: the registration and SUB / BODY / TOP)
 *   PHYS     the partials the model rings (MODAL by STRC, STRNG by POS, MEMB, SYMP's strings), BRIT's tilt
 *   DRUM     the kit's lanes, lit as they hit
 * Drawn on the picture's band (ui_pages.c pv_picture), h rows. Included by ui_pages.c */
#define EP_X0 10                                        /* the traces: x 10 .. 229 */
#define EP_W 220

/* ---------------------------------------------------------- helpers --- */
static float ep_log2(float x)
{
    union { float f; uint32_t i; } u;
    float m;
    int32_t e;
    if (x <= 1e-12f)
        return -40.0f;
    u.f = x;
    e = (int32_t)((u.i >> 23) & 255u) - 127;
    u.i = (u.i & 0x7FFFFFu) | 0x3F800000u;
    m = u.f;
    return (float)e + (-0.34484843f * m + 2.02466578f) * m - 0.67487759f;
}
static float ep_exp2(float x)
{
    union { float f; uint32_t i; } u;
    int32_t n = (int32_t)(x < 0.0f ? x - 1.0f : x);
    float f = x - (float)n, r = 1.0f + f * (0.6565f + f * 0.344f);   /* 2^f on 0..1 */
    if (n < -60) return 0.0f;
    if (n > 60) n = 60;
    u.f = r;
    u.i += (uint32_t)n << 23;
    return u.f;
}
/* a trace: v(ctx, phase 0..65535) -32768..32767 across w columns from x0, centred on row cy, amp rows each way */
typedef int32_t (*ep_fn_t)(const void *ctx, uint32_t ph);
static void ep_trace(int32_t x0, int32_t w, int32_t cy, int32_t amp, ep_fn_t f, const void *ctx, uint32_t cycles,
                     uint16_t c, int bold)
{
    int32_t x, py = 0;
    for (x = 0; x < w; x++) {
        uint32_t ph = (uint32_t)((uint64_t)x * cycles * 65536u / (uint32_t)w) & 0xFFFFu;
        int32_t v = f(ctx, ph), y = cy * 16 - v * amp / 2048;
        cv_vspan_aa(x0 + x, x ? py : y, y, c);
        if (bold)
            cv_vspan_aa(x0 + x, (x ? py : y) + 16, y + 16, c);
        py = y;
    }
}
static uint16_t ep_fade(uint32_t pct) { return ux_mix(T_BG, T_THEME, (int32_t)pct); }
static void ep_label(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text_on(x, y, TY_LB, s, c, T_BG); }
static void ep_label_r(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text_r(x, y, TY_LB, s, c, T_BG); }

/* ------------------------------------------------------------- WAV --- */
typedef struct { int32_t ia, ib, wf, bits, stp; } ep_wav_t;
static int32_t ep_wav_at(const void *ctx, uint32_t ph16)
{
    const ep_wav_t *w = (const ep_wav_t *)ctx;
    uint32_t ph = ph16 << 16;
    int32_t j, f, a, b, x, dc, g;
    if (w->stp)
        ph &= ~((1u << (24 + w->stp)) - 1u);
    j = (int32_t)(ph >> 24);
    f = w->stp ? 0 : (int32_t)((ph >> 16) & 255u);
    {
        const int16_t *wa = &T_WAVES64[w->ia * 256], *wb = &T_WAVES64[w->ib * 256];
        a = wa[j] + (((wa[(j + 1) & 255] - wa[j]) * f) >> 8);
        b = wb[j] + (((wb[(j + 1) & 255] - wb[j]) * f) >> 8);
    }
    dc = T_WAVES64_DC[w->ia] + (((T_WAVES64_DC[w->ib] - T_WAVES64_DC[w->ia]) * w->wf) >> 8);
    g = T_WAVES64_G[w->ia] + (((T_WAVES64_G[w->ib] - T_WAVES64_G[w->ia]) * w->wf) >> 8);
    x = a + (((b - a) * w->wf) >> 8);
    x = ((x - dc) * g) >> 14;
    if (w->bits < 16) {
        int32_t sh = 16 - w->bits;
        x = (x >> sh) << sh;
    }
    return clamp(x, -32768, 32767);
}
static void ep_wav(const int16_t *p, int32_t h)
{
    ep_wav_t w;
    int32_t pos = clamp(p[P_E0] * 128, 0, 63 * 256), k, cy = h / 2 + 8, amp = h / 2 - 22;
    char b[16];
    if (!RES_OK()) {
        ep_label(EP_X0, ty_y(h / 2, TY_LB), "waves not installed", T_DIM);
        return;
    }
    w.bits = 16 - (p[P_E4] * 14 >> 7);
    w.stp = p[P_E7] * 6 >> 7;
    for (k = 3; k >= 1; k--) {                          /* the table behind: the waves either side, further back */
        for (int32_t sgn = -1; sgn <= 1; sgn += 2) {
            int32_t i = (pos >> 8) + sgn * k;
            if (i < 0 || i > 63)
                continue;
            w.ia = w.ib = i;
            w.wf = 0;
            ep_trace(EP_X0 + 40 + sgn * k * 7 + 3 * k, EP_W - 60, cy - k * 7, amp * (10 - k) / 12, ep_wav_at, &w, 1u,
                     ux_mix(T_BG, T_TEXT, 22 - k * 5), 0);
        }
    }
    w.ia = pos >> 8;
    w.ib = clamp(w.ia + 1, 0, 63);
    w.wf = pos & 255;
    ep_trace(EP_X0 + 30, EP_W - 40, cy, amp, ep_wav_at, &w, 1u, T_THEME, 1);
    str_cpy(b, T_WAVE64_NAME[w.ia], sizeof b);
    if (w.wf) {
        str_cpy(b + str_len(b), " > ", 4);
        str_cpy(b + str_len(b), T_WAVE64_NAME[w.ib], sizeof b - str_len(b));
    }
    ep_label(EP_X0, ty_y(10, TY_LB), b, T_TEXT);
    if (w.bits < 16 || w.stp) {
        char q[16];
        fmt_int(q, w.bits);
        str_cpy(q + str_len(q), w.stp ? " bit, step" : " bit", 12);
        ep_label_r(230, ty_y(10, TY_LB), w.bits < 16 ? q : "step", T_MID);
    }
}

/* ------------------------------------------------------------- SAW --- */
static int32_t ep_saw_at(const void *ctx, uint32_t ph) { (void)ctx; return (int32_t)ph - 32768; }
typedef struct { int32_t gx, g1, g2; } ep_sub_t;
static int32_t ep_sub_at(const void *ctx, uint32_t ph)
{
    const ep_sub_t *s = (const ep_sub_t *)ctx;
    uint32_t p32 = ph << 16;
    int32_t y = 0;
    y += ((p32 >> 1) < 0x40000000u ? 32767 : -32767) * s->gx >> 15;   /* SUBX: a square an octave down */
    y += (sine_i(p32 >> 1) * s->g1) >> 15;                              /* SUB1: a sine an octave down */
    y += (sine_i(p32 >> 2) * s->g2) >> 15;                              /* SUB2: two octaves down */
    return clamp(y, -32768, 32767);
}
static int32_t ep_cents(uint32_t q24)                    /* a ratio (Q24) in cents */
{
    return (int32_t)(1200.0f * ep_log2((float)q24 / 16777216.0f));
}
static void ep_saw(const int16_t *p, int32_t h)
{
    int32_t unil = p[P_E0] << 8, uniw = p[P_E1] << 8, g1 = 5166 * unil / (127 * 256), g2 = (g1 * (p[P_E2] << 8)) / (127 * 256);
    ep_sub_t s;
    int32_t top = h * 58 / 100, cy = top / 2 + 4, k, mx;
    int32_t ct[5], g[5];
    char b[16];
    s.gx = (3369 * m2k_tq16(T_SWSUBX, p[P_E4] << 8)) >> 15 << 2;
    s.g1 = (7097 * (p[P_E5] << 8) / (127 * 256)) << 2;
    s.g2 = (2701 * (p[P_E6] << 8) / (127 * 256)) << 2;
    ep_trace(EP_X0, EP_W, cy, top / 2 - 8, ep_saw_at, 0, 3u, T_THEME, 1);
    if (s.gx || s.g1 || s.g2)
        ep_trace(EP_X0, EP_W, cy, top / 2 - 8, ep_sub_at, &s, 3u, T_MID, 0);
    /* the unison: a line a saw at its detune (cents), as tall as its level */
    ct[0] = 0;
    ct[1] = ep_cents(m2k_tq32(T_SW1LO, uniw));
    ct[2] = ep_cents(m2k_tq32(T_SW1HI, uniw));
    ct[3] = ep_cents(m2k_tq32(T_SW2LO, uniw));
    ct[4] = ep_cents(m2k_tq32(T_SW2HI, uniw));
    g[0] = 5166, g[1] = g[2] = unil ? g1 : 0, g[3] = g[4] = unil ? g2 : 0;
    for (mx = 25, k = 1; k < 5; k++)
        if (g[k] && (ct[k] < 0 ? -ct[k] : ct[k]) > mx)
            mx = ct[k] < 0 ? -ct[k] : ct[k];
    {
        int32_t base = h - 6, ht = h - top - 12, xc = 120;
        cv_rect(EP_X0, base, EP_W, 1, T_LINE);
        for (k = 0; k < 5; k++) {
            int32_t x = xc + ct[k] * 96 / mx, l = g[k] * ht / 5166;
            if (!g[k])
                continue;
            cv_rect(x - 1, base - l, 3, l, k ? ep_fade(70) : T_THEME);
        }
        if (mx >= 100) {                                /* (wide: in semitones, a decimal) */
            b[0] = '+';
            fmt_int(b + 1, mx / 100);
            str_cpy(b + str_len(b), ".", 2);
            fmt_int(b + str_len(b), mx / 10 % 10);
            str_cpy(b + str_len(b), " st", 4);
        } else {
            b[0] = '+';
            fmt_int(b + 1, mx);
            str_cpy(b + str_len(b), " ct", 4);
        }
        ep_label_r(230, ty_y(top + 10, TY_LB), b, T_DIM);
        b[0] = '-';
        ep_label(EP_X0, ty_y(top + 10, TY_LB), b, T_DIM);
    }
}

/* ------------------------------------------------------------- ENS --- */
typedef struct { int32_t wa, wb; uint32_t off; } ep_ens_t;
static int32_t ep_ens_at(const void *ctx, uint32_t ph)
{
    const ep_ens_t *e = (const ep_ens_t *)ctx;
    uint32_t p32 = ph << 16;
    int32_t a = (int32_t)(p32 >> 16) - 32768, b = (int32_t)((p32 + e->off) >> 16) - 32768;
    return clamp((a * e->wa + b * e->wb) >> 15, -32768, 32767);
}
static void ep_ens(const int16_t *p, int32_t h)
{
    ep_ens_t e;
    int32_t w8 = p[P_E3] << 8, k, cy = h / 2, amp = h / 2 - 14;
    const param_desc_t *d = param_desc_of(ENGI_SAW + 1u, P_E0);
    e.wa = 32768 - (w8 * 12124) / (127 * 256);
    e.wb = 32768 - (w8 * 53412) / (127 * 256);
    e.off = (uint32_t)(p[P_E4] << 8) << 17;
    ep_trace(EP_X0, 100, cy, amp, ep_ens_at, &e, 2u, T_THEME, 1);
    /* the chord: a rung a sounding oscillator at its interval (semitones from the root, -12 .. +12) */
    {
        int32_t x0 = 130, x1 = 230, y0 = 8, y1 = h - 8;
        int32_t ly[4], nl = 0;
        cv_rect(x0, y0, 1, y1 - y0, T_LINE);
        for (k = 0; k < 4; k++) {
            int32_t iv = 0, y;
            const char *nm = "root";
            if (k) {
                uint32_t x = (uint32_t)clamp(p[P_E0 + k - 1], 0, 32), j = 0;
                while (j < 127u && T_ENS_PIDX[j] < x) j++;
                iv = T_ENS_PCH[j];
                if (iv == ENS_OFF)
                    continue;
                nm = d && d->names ? d->names[x] : "";
            }
            y = (y0 + y1) / 2 - iv * (y1 - y0) / (2 * 12 * 64);
            cv_rrect(x0 + 4, y - 4, x1 - x0 - 50, 8, 4, k ? ep_fade(60) : T_THEME, T_BG);
            {   /* its name, unless one is drawn within a line of it (close intervals: the rungs say it) */
                int clash = 0;
                for (int32_t j = 0; j < nl; j++)
                    clash |= y > ly[j] - 15 && y < ly[j] + 15;
                if (!clash) {
                    ep_label_r(x1, ty_y(y, TY_LB), nm, k ? T_MID : T_TEXT);
                    ly[nl++] = y;
                }
            }
        }
    }
}

/* ----------------------------------------------------------- NOISE --- */
static void ep_noise(const int16_t *p, int32_t h)
{
    int32_t x, py = 0, colr = p[P_E1], res = p[P_E3], top = 6, bot = h - 20;
    float k = (float)(8192 - res * 7800 / 127) / 4096.0f, mix = (float)res / 127.0f;
    float fc = 20000.0f * ep_exp2((float)(p[P_E2] - 127) * 0.86f / 12.0f);
    const char *const MODE[4] = {"ANLG", "DUST", "LFSR", "META"};
    cv_rect(EP_X0, bot, EP_W, 1, T_LINE);
    for (x = 0; x < EP_W; x++) {                        /* 20 Hz .. 20 kHz, ten octaves across */
        float oct = (float)x * 10.0f / (float)EP_W, f = 20.0f * ep_exp2(oct), r = f / fc, r2 = r * r;
        float den = (1.0f - r2) * (1.0f - r2) + k * k * r2, lp = 1.0f / den, bp = k * k * r2 / den;
        float a = (1.0f - mix) * ep_exp2(0.5f * ep_log2(lp)) + mix * ep_exp2(0.5f * ep_log2(bp));
        float db = 6.0206f * ep_log2(a) - (float)colr * 6.0f / 127.0f * (oct - 5.6f);   /* COLR: 0 .. -6 dB/oct */
        int32_t y = top * 16 + (int32_t)((12.0f - db) * (float)(bot - top) * 16.0f / 60.0f);   /* +12 .. -48 dB */
        y = clamp(y, top * 16, bot * 16);
        cv_rect(EP_X0 + x, y / 16, 1, bot - y / 16, ep_fade(22));
        cv_vspan_aa(EP_X0 + x, x ? py : y, y, T_THEME);
        py = y;
    }
    ep_label(EP_X0, ty_y(h - 13, TY_LB), MODE[(uint32_t)p[P_E0] & 3u], T_TEXT);
    ep_label_r(230, ty_y(h - 13, TY_LB), res > 64 ? "band-pass" : "low-pass", T_MID);
}

/* ---------------------------------------------------------- FMTONE --- */
/* fmt_render's eight routings: where B2 B1 A C sit (column, row of three), what modulates what, the carriers */
static const struct { uint8_t col[4], row[4], edge[4][2], ne, car; } EP_ALG[8] = {
    {{0, 1, 2, 3}, {1, 1, 1, 1}, {{0, 1}, {1, 2}, {2, 3}}, 3, 8},           /* B2 > B1 > A > C */
    {{0, 0, 1, 2}, {2, 0, 1, 1}, {{1, 2}, {0, 2}, {2, 3}}, 3, 8},           /* B1 + B2 > A > C */
    {{1, 0, 1, 2}, {2, 0, 0, 1}, {{1, 2}, {2, 3}, {0, 3}}, 3, 8},           /* B1 > A > C, B2 > C */
    {{0, 1, 1, 2}, {0, 0, 2, 1}, {{0, 1}, {1, 3}, {2, 3}}, 3, 8},           /* B2 > B1 > C, A > C */
    {{0, 1, 0, 1}, {0, 0, 2, 2}, {{0, 1}, {2, 3}}, 2, 2 | 8},               /* B2 > B1, A > C: two carriers */
    {{0, 2, 2, 2}, {1, 0, 1, 2}, {{0, 1}, {0, 2}, {0, 3}}, 3, 2 | 4 | 8},   /* B2 > B1, A, C */
    {{0, 1, 1, 1}, {0, 0, 1, 2}, {{0, 1}}, 1, 2 | 4 | 8},                   /* B2 > B1; A; C */
    {{0, 1, 2, 3}, {1, 1, 1, 1}, {{0, 0}}, 0, 1 | 2 | 4 | 8},               /* four carriers */
};
static void ep_fmtone(const int16_t *p, int32_t h)
{
    uint32_t alg = (uint32_t)p[P_E0] & 7u, o, i;
    const param_desc_t *dr = param_desc_of(ENGI_FMTONE, P_E1);
    static const char *const NM[4] = {"B2", "B1", "A", "C"};
    int32_t bw = 46, bh = (h - 16) / 3 - 4, x[4], y[4], lv[4];
    char b[12];
    lv[0] = lv[1] = TSEL->p[P_FM2_LEVEL];
    lv[2] = TSEL->p[P_FM1_LEVEL];
    lv[3] = 127;
    for (o = 0; o < 4u; o++) {
        x[o] = 14 + EP_ALG[alg].col[o] * 56;
        y[o] = 4 + EP_ALG[alg].row[o] * (bh + 5);
    }
    for (i = 0; i < EP_ALG[alg].ne; i++) {               /* modulator -> modulated: from its right edge */
        uint32_t a = EP_ALG[alg].edge[i][0], c = EP_ALG[alg].edge[i][1];
        cv_line_t(x[a] + bw, y[a] + bh / 2, x[c], y[c] + bh / 2, T_MID, 2);
    }
    cv_rect(228, 6, 2, 3 * (bh + 5) - 6, T_LINE);       /* the output */
    for (o = 0; o < 4u; o++) {
        int car = (EP_ALG[alg].car >> o) & 1u;
        uint16_t c = car ? T_THEME : T_SURF;
        if (car)
            cv_line_t(x[o] + bw, y[o] + bh / 2, 228, y[o] + bh / 2, T_THEME, 2);
        cv_rrect(x[o], y[o], bw, bh, 6, car ? T_THEME : T_MID, T_BG);
        cv_rrect(x[o] + 1, y[o] + 1, bw - 2, bh - 2, 5, c, car ? T_THEME : T_MID);
        if (!car && lv[o]) {                             /* a modulator's level: its fill */
            int32_t fh = (bh - 4) * lv[o] / 127;
            cv_rect(x[o] + 3, y[o] + bh - 2 - fh, bw - 6, fh, ep_fade(30));
        }
        str_cpy(b, NM[o], sizeof b);
        if (dr && dr->names) {                           /* its ratio (B2: an octave above B1, at most 16) */
            int32_t ri = o == 3u ? p[P_E1] : o == 2u ? p[P_E2] : p[P_E3];
            str_cpy(b + str_len(b), " ", 2);
            if (o == 0u) {                               /* (fmt_render's rb2: B1's x2, at most 16) */
                uint32_t q8 = FMT_RATIO_Q8[clamp(ri, 0, FMT_NRATIO - 1)] * 2u;
                q8 = q8 > 4096u ? 4096u : q8;
                fmt_int(b + str_len(b), (int32_t)(q8 >> 8));
                if (q8 & 255u) {
                    str_cpy(b + str_len(b), ".", 2);
                    fmt_int(b + str_len(b), (int32_t)((q8 & 255u) * 10u / 256u));
                }
            } else {
                str_cpy(b + str_len(b), dr->names[clamp(ri, 0, 22)], sizeof b - str_len(b));
            }
        }
        cv_text_c(x[o] + bw / 2, ty_y(y[o] + bh / 2, TY_LB), ty_fit(TY_LB, b, bw - 4), b, car ? T_INK : T_TEXT,
                  c);
    }
    if (p[P_E6])                                        /* B2's feedback: a loop over it */
        cv_frame(x[0] + 10, y[0] - 3 < 0 ? 0 : y[0] - 3, bw - 20, 4, T_THEME);
}

/* ---------------------------------------------------------- DUALVCO --- */
typedef struct { uint32_t w, osc, mode, w2; int32_t ratio_q8, d1, d2; } ep_dual_t;
static int32_t ep_dual_at(const void *ctx, uint32_t ph)
{
    const ep_dual_t *d = (const ep_dual_t *)ctx;
    uint32_t p32 = ph << 16, p2 = (uint32_t)((uint64_t)p32 * (uint32_t)d->ratio_q8 >> 8);
    int32_t a = dual_w1(d->w, p32, 1u << 12), b = dual_w2(d->w2, p2, 1u << 12);
    if (d->osc == 1u) return a;
    if (d->osc == 2u) return b;
    return d->mode == 1u ? (a * b) >> 15 : (a + b) >> 1;   /* (R: ring, else the mix; FM: the mix stands for it) */
}
static void ep_dual(const int16_t *p, int32_t h)
{
    static const char *const MODE[4] = {"mix", "ring", "FM", "FM + ring"};
    uint32_t c = (uint32_t)clamp(p[P_E4], 0, 79), m = c % 40u;
    ep_dual_t d;
    int32_t lane = (h - 20) / 2, x, py = 0;
    d.w = (m % 10u) / 5u;
    d.w2 = m % 5u;
    d.mode = m / 10u;
    d.ratio_q8 = (int32_t)(256.0f * ep_exp2((float)p[P_E1] / 12.0f));
    for (d.osc = 1u; d.osc <= 2u; d.osc++) {            /* each oscillator under its decay */
        int32_t cy = 6 + lane / 2 + (int32_t)(d.osc - 1u) * (lane + 4), dec = d.osc == 1u ? p[P_E2] : p[P_E3];
        float tau = 0.04f + (float)dec * (float)dec / 3000.0f;   /* (the decay's time constant, the picture's widths) */
        cv_rect(EP_X0, cy, EP_W, 1, T_LINE);
        for (x = 0; x < EP_W; x++) {
            uint32_t ph = (uint32_t)(x * 6 * 65536 / EP_W) & 0xFFFFu;
            float env = ep_exp2(-(float)x / (float)EP_W / tau * 1.44f);
            int32_t v = ep_dual_at(&d, ph), y = cy * 16 - (int32_t)((float)v * env) * (lane / 2 - 2) / 2048;
            cv_vspan_aa(EP_X0 + x, x ? py : y, y, d.osc == 1u ? T_THEME : T_SEC);
            py = y;
        }
    }
    ep_label(EP_X0, ty_y(h - 13, TY_LB), MODE[d.mode & 3u], T_TEXT);
    if (c >= DUAL_RESET_FROM)
        ep_label_r(230, ty_y(h - 13, TY_LB), "phase reset", T_MID);
}

/* ---------------------------------------------------------- ANALOG4 --- */
typedef struct { uint32_t w; uint32_t pw; int32_t ratio_q8; } ep_a4_t;
static int32_t ep_a4_at(const void *ctx, uint32_t ph)
{
    const ep_a4_t *a = (const ep_a4_t *)ctx;
    uint32_t p32 = (uint32_t)((uint64_t)(ph << 16) * (uint32_t)a->ratio_q8 >> 8);
    return a4_osc(a->w, p32, 1u << 12, a->pw);
}
static int32_t ep_sine_oct_at(const void *ctx, uint32_t ph) { (void)ctx; return sine_i((ph << 16) >> 1); }
static void ep_analog4(const int16_t *p, int32_t h)
{
    ep_a4_t o1, o2;
    int32_t cy = h / 2 - 4, amp = h / 2 - 16, l1 = TSEL->p[P_FM1_LEVEL], l2 = TSEL->p[P_FM2_LEVEL];
    char b[16];
    o1.pw = o2.pw = (uint32_t)clamp(p[P_E1] * 32, 16, 4080) << 20;
    o1.w = (uint32_t)p[P_E0] & 3u;
    o2.w = (uint32_t)p[P_E4] & 3u;
    o1.ratio_q8 = (int32_t)(256.0f * ep_exp2((float)p[P_E2] / 12.0f));
    o2.ratio_q8 = (int32_t)(256.0f * ep_exp2((float)p[P_E6] / 12.0f + (float)p[P_E7] / 1200.0f));
    cv_rect(EP_X0, cy, EP_W, 1, T_LINE);
    if (p[P_E3])                                        /* the sub: a sine an octave under osc 1 */
        ep_trace(EP_X0, EP_W, cy, amp * p[P_E3] / 127, ep_sine_oct_at, 0, 2u, T_DIM, 0);
    if (l2)
        ep_trace(EP_X0, EP_W, cy, amp * (l2 + 20) / 147, ep_a4_at, &o2, 2u, T_SEC, 0);
    if (l1)
        ep_trace(EP_X0, EP_W, cy, amp * (l1 + 20) / 147, ep_a4_at, &o1, 2u, T_THEME, 1);
    str_cpy(b, "osc 2 ", 8);
    if (p[P_E6] >= 0) str_cpy(b + str_len(b), "+", 2);
    fmt_int(b + str_len(b), p[P_E6]);
    str_cpy(b + str_len(b), " st", 4);
    ep_label_r(230, ty_y(h - 13, TY_LB), b, T_SEC);
    ep_label(EP_X0, ty_y(h - 13, TY_LB), p[P_E3] ? "osc 1 + sub" : "osc 1", T_TEXT);
}

/* ------------------------------------------------------------- LOFI --- */
#if MELODEE_FELUCCA_ENGINES                             /* (eng_lofi.c: the Felucca engines' build) */
typedef struct { uint32_t wave, levels; int32_t duty; uint32_t tab; } ep_lofi_t;
static int32_t ep_lofi_at(const void *ctx, uint32_t ph)
{
    const ep_lofi_t *l = (const ep_lofi_t *)ctx;
    int32_t v, q;
    switch (l->wave) {
    case RW_PLS: v = (int32_t)ph < l->duty ? 32767 : -32767; break;
    case RW_TRI: v = ph < 32768u ? (int32_t)ph * 2 - 32768 : 98303 - (int32_t)ph * 2; break;
    case RW_SAW: v = (int32_t)ph - 32768; break;
    case RW_NOIS: {                                     /* (a fixed run of random steps: what noise looks like) */
        uint32_t s = (ph >> 11) * 2654435761u;
        v = (int32_t)((s >> 16) & 0xFFFFu) - 32768;
        break;
    }
    default: {                                          /* WRAM: 32 four-bit samples, read step-wise */
        uint32_t i = ph >> 11, byte = WRAM[l->tab & 15u][i >> 1];
        v = (int32_t)((i & 1u ? byte : byte >> 4) & 15u) * 4369 - 32768;
        break;
    }
    }
    q = 65536 / (int32_t)l->levels;
    v = ((v + 32768) / q) * q - 32768 + q / 2;          /* the bit depth: as many steps */
    return clamp(v, -32768, 32767);
}
static void ep_lofi(const int16_t *p, int32_t h)
{
    static const uint16_t LEV[5] = {16, 8, 256, 2, 16};
    ep_lofi_t l;
    uint32_t chip = (uint32_t)clamp(p[P_E0], 0, 4);
    int32_t cy = h / 2 - 4;
    char b[16];
    l.wave = (uint32_t)clamp(p[P_E1], 0, 4);
    l.levels = LEV[chip];
    l.duty = chip == CHIP_STEP ? (int32_t)((p[P_E2] >> 5) + 1) * 8192 / (p[P_E2] >> 5 == 3 ? 1 : 1) : 4096 + p[P_E2] * 448;
    if (chip == CHIP_STEP) {                            /* STEP's duties: 12.5 / 25 / 50 / 75 % */
        static const int32_t D[4] = {8192, 16384, 32768, 49152};
        l.duty = D[(p[P_E2] >> 5) & 3];
    }
    l.tab = (uint32_t)p[P_E2] >> 3;
    cv_rect(EP_X0, cy, EP_W, 1, T_LINE);
    ep_trace(EP_X0, EP_W, cy, h / 2 - 16, ep_lofi_at, &l, 2u, T_THEME, 1);
    str_cpy(b, N_RWAVE[l.wave], sizeof b);
    if (l.wave == RW_WRAM) {
        str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), N_WRAM[l.tab & 15u], sizeof b - str_len(b));
    }
    ep_label(EP_X0, ty_y(h - 13, TY_LB), b, T_TEXT);
    ep_label_r(230, ty_y(h - 13, TY_LB), N_CHIP[chip], T_MID);
}

#endif
/* ------------------------------------------------------------ WHEEL --- */
static void ep_wheel(const int16_t *p, int32_t h)
{
    static const char *const FT[9] = {"16", "5", "8", "4", "2", "2", "1", "1", "1"};
    int32_t k, bw = 18, gap = (220 - 9 * bw) / 8, top = 6, bot = h - 22;
    for (k = 0; k < 9; k++) {
        int32_t x = EP_X0 + k * (bw + gap), lv = drw_level(p, (uint32_t)k), l = lv * (bot - top) / 8;
        cv_rrect(x, top, bw, bot - top, 5, T_SURF, T_BG);
        if (l)
            cv_rrect(x, top, bw, l < 6 ? 6 : l, 5, k < 2 ? ep_fade(70) : k < 4 ? T_THEME : ep_fade(85), T_SURF);
        cv_text_c(x + bw / 2, ty_y(h - 10, TY_LB), TY_LB, FT[k], T_MID, T_BG);
    }
}

/* ------------------------------------------------------------- PHYS --- */
static void ep_phys(const int16_t *p, int32_t h)
{
    static const float MEMB[8] = {1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f};
    uint32_t md = (uint32_t)p[P_E0] % 4u, n, i;
    float r[12], a[12], strc = (float)p[P_E1], brit = (float)p[P_E2] / 127.0f;
    int32_t base = h - 20, top = 6;
    static const char *const MN[4] = {"modal", "string", "membrane", "sympathetic"};
    if (md == 0u) {                                     /* MODAL: STRC from harmonic (34) to a free bar (70) to a tuned
                                                         * bar (116) */
        static const float A0[4] = {1.0f, 2.0f, 3.0f, 4.0f}, A1[4] = {1.0f, 2.83f, 5.48f, 8.90f},
                           A2[4] = {1.0f, 3.57f, 7.65f, 12.0f}, A3[4] = {1.0f, 4.0f, 9.0f, 16.0f};
        for (n = 4u, i = 0; i < 4u; i++) {
            float t;
            if (strc <= 34.0f) r[i] = A0[i];
            else if (strc <= 70.0f) t = (strc - 34.0f) / 36.0f, r[i] = A0[i] + (A1[i] - A0[i]) * t;
            else if (strc <= 98.0f) t = (strc - 70.0f) / 28.0f, r[i] = A1[i] + (A2[i] - A1[i]) * t;
            else t = (strc - 98.0f) / 29.0f, r[i] = A2[i] + (A3[i] - A2[i]) * (t > 1.0f ? 1.0f : t);
            a[i] = ep_exp2(-(float)i * (2.2f - 2.0f * brit));
        }
    } else if (md == 2u) {                              /* MEMB: a drum head's modes */
        for (n = 8u, i = 0; i < 8u; i++)
            r[i] = MEMB[i], a[i] = ep_exp2(-(float)i * (1.6f - 1.4f * brit));
    } else {                                            /* STRNG, SYMP: harmonics, POS's comb (where it is plucked) */
        float pos = (float)p[P_E4] / 254.0f;            /* 0 .. 1/2 */
        for (n = 12u, i = 0; i < 12u; i++) {
            float x = (float)(i + 1u) * pos, s = x - (float)(int32_t)x;   /* |sin(pi n pos)|, roughly */
            s = s > 0.5f ? 1.0f - s : s;
            r[i] = (float)(i + 1u);
            a[i] = (pos > 0.002f ? s * 2.0f : 1.0f) * ep_exp2(-(float)i * (1.0f - 0.9f * brit) * 0.6f);
        }
    }
    cv_rect(EP_X0, base, EP_W, 1, T_LINE);
    for (i = 0; i < n; i++) {                           /* a line a partial: x by its ratio (log, 1 .. 16) */
        int32_t x = EP_X0 + 6 + (int32_t)(ep_log2(r[i]) * (float)(EP_W - 12) / 4.0f), l = (int32_t)(a[i] * (float)(base - top));
        if (x > EP_X0 + EP_W - 2 || l < 1)
            continue;
        cv_rect(x - 1, base - l, i ? 2 : 3, l, i ? ep_fade(80) : T_THEME);
    }
    ep_label(EP_X0, ty_y(h - 13, TY_LB), MN[md], T_TEXT);
    ep_label_r(230, ty_y(h - 13, TY_LB), "partials", T_MID);
}

/* ------------------------------------------------------------- DRUM --- */
static uint8_t ep_drum_lit[NLANE];
static uint32_t ep_drum_tick(void)                      /* the lanes that hit lately (drum_flash, as Stage reads it) */
{
    uint32_t f = drum_flash[song.sel % NTRK], l, m = 0;
    drum_flash[song.sel % NTRK] = 0;
    for (l = 0; l < NLANE; l++) {
        ep_drum_lit[l] = (f >> l) & 1u ? 8u : ep_drum_lit[l] ? (uint8_t)(ep_drum_lit[l] - 1u) : 0u;
        m |= (ep_drum_lit[l] ? 1u : 0u) << l;
    }
    return m;
}
static void ep_drum(const int16_t *p, int32_t h)
{
    uint32_t l;
    int32_t pw = 52, ph = (h - 14) / 2;
    (void)p;
    for (l = 0; l < NLANE; l++) {
        int32_t x = 6 + (int32_t)(l % 4u) * (pw + 6), y = 4 + (int32_t)(l / 4u) * (ph + 6);
        int lit = ep_drum_lit[l] != 0u;
        cv_rrect(x, y, pw, ph, 8, lit ? T_THEME : T_LINE, T_BG);
        cv_rrect(x + 1, y + 1, pw - 2, ph - 2, 7, lit ? T_THEME : T_SURF, lit ? T_THEME : T_LINE);
        {
            const char *nm = drum_lane_name(TSEL, l);
            if (text_w(TY_LB, nm) > pw - 8)
                nm = drum_lane_abbr(TSEL, l);           /* ("SD": a name too wide for its pad) */
            cv_text_c(x + pw / 2, ty_y(y + ph / 2, TY_LB), TY_LB, nm, lit ? T_INK : T_TEXT, lit ? T_THEME : T_SURF);
        }
    }
}

/* ------------------------------------------------------------- MIDI --- */
static void ep_midi(const int16_t *p, int32_t h)
{
    char b[16];
    str_cpy(b, "CH ", 4);
    fmt_int(b + 3, p[P_E0]);
    cv_text_on(EP_X0, ty_y(h / 2 - 8, TY_VL), TY_VL, b, T_THEME, T_BG);
    ep_label(EP_X0, ty_y(h - 12, TY_LB), p[P_E1] ? "notes and CCs out, USB and TRS" : "notes and CCs out, USB",
             T_MID);
}

/* the engine's picture (1 drawn, 0 none: the scope) */
static int ep_has(uint32_t e)
{
    return e == ENGI_SAW || e == ENGI_SAW + 1u || e == ENGI_WAV || e == 11u || e == ENGI_FMTONE || e == ENGI_DUAL ||
           e == ENGI_ANA4 || (MELODEE_FELUCCA_ENGINES && e == 3u) || e == 7u || e == 9u || e == ENGI_DRUM || e == ENGI_MIDI;
}
static uint32_t ep_sig(uint32_t e)
{
    uint32_t s = 2166136261u, i;
    for (i = 0; i < 8u; i++)
        s = (s ^ (uint16_t)TSEL->p[P_E0 + i]) * 16777619u;
    if (e == ENGI_FMTONE || e == ENGI_ANA4)
        for (i = P_FM1_ATK; i <= P_FM4_LEVEL; i++)
            s = (s ^ (uint16_t)TSEL->p[i]) * 16777619u;
    if (e == ENGI_DRUM)
        s ^= ep_drum_tick() * 2654435761u;
    return s ^ e * 40503u;
}
static void ep_draw(uint32_t e, int32_t h)
{
    const int16_t *p = TSEL->p + 0;
    if (e == ENGI_WAV) ep_wav(p, h);
    else if (e == ENGI_SAW) ep_saw(p, h);
    else if (e == ENGI_SAW + 1u) ep_ens(p, h);
    else if (e == 11u) ep_noise(p, h);
    else if (e == ENGI_FMTONE) ep_fmtone(p, h);
    else if (e == ENGI_DUAL) ep_dual(p, h);
    else if (e == ENGI_ANA4) ep_analog4(p, h);
#if MELODEE_FELUCCA_ENGINES
    else if (e == 3u) ep_lofi(p, h);
#endif
    else if (e == 7u) ep_wheel(p, h);
    else if (e == 9u) ep_phys(p, h);
    else if (e == ENGI_DRUM) ep_drum(p, h);
    else ep_midi(p, h);
}
