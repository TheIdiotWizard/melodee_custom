/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* MONO2K's seven engines (src/eng_m2k.c) on the host, through hostsim.c.
 *   build/host/m2k_test
 * 1. every preset of SAW ENS STAT PAR DYN WAV NOIS sounds: one note at velocity 127, its peak short of the voice clip.
 * 2. the first note after silence: a note, its release run out, then idle blocks rendered the way the mixer does
 *    (track_render_audio, clear_idle 0) over a buffer full of other audio; the next note's first 50 ms peak no
 *    higher than the same note's from a fresh part (+25 %). ENS's chorus once took that buffer in and played it
 *    back at the next note (a burst, often a clip).
 * 3. NOIS: RED's colour (the power of an octave at 300 Hz against one at 4.8 kHz: white -12 dB, pink 0, red +12, each
 *    +-3); two voices are two noises (their sum's power twice one's, +-15 %); STON without SPREAD leaves the middle the
 *    first noise alone (sample for sample as ST 0), with SPREAD it adds the second to the left side only. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("m2k: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void setup(uint32_t e, uint32_t pi)
{
    memset(trk, 0, sizeof trk); trk_voices_bind();
    eng_state_reset();
    host_tracks_init();
    host_preset(&trk[0], e, pi);
    trk[0].p[P_VOICE] = V_POLY;
}

static int32_t held(void)
{
    for (uint32_t k = 0; k < NVOICE; k++) if (trk[0].v[k].active) return 1;
    return 0;
}

/* the peak of n samples rendered as the mixer renders (idle blocks over a dirty buffer) */
static int32_t render_peak(uint32_t n, int dirty)
{
    static int32_t b[CTL];
    int32_t pk = 0;
    for (uint32_t i = 0; i < n; i += CTL) {
        if (dirty) for (uint32_t k = 0; k < CTL; k++) b[k] = (k & 4u) ? 28000 : -28000;
        uint32_t nr = track_render_audio(&trk[0], b, CTL, 0);
        if (nr) for (uint32_t k = 0; k < CTL; k++) { int32_t a = b[k] < 0 ? -b[k] : b[k]; if (a > pk) pk = a; }
    }
    return pk;
}

/* NOIS: one note held, ST st, RED red, STON on; the filter out of the way; n samples of the voice sum (and the left
 * side, with SPREAD) */
static void nois_render(int st, int red, int on, int spread, int32_t *y, int32_t *left, uint32_t n, uint32_t notes)
{
    setup(ENGI_NOIS, 0);
    trk[0].p[P_E0] = (int16_t)st; trk[0].p[P_E1] = (int16_t)red; trk[0].p[P_E2] = (int16_t)on;
    trk[0].p[P_ATK] = 0; trk[0].p[P_SUS] = 127; trk[0].p[P_REL] = 20; trk[0].p[P_ED_FLT] = 0;
    for (uint32_t k = 0; k < notes; k++) trk_note_on(&trk[0], 60 + 4 * k, 110);
    static int32_t b[CTL];
    uint32_t warm = FS / 10u / CTL;
    for (uint32_t i = 0; i < warm + n / CTL; i++) {
        spread_on = (uint8_t)spread;
        track_render_audio(&trk[0], b, CTL, 1);
        if (i >= warm) for (uint32_t k = 0; k < CTL; k++) {
            y[(i - warm) * CTL + k] = b[k];
            if (left) left[(i - warm) * CTL + k] = spread_buf[k];
        }
    }
    spread_on = 0;
}
/* the power of y between f0 and 2 f0 (a Hann-windowed DFT of 4096-sample frames, averaged) */
static double band(const int32_t *y, uint32_t n, double f0)
{
    double pw = 0;
    uint32_t N = 4096u, b0 = (uint32_t)(f0 * N / FS), b1 = 2u * b0;
    for (uint32_t f = 0; f + N <= n; f += N)
        for (uint32_t k = b0; k < b1; k++) {
            double re = 0, im = 0;
            for (uint32_t i = 0; i < N; i++) {
                double w = y[f + i] * (0.5 - 0.5 * cos(2 * M_PI * i / N));
                re += w * cos(2 * M_PI * k * i / N); im -= w * sin(2 * M_PI * k * i / N);
            }
            pw += re * re + im * im;
        }
    return pw;
}
static double power(const int32_t *y, uint32_t n) { double s = 0; for (uint32_t i = 0; i < n; i++) s += (double)y[i] * y[i]; return s / n; }

int main(void)
{
    static const char *const NAME[7] = {"SAW", "ENS", "STAT", "PAR", "DYN", "WAV", "NOIS"};
    int ok = 1;
    for (uint32_t k = 0; k < 7u; k++) {
        const engine_t *e = ENGINES[ENGI_SAW + k];
        for (uint32_t pi = 0; pi < e->npresets; pi++) {
            setup(ENGI_SAW + k, pi);
            trk_note_on(&trk[0], 48, 127);
            int32_t pk = render_peak(FS / 2u, 0);
            if (!(pk > 500 && pk < 32000)) { printf("m2k: %s %s peak %d\n", NAME[k], e->presets[pi].name, (int)pk); ok = 0; }
        }
    }
    check("every preset of the seven sounds: one note at velocity 127, short of the voice's clip (32000)", ok);

    ok = 1;
    for (uint32_t k = 0; k < 7u; k++) {
        const engine_t *e = ENGINES[ENGI_SAW + k];
        for (uint32_t pi = 0; pi < e->npresets; pi++) {
            setup(ENGI_SAW + k, pi);
            trk[0].p[P_REL] = 20;
            trk_note_on(&trk[0], 60, 110);
            int32_t fresh = render_peak(FS / 20u, 0);   /* (NOIS: a noise's peak varies, its level does not) */
            render_peak(FS / 4u, 0);
            trk_note_off(&trk[0], 60);
            uint32_t idle = 0;
            for (uint32_t s = 0; s < 10u * FS / CTL && idle < FS / 2u / CTL; s++) {
                render_peak(CTL, 1);
                idle = held() ? 0 : idle + 1u;
            }
            trk_note_on(&trk[0], 60, 110);
            int32_t again = render_peak(FS / 20u, 0);
            if (!idle || again > fresh + fresh / (k == 6u ? 2 : 4)) {
                printf("m2k: %s %s first note %d, after silence %d (idle %u)\n", NAME[k], e->presets[pi].name,
                       (int)fresh, (int)again, idle);
                ok = 0;
            }
        }
    }
    check("the first note after silence (idle blocks over a dirty mixer buffer) starts as from a fresh part", ok);

    {
        enum { NS = 4096u * 6u };
        static int32_t y[NS], z[NS], l[NS], l0[NS];   /* (l0: the voice on the left side alone) */
        static const struct { int red; double db; } COL[3] = {{0, -12}, {64, 0}, {127, 12}};
        ok = 1;
        for (uint32_t c = 0; c < 3u; c++) {
            nois_render(0, COL[c].red, 0, 0, y, 0, NS, 1);
            double db = 10 * log10(band(y, NS, 300) / band(y, NS, 4800));
            printf("m2k: NOIS RED %3d: an octave at 300 Hz against 4.8 kHz %+.1f dB (want %+.0f)\n", COL[c].red, db, COL[c].db);
            ok &= fabs(db - COL[c].db) <= 3;
        }
        check("NOIS RED: white -12 dB, pink 0, red +12 over four octaves (+-3)", ok);
        nois_render(0, 0, 0, 0, y, 0, NS, 1); double p1 = power(y, NS);
        nois_render(0, 0, 0, 0, y, 0, NS, 2); double p2 = power(y, NS);
        check("NOIS: two voices are two noises (their power twice one's, +-15 %)", fabs(p2 / p1 - 2) <= 0.3);
        nois_render(0, 64, 0, 0, y, 0, NS, 1);
        nois_render(127, 64, 1, 0, z, 0, NS, 1);
        check("NOIS STON without SPREAD: the middle is the first noise alone (as ST 0, sample for sample)", !memcmp(y, z, sizeof y));
        nois_render(127, 64, 0, 0, z, 0, NS, 1);
        check("NOIS ST 127 without STON: the second noise in the middle (louder than ST 0)", power(z, NS) > 1.3 * power(y, NS));
        nois_render(0, 64, 0, 1, y, l0, NS, 1);          /* (one voice: SPREAD's left side, the whole voice) */
        nois_render(127, 64, 1, 1, z, l, NS, 1);
        double dd = 0;
        for (uint32_t i = 0; i < NS; i++) dd += (double)(l[i] - z[i]) * (l[i] - z[i]);
        printf("m2k: NOIS STON with SPREAD: the left side's second noise %.0f, the voice %.0f\n", sqrt(dd / NS), sqrt(power(z, NS)));
        check("NOIS STON with SPREAD: the middle the first noise (as ST 0), the left side the second besides",
              !memcmp(y, z, sizeof y) && !memcmp(l0, y, sizeof y) && dd / NS > 0.2 * power(z, NS));
    }

    printf(bad ? "m2k: FAILED\n" : "m2k: SAW ENS STAT PAR DYN WAV NOIS: presets, the first note after silence, NOIS's colour and stereo: OK\n");
    return bad != 0;
}
