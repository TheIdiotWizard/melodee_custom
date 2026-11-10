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
static void ep_label(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text_on(x, y, TY_SM, s, c, T_BG); }
static void ep_label_r(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text_r(x, y, TY_SM, s, c, T_BG); }
/* the caption (1.3.7, the mockup's: under the picture, small and dim, what the values mean in words) */
#define EP_CAP 48
static void ep_cat(char *cap, const char *s) { str_cpy(cap + str_len(cap), s, EP_CAP - str_len(cap)); }
static void ep_cati(char *cap, int32_t v, int sign)
{
    char b[12];
    b[0] = '+';
    fmt_int(b + (sign && v > 0), v);
    ep_cat(cap, b);
}
static void ep_catv(char *cap, uint32_t e, uint32_t id, int32_t v)   /* (a value as its knob shows it) */
{
    const param_desc_t *d = param_desc_of(e, id);
    const char *u = "";
    char b[16];
    if (!d) { ep_cati(cap, v, 0); return; }
    param_format(d, v, b, &u);
    ep_cat(cap, b);
    if (u && u[0] && !str_eq(u, "%")) { ep_cat(cap, " "); ep_cat(cap, u); }
    else if (u && u[0]) ep_cat(cap, u);
}
static void ep_catl(char *cap, const char *s)           /* (a name in small letters) */
{
    uint32_t n = str_len(cap), i;
    for (i = 0; s[i] && n + 1u < EP_CAP; i++)
        cap[n++] = s[i] >= 'A' && s[i] <= 'Z' ? (char)(s[i] + 32) : s[i];
    cap[n] = 0;
}

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
static void ep_wav(const int16_t *p, int32_t h, char *cap)
{
    ep_wav_t w;
    int32_t pos = clamp(p[P_E0] * 128, 0, 63 * 256), k, cy = h / 2 + 6, amp = h / 2 - 12;
    if (!RES_OK()) {
        ep_cat(cap, "waves not installed");
        return;
    }
    w.bits = 16 - (p[P_E4] * 14 >> 7);
    w.stp = p[P_E7] * 6 >> 7;
    for (k = 4; k >= 1; k--) {                          /* the table behind: the next waves, further up and back */
        int32_t i = (pos >> 8) + k;
        if (i > 63)
            continue;
        w.ia = w.ib = i;
        w.wf = 0;
        ep_trace(EP_X0 + 30 + k * 9, EP_W - 60, cy - k * 7, amp * (12 - k) / 12, ep_wav_at, &w, 1u, ep_fade(48 - k * 9), 0);
    }
    w.ia = pos >> 8;
    w.ib = clamp(w.ia + 1, 0, 63);
    w.wf = pos & 255;
    ep_trace(EP_X0 + 20, EP_W - 60, cy, amp, ep_wav_at, &w, 1u, T_THEME, 1);
    ep_cat(cap, T_WAVE64_NAME[w.ia]);                   /* "OS14 · scan +17 through the table" */
    ep_cat(cap, " \xb7 scan ");
    ep_catv(cap, ENGI_WAV, P_E1, p[P_E1]);
    ep_cat(cap, " through the table");
    if (w.bits < 16) {
        ep_cat(cap, ", ");
        ep_cati(cap, w.bits, 0);
        ep_cat(cap, " bit");
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
typedef struct { int32_t r; } ep_ratio_t;
static int32_t ep_saw_r_at(const void *ctx, uint32_t ph)  /* a saw at a ratio (Q16) of the centre's */
{
    return (int32_t)((uint32_t)((uint64_t)ph * (uint32_t)((const ep_ratio_t *)ctx)->r >> 16) & 0xFFFFu) - 32768;
}
static void ep_saw(const int16_t *p, int32_t h, char *cap)
{
    int32_t unil = p[P_E0] << 8, uniw = p[P_E1] << 8, g1 = 5166 * unil / (127 * 256), g2 = (g1 * (p[P_E2] << 8)) / (127 * 256);
    ep_sub_t s;
    int32_t cy = h / 2, amp = h / 2 - 6, k, mx, n = 1;
    int32_t ct[5], g[5];
    ep_ratio_t r;
    s.gx = (3369 * m2k_tq16(T_SWSUBX, p[P_E4] << 8)) >> 15 << 2;
    s.g1 = (7097 * (p[P_E5] << 8) / (127 * 256)) << 2;
    s.g2 = (2701 * (p[P_E6] << 8) / (127 * 256)) << 2;
    ct[0] = 0;                                          /* the unison: four saws at their detune (cents), by level */
    ct[1] = ep_cents(m2k_tq32(T_SW1LO, uniw));
    ct[2] = ep_cents(m2k_tq32(T_SW1HI, uniw));
    ct[3] = ep_cents(m2k_tq32(T_SW2LO, uniw));
    ct[4] = ep_cents(m2k_tq32(T_SW2HI, uniw));
    g[0] = 5166, g[1] = g[2] = unil ? g1 : 0, g[3] = g[4] = unil ? g2 : 0;
    cv_rect(EP_X0, cy, EP_W, 1, T_LINE);
    for (mx = 0, k = 4; k >= 1; k--) {
        if (!g[k])
            continue;
        n++;
        if ((ct[k] < 0 ? -ct[k] : ct[k]) > mx)
            mx = ct[k] < 0 ? -ct[k] : ct[k];
        r.r = (int32_t)(65536.0f * ep_exp2((float)ct[k] / 1200.0f));
        ep_trace(EP_X0, EP_W, cy, amp, ep_saw_r_at, &r, 3u, ep_fade(18 + (uint32_t)g[k] * 30u / 5166u), 0);
    }
    if (s.gx || s.g1 || s.g2)
        ep_trace(EP_X0, EP_W, cy, amp, ep_sub_at, &s, 3u, ep_fade(40), 0);
    ep_trace(EP_X0, EP_W, cy, amp, ep_saw_at, 0, 3u, T_THEME, 1);
    ep_cati(cap, n, 0);                                 /* "5 saws, 12.2 st apart, a sub" */
    ep_cat(cap, n > 1 ? " saws" : " saw");
    if (n > 1) {
        ep_cat(cap, ", ");
        if (mx >= 100) {
            ep_cati(cap, mx / 100, 0);
            ep_cat(cap, ".");
            ep_cati(cap, mx / 10 % 10, 0);
            ep_cat(cap, " st apart");
        } else {
            ep_cati(cap, mx, 0);
            ep_cat(cap, " ct apart");
        }
    }
    if (s.gx || s.g1 || s.g2)
        ep_cat(cap, ", a sub");
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
static void ep_ens(const int16_t *p, int32_t h, char *cap)
{
    int32_t k, n = 0, y0 = 8, y1 = h - 6, ly[4], nl = 0, iv4[4], lo = 0, hi = 12 * 64;
    for (k = 0; k < 4; k++) {                           /* (the intervals first: the ladder spans them) */
        iv4[k] = 0;
        if (k) {
            uint32_t xs = (uint32_t)clamp(p[P_E0 + k - 1], 0, 32), j = 0;
            while (j < 127u && T_ENS_PIDX[j] < xs) j++;
            iv4[k] = T_ENS_PCH[j];
            if (iv4[k] == ENS_OFF) continue;
            if (iv4[k] < lo) lo = iv4[k];
            if (iv4[k] > hi) hi = iv4[k];
        }
    }
    /* the chord: a bar a sounding oscillator at its interval (semitones from the root, -12 .. +12), the root filled */
    for (k = 0; k < 4; k++) {
        int32_t iv = 0, y, x;
        char b[8];
        if (k) {
            uint32_t xs = (uint32_t)clamp(p[P_E0 + k - 1], 0, 32), j = 0;
            while (j < 127u && T_ENS_PIDX[j] < xs) j++;
            iv = T_ENS_PCH[j];
            if (iv == ENS_OFF)
                continue;
        }
        n++;
        y = y1 - (iv - lo) * (y1 - y0) / (hi - lo);
        x = 50 + k * 4;
        if (k) {
            cv_rrect(x, y - 4, 228 - x - (3 - k) * 4, 9, 4, T_THEME, T_BG);
            cv_rrect(x + 1, y - 3, 228 - x - (3 - k) * 4 - 2, 7, 3, ep_fade(30), T_THEME);
        } else {
            cv_rrect(x, y - 4, 228 - x - 12, 9, 4, T_THEME, T_BG);
        }
        {   /* its interval, unless one is drawn within a line of it */
            int clash = 0;
            for (int32_t j = 0; j < nl; j++)
                clash |= y > ly[j] - 12 && y < ly[j] + 12;
            if (!clash) {
                if (k) {
                    b[0] = iv >= 0 ? '+' : '-';
                    fmt_int(b + 1, (iv < 0 ? -iv : iv) / 64);
                }
                ep_label_r(44, ty_y(y, TY_SM), k ? b : "root", k ? T_MID : T_TEXT);
                ly[nl++] = y;
            }
        }
    }
    ep_cati(cap, n, 0);                                 /* "4 oscillators, chorus 80" */
    ep_cat(cap, n > 1 ? " oscillators, chorus " : " oscillator, chorus ");
    ep_catv(cap, ENGI_SAW + 1u, P_E5, p[P_E5]);
}

/* ----------------------------------------------------------- NOISE --- */
static void ep_noise(const int16_t *p, int32_t h, char *cap)
{
    int32_t x, py = 0, colr = p[P_E1], res = p[P_E3], top = 4, bot = h - 2;
    float k = (float)(8192 - res * 7800 / 127) / 4096.0f, mix = (float)res / 127.0f;
    float fc = 20000.0f * ep_exp2((float)(p[P_E2] - 127) * 0.86f / 12.0f);
    const char *const MODE[4] = {"anlg", "dust", "lfsr", "meta"};
    cv_rect(EP_X0, bot, EP_W, 1, T_LINE);
    for (x = 0; x < EP_W; x++) {                        /* 20 Hz .. 20 kHz, ten octaves across */
        float oct = (float)x * 10.0f / (float)EP_W, f = 20.0f * ep_exp2(oct), r = f / fc, r2 = r * r;
        float den = (1.0f - r2) * (1.0f - r2) + k * k * r2, lp = 1.0f / den, bp = k * k * r2 / den;
        float a = (1.0f - mix) * ep_exp2(0.5f * ep_log2(lp)) + mix * ep_exp2(0.5f * ep_log2(bp));
        float db = 6.0206f * ep_log2(a) - (float)colr * 6.0f / 127.0f * (oct - 5.6f);   /* COLR: 0 .. -6 dB/oct */
        int32_t y = top * 16 + (int32_t)((12.0f - db) * (float)(bot - top) * 16.0f / 60.0f);   /* +12 .. -48 dB */
        y = clamp(y, top * 16, bot * 16);
        cv_rect(EP_X0 + x, y / 16, 1, bot - y / 16, ep_fade(24));
        cv_vspan_aa(EP_X0 + x, x ? py : y, y, T_THEME);
        py = y;
    }
    ep_cat(cap, MODE[(uint32_t)p[P_E0] & 3u]);          /* "anlg · its own filter, res 79" */
    ep_cat(cap, res > 64 ? " \xb7 band-pass, res " : " \xb7 low-pass, res ");
    ep_catv(cap, 11u, P_E3, res);
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
static void ep_fmtone(const int16_t *p, int32_t h, char *cap)
{
    uint32_t alg = (uint32_t)p[P_E0] & 7u, o, i;
    const param_desc_t *dr = param_desc_of(ENGI_FMTONE, P_E1);
    static const char *const NM[4] = {"B2", "B1", "A", "C"};
    int32_t bw = 38, bh = 20, x[4], y[4], lv[4], step = (h - bh) / 2;
    char b[12];
    lv[0] = lv[1] = TSEL->p[P_FM2_LEVEL];
    lv[2] = TSEL->p[P_FM1_LEVEL];
    lv[3] = 127;
    for (o = 0; o < 4u; o++) {
        x[o] = 16 + EP_ALG[alg].col[o] * 52;
        y[o] = EP_ALG[alg].row[o] * step;
    }
    for (i = 0; i < EP_ALG[alg].ne; i++) {               /* modulator -> modulated: from its right edge */
        uint32_t a = EP_ALG[alg].edge[i][0], c = EP_ALG[alg].edge[i][1];
        cv_line_t(x[a] + bw, y[a] + bh / 2, x[c], y[c] + bh / 2, T_MID, 1);
    }
    for (o = 0; o < 4u; o++) {
        int car = (EP_ALG[alg].car >> o) & 1u;
        if (car)                                        /* a carrier: to the output, filled */
            cv_line_t(x[o] + bw, y[o] + bh / 2, 228, y[o] + bh / 2, T_MID, 1);
        cv_rrect(x[o], y[o], bw, bh, 4, car ? T_THEME : T_MID, T_BG);
        cv_rrect(x[o] + 1, y[o] + 1, bw - 2, bh - 2, 3, car ? T_THEME : T_BG, car ? T_THEME : T_MID);
        if (!car && lv[o]) {                             /* a modulator's level: its fill */
            int32_t fh = (bh - 4) * lv[o] / 127;
            cv_rect(x[o] + 2, y[o] + bh - 2 - fh, bw - 4, fh, ep_fade(30));
        }
        cv_text_c(x[o] + bw / 2, ty_y(y[o] + bh / 2, TY_SM), TY_SM, NM[o], car ? T_INK : T_TEXT, car ? T_THEME : T_BG);
    }
    cv_rect(228, y[0] < y[3] ? 2 : 2, 1, h - 4, T_LINE);   /* the output */
    if (p[P_E6])                                        /* B2's feedback: a loop over it */
        cv_frame(x[0] + 10, y[0] > 3 ? y[0] - 3 : 0, bw - 20, 4, T_THEME);
    ep_cat(cap, "algorithm ");                          /* "algorithm 2: ratios 1 · 1 · 4" */
    ep_cati(cap, (int32_t)alg + 1, 0);
    if (dr && dr->names) {
        ep_cat(cap, ": ratios ");
        for (o = 0; o < 3u; o++) {
            int32_t ri = o == 0u ? p[P_E1] : o == 1u ? p[P_E2] : p[P_E3];
            str_cpy(b, dr->names[clamp(ri, 0, 22)], sizeof b);
            ep_cat(cap, b);
            if (o < 2u) ep_cat(cap, " \xb7 ");
        }
    }
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
static void ep_dual(const int16_t *p, int32_t h, char *cap)
{
    static const char *const MODE[4] = {"mix", "ring", "FM", "FM + ring"};
    uint32_t c = (uint32_t)clamp(p[P_E4], 0, 79), m = c % 40u;
    ep_dual_t d;
    int32_t lane = (h - 4) / 2, x, py = 0;
    d.w = (m % 10u) / 5u;
    d.w2 = m % 5u;
    d.mode = m / 10u;
    d.ratio_q8 = (int32_t)(256.0f * ep_exp2((float)p[P_E1] / 12.0f));
    for (d.osc = 1u; d.osc <= 2u; d.osc++) {            /* each oscillator under its decay */
        int32_t cy = lane / 2 + (int32_t)(d.osc - 1u) * (lane + 4), dec = d.osc == 1u ? p[P_E2] : p[P_E3];
        float tau = 0.04f + (float)dec * (float)dec / 3000.0f;   /* (the decay's time constant, the picture's widths) */
        for (x = 0; x < EP_W; x++) {
            uint32_t ph = (uint32_t)(x * 6 * 65536 / EP_W) & 0xFFFFu;
            float env = ep_exp2(-(float)x / (float)EP_W / tau * 1.44f);
            int32_t v = ep_dual_at(&d, ph), y = cy * 16 - (int32_t)((float)v * env) * (lane / 2 - 2) / 2048;
            cv_vspan_aa(EP_X0 + x, x ? py : y, y, d.osc == 1u ? T_THEME : ep_fade(55));
            py = y;
        }
    }
    ep_cat(cap, MODE[d.mode & 3u]);                     /* "mix, decays 476 / 18 ms, phase reset" */
    ep_cat(cap, ", decays ");
    ep_catv(cap, ENGI_DUAL, P_E2, p[P_E2]);
    ep_cat(cap, " / ");
    ep_catv(cap, ENGI_DUAL, P_E3, p[P_E3]);
    if (c >= DUAL_RESET_FROM)
        ep_cat(cap, ", reset");
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
static void ep_analog4(const int16_t *p, int32_t h, char *cap)
{
    ep_a4_t o1, o2;
    int32_t cy = h / 2, amp = h / 2 - 4, l1 = TSEL->p[P_FM1_LEVEL], l2 = TSEL->p[P_FM2_LEVEL];
    const param_desc_t *dw = param_desc_of(ENGI_ANA4, P_E0);
    o1.pw = o2.pw = (uint32_t)clamp(p[P_E1] * 32, 16, 4080) << 20;
    o1.w = (uint32_t)p[P_E0] & 3u;
    o2.w = (uint32_t)p[P_E4] & 3u;
    o1.ratio_q8 = (int32_t)(256.0f * ep_exp2((float)p[P_E2] / 12.0f));
    o2.ratio_q8 = (int32_t)(256.0f * ep_exp2((float)p[P_E6] / 12.0f + (float)p[P_E7] / 1200.0f));
    cv_rect(EP_X0, cy, EP_W, 1, T_LINE);
    if (p[P_E3])                                        /* the sub: a sine an octave under osc 1 */
        ep_trace(EP_X0, EP_W, cy, amp * p[P_E3] / 127, ep_sine_oct_at, 0, 2u, ep_fade(35), 0);
    if (l2)
        ep_trace(EP_X0, EP_W, cy, amp * (l2 + 20) / 147, ep_a4_at, &o2, 2u, ep_fade(55), 0);
    if (l1)
        ep_trace(EP_X0, EP_W, cy, amp * (l1 + 20) / 147, ep_a4_at, &o1, 2u, T_THEME, 1);
    if (dw && dw->names) {                              /* "saw + pls, osc 2 +7 st, a sub" */
        ep_catl(cap, dw->names[o1.w]);
        ep_cat(cap, " + ");
        ep_catl(cap, dw->names[o2.w]);
        ep_cat(cap, ", ");
    }
    ep_cat(cap, "osc 2 ");
    ep_cati(cap, p[P_E6], 1);
    ep_cat(cap, " st");
    if (p[P_E3])
        ep_cat(cap, ", a sub");
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
static void ep_lofi(const int16_t *p, int32_t h, char *cap)
{
    static const uint16_t LEV[5] = {16, 8, 256, 2, 16};
    ep_lofi_t l;
    uint32_t chip = (uint32_t)clamp(p[P_E0], 0, 4);
    int32_t cy = h / 2;
    l.wave = (uint32_t)clamp(p[P_E1], 0, 4);
    l.levels = LEV[chip];
    l.duty = 4096 + p[P_E2] * 448;
    if (chip == CHIP_STEP) {                            /* STEP's duties: 12.5 / 25 / 50 / 75 % */
        static const int32_t D[4] = {8192, 16384, 32768, 49152};
        l.duty = D[(p[P_E2] >> 5) & 3];
    }
    l.tab = (uint32_t)p[P_E2] >> 3;
    cv_rect(EP_X0, cy, EP_W, 1, T_LINE);
    ep_trace(EP_X0, EP_W, cy, h / 2 - 6, ep_lofi_at, &l, 2u, T_THEME, 1);
    ep_catl(cap, N_CHIP[chip]);                         /* "4bit pls, duty 32" */
    ep_cat(cap, " ");
    ep_catl(cap, N_RWAVE[l.wave]);
    if (l.wave == RW_WRAM) {
        ep_cat(cap, " ");
        ep_catl(cap, N_WRAM[l.tab & 15u]);
    } else if (l.wave == RW_PLS) {
        ep_cat(cap, ", duty ");
        ep_cati(cap, p[P_E2], 0);
    }
}

#endif
/* ------------------------------------------------------------ WHEEL --- */
static void ep_wheel(const int16_t *p, int32_t h, char *cap)
{
    static const char *const FT[9] = {"16", "5", "8", "4", "2", "2", "1", "1", "1"};
    int32_t k, bw = 14, gap = (EP_W - 9 * bw) / 8, top = 2, bot = h - 14;
    (void)cap;
    for (k = 0; k < 9; k++) {                           /* a drawbar a footage: pulled out as far as its level */
        int32_t x = EP_X0 + k * (bw + gap), lv = drw_level(p, (uint32_t)k), l = lv * (bot - top) / 8;
        cv_rrect(x, top, bw, bot - top, 4, T_RAISE, T_BG);
        if (l)
            cv_rrect(x, top, bw, l < 6 ? 6 : l, 4, T_THEME, T_RAISE);
        cv_text_c(x + bw / 2, ty_y(h - 6, TY_SM), TY_SM, FT[k], T_MID, T_BG);
    }
}

/* ------------------------------------------------------------- PHYS --- */
static void ep_phys(const int16_t *p, int32_t h, char *cap)
{
    static const float MEMB[8] = {1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f};
    uint32_t md = (uint32_t)p[P_E0] % 4u, n, i;
    float r[12], a[12], strc = (float)p[P_E1], brit = (float)p[P_E2] / 127.0f;
    int32_t base = h - 2, top = 4;
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
    ep_cat(cap, MN[md]);                                /* "modal · 4 partials, damp 83" */
    ep_cat(cap, " \xb7 ");
    ep_cati(cap, (int32_t)n, 0);
    ep_cat(cap, " partials, damp ");
    ep_catv(cap, 9u, P_E3, p[P_E3]);
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
static void ep_drum(const int16_t *p, int32_t h, char *cap)
{
    uint32_t l;
    int32_t pw = 48, ph = (h - 8) / 2;
    (void)p;
    for (l = 0; l < NLANE; l++) {                       /* a pad a lane, lit as it hits */
        int32_t x = 16 + (int32_t)(l % 4u) * (pw + 6), y = 2 + (int32_t)(l / 4u) * (ph + 4);
        int lit = ep_drum_lit[l] != 0u;
        uint16_t f = lit ? ep_fade(35) : T_SURF;
        cv_rrect(x, y, pw, ph, 5, lit ? T_THEME : T_LINE, T_BG);
        cv_rrect(x + 1, y + 1, pw - 2, ph - 2, 4, f, lit ? T_THEME : T_LINE);
        {
            const char *nm = drum_lane_abbr(TSEL, l);
            cv_text_c(x + pw / 2, ty_y(y + ph / 2, TY_SM), TY_SM, nm, T_TEXT, f);
        }
    }
    ep_cat(cap, "keys play its sounds");
}

/* ------------------------------------------------------------- MIDI --- */
static void ep_midi(const int16_t *p, int32_t h, char *cap)
{
    char b[16];
    int32_t cx = 50, cy = h / 2, k;
    static const int8_t PX[5] = {-12, -9, 0, 9, 12}, PY[5] = {2, -8, -12, -8, 2};
    cv_ring(cx, cy, 44, T_THEME);                       /* the DIN socket, the arrow out */
    cv_ring(cx, cy, 42, T_THEME);
    for (k = 0; k < 5; k++)
        cv_disc(cx + PX[k], cy + PY[k], 5, T_THEME, 0);
    cv_rect(cx + 6, cy + 13, 8, 3, T_THEME);
    cv_line_t(84, cy, 128, cy, T_MID, 1);
    cv_line_t(122, cy - 4, 128, cy, T_MID, 1);
    cv_line_t(122, cy + 4, 128, cy, T_MID, 1);
    cv_text_on(134, ty_y(cy - 8, TY_MD), TY_MD, "notes, CCs", T_TEXT, T_BG);
    str_cpy(b, "out on ch ", sizeof b);
    fmt_int(b + str_len(b), p[P_E0]);
    ep_label(134, ty_y(cy + 9, TY_SM), b, T_MID);
    ep_cat(cap, p[P_E1] ? "no sound of its own; USB and TRS" : "no sound of its own; USB");
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
static void ep_draw(uint32_t e, int32_t h, char *cap)
{
    const int16_t *p = TSEL->p + 0;
    cap[0] = 0;
    if (e == ENGI_WAV) ep_wav(p, h, cap);
    else if (e == ENGI_SAW) ep_saw(p, h, cap);
    else if (e == ENGI_SAW + 1u) ep_ens(p, h, cap);
    else if (e == 11u) ep_noise(p, h, cap);
    else if (e == ENGI_FMTONE) ep_fmtone(p, h, cap);
    else if (e == ENGI_DUAL) ep_dual(p, h, cap);
    else if (e == ENGI_ANA4) ep_analog4(p, h, cap);
#if MELODEE_FELUCCA_ENGINES
    else if (e == 3u) ep_lofi(p, h, cap);
#endif
    else if (e == 7u) ep_wheel(p, h, cap);
    else if (e == 9u) ep_phys(p, h, cap);
    else if (e == ENGI_DRUM) ep_drum(p, h, cap);
    else ep_midi(p, h, cap);
}

/* the engine's picture in its frame (1.3.7, the mockup's): a title row (the engine in small letters, the sound at the
 * right, dim), the picture, its caption (what the values mean, small and dim) */
#define EP_TITLE_H 18
#define EP_CAP_H 14
static void ep_frame(uint32_t e, int32_t h)
{
    char nm[EP_CAP], cap[EP_CAP], en[EP_CAP];
    int32_t ex;
    en[0] = 0;
    ep_catl(en, ENGINES[eng_idx(e)]->name);
    ex = cv_text_on(9, ty_y(EP_TITLE_H / 2, TY_MD), TY_MD, en, T_TEXT, T_BG);
    sound_name(TSEL, cap);
    nm[0] = 0;
    if (!str_eq(cap, ENGINES[eng_idx(e)]->name))
        ep_catl(nm, cap);                               /* (the sound in small letters, as the mockup) */
    if (nm[0])
        cv_free_text(231 - (text_w(TY_SM, nm) < 231 - ex - 12 ? text_w(TY_SM, nm) : 231 - ex - 12),
                     ty_y(EP_TITLE_H / 2 + 1, TY_SM), TY_SM, nm, T_MID, T_BG, 231 - ex - 12);
    cv_oy = EP_TITLE_H;
    ep_draw(e, h - EP_TITLE_H - EP_CAP_H, cap);
    cv_oy = 0;
    if (cap[0])
        cv_free_text(12, ty_y(h - EP_CAP_H / 2, TY_SM), TY_SM, cap, T_MID, T_BG, 216);
}
