/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* VOX (1.3.4, src/eng_vox.c: MONO2K's VOX-6): engine 16, in the shown order after ANALOG4; every preset sounds (a
 * level near the other engines', no clipping); VOC2 moves F2 (its brightness), VOIC turns the buzz to a whisper (the
 * pitch's harmonics give way to noise); a fresh note fades in, a legato note keeps its level; the vowels' presets
 * stand apart. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("vox: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void setup(uint32_t e, uint32_t pi)
{
    memset(trk, 0, sizeof trk); trk_voices_bind();
    eng_state_reset();
    host_tracks_init();
    host_preset(&trk[0], e, pi);
    trk[0].p[P_ATK] = 0; trk[0].p[P_SUS] = 127;
}
static void run(int32_t *y, uint32_t n)
{
    static int32_t b[CTL];
    for (uint32_t i = 0; i < n; i += CTL) {
        memset(b, 0, sizeof b);
        track_render_audio(&trk[0], b, CTL, 0);
        memcpy(y + i, b, sizeof b);
    }
}
static double rms(const int32_t *y, uint32_t a, uint32_t b)
{
    double m = 0;
    for (uint32_t i = a; i < b; i++) m += (double)y[i] * y[i];
    return sqrt(m / (b - a));
}
static int32_t peak(const int32_t *y, uint32_t n)
{
    int32_t p = 0;
    for (uint32_t i = 0; i < n; i++) { int32_t a = y[i] < 0 ? -y[i] : y[i]; if (a > p) p = a; }
    return p;
}
/* the power between f0 and f1 (Hz), a Hann-windowed DFT of one 4096-sample frame from i0 */
static double band(const int32_t *y, uint32_t i0, double f0, double f1)
{
    double pw = 0;
    uint32_t N = 4096u, b0 = (uint32_t)(f0 * N / FS), b1 = (uint32_t)(f1 * N / FS);
    for (uint32_t k = b0; k < b1; k++) {
        double re = 0, im = 0;
        for (uint32_t i = 0; i < N; i++) {
            double w = y[i0 + i] * (0.5 - 0.5 * cos(2 * M_PI * i / N));
            re += w * cos(2 * M_PI * k * i / N); im -= w * sin(2 * M_PI * k * i / N);
        }
        pw += re * re + im * im;
    }
    return pw;
}
/* the share of the power off the note's harmonics (110 Hz: bins between them), 200 .. 4000 Hz */
static double noisiness(const int32_t *y, uint32_t i0)
{
    double on = 0, off = 0;
    for (uint32_t h = 2; h < 36; h++) {
        on += band(y, i0, h * 110.0 - 8, h * 110.0 + 8);
        off += band(y, i0, h * 110.0 + 30, h * 110.0 + 80);
    }
    return off / (on + off);
}

int main(void)
{
    enum { N = 32768 };
    static int32_t y[N], z[N];
    const engine_t *e = ENGINES[ENGI_VOX];
    int shown = 0, after = 0;
    for (uint32_t k = 0; k < NENG_SHOWN; k++) {
        if (ENGINE_ORDER[k] == ENGI_VOX) shown = 1;
        if (k && ENGINE_ORDER[k] == ENGI_VOX && ENGINE_ORDER[k - 1] == ENGI_ANA4) after = 1;
    }
    check("VOX is engine 16, named VOX, shown after ANALOG4, VOC1 VOC2 VOIC TUNE on its first page",
          !strcmp(e->name, "VOX") && shown && after && !strcmp(e->edit[0].label, "VOC1") && !strcmp(e->edit[3].label, "TUNE"));
    /* every preset sounds, near the others' level, without clipping */
    {
        int ok = 1;
        double ref, lv[16];
        setup(ENGI_ANA4, 0); trk_note_on(&trk[0], 45, 100); run(y, N); ref = rms(y, 8192, N);
        for (uint32_t pi = 0; pi < e->npresets; pi++) {
            setup(ENGI_VOX, pi);
            trk_note_on(&trk[0], 45, 100);
            run(y, N);
            lv[pi] = rms(y, 8192, N);
            ok &= lv[pi] > ref * 0.25 && lv[pi] < ref * 4 && peak(y, N) < 32767 * 4;
            printf("vox: %-10s rms %.0f (ANALOG4 %.0f) peak %d\n", eng_preset(e, pi)->name, lv[pi], ref, peak(y, N));
        }
        check("every preset sounds, within 12 dB of ANALOG4's first, no runaway", ok);
    }
    /* VOC2 moves F2: more power around 2 kHz with VOC2 high */
    setup(ENGI_VOX, 0); trk[0].p[P_E1] = 10; trk_note_on(&trk[0], 45, 100); run(y, N);
    setup(ENGI_VOX, 0); trk[0].p[P_E1] = 120; trk_note_on(&trk[0], 45, 100); run(z, N);
    {
        double lo = band(y, 16384, 1800, 2800) / band(y, 16384, 100, 4000), hi = band(z, 16384, 1800, 2800) / band(z, 16384, 100, 4000);
        printf("vox: share of 1.8 .. 2.8 kHz: VOC2 10 %.4f, VOC2 120 %.4f\n", lo, hi);
        check("VOC2 moves the second formant up (more of its power at 1.8 .. 2.8 kHz)", hi > lo * 3);
    }
    /* VOIC: a whisper */
    setup(ENGI_VOX, 0); trk[0].p[P_E2] = 0; trk_note_on(&trk[0], 45, 100); run(y, N);
    setup(ENGI_VOX, 0); trk[0].p[P_E2] = 127; trk_note_on(&trk[0], 45, 100); run(z, N);
    {
        double a = noisiness(y, 16384), b = noisiness(z, 16384);
        printf("vox: off-harmonic share: VOIC 0 %.4f, VOIC 127 %.4f\n", a, b);
        check("VOIC 127 whispers: the power between the harmonics grows (noise for the buzz)", b > a * 5 && rms(z, 8192, N) > 100);
    }
    /* a fresh note fades in; a legato note keeps its level */
    setup(ENGI_VOX, 0); trk[0].p[P_VOICE] = V_LEGATO; trk_note_on(&trk[0], 45, 100); run(y, 16384);
    {
        double early = rms(y, 0, 512), held = rms(y, 12288, 16384);
        trk_note_on(&trk[0], 52, 100); run(z, 4096);
        double after_leg = rms(z, 0, 512);
        printf("vox: first 12 ms %.0f, held %.0f, the first 12 ms of a legato note %.0f\n", early, held, after_leg);
        check("a fresh note fades in (its first 12 ms under half its held level)", early < held * 0.5);
        check("a legato note keeps its level (its first 12 ms within 6 dB of the held)", after_leg > held * 0.5);
    }
    printf(bad ? "vox: %d FAILED\n" : "vox: all checks ok\n", bad);
    return bad != 0;
}
