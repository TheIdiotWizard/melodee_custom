/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* ENV > DRIVE (1.3.4, dsp.c voice_drive, voice.c): each voice's drive before its amp. DRIVE 0 is the voice as it was
 * (bit for bit); DRIVE adds harmonics; VEL makes a soft note drive less (127: a velocity-1 note nearly clean), 0 every
 * note alike; FUZZ leaves no DC; each SHAPE sounds; a factory preset sets it to 0; the drive effect types are no
 * longer offered in the FX picker, a slot holding one keeps it. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("drive: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* ANALOG4's first preset, one note: n samples of it */
static void render(uint32_t e, int drive, int shape, int vel_amt, int vel, int32_t *y, uint32_t n)
{
    static int32_t b[CTL];
    memset(trk, 0, sizeof trk); trk_voices_bind();
    eng_state_reset();
    host_tracks_init();
    host_preset(&trk[0], e, 0);
    trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_ATK] = 0; trk[0].p[P_SUS] = 127;
    trk[0].p[P_DRIVE] = (int16_t)drive; trk[0].p[P_DSHAPE] = (int16_t)shape; trk[0].p[P_DVEL] = (int16_t)vel_amt;
    trk_note_on(&trk[0], 45, (uint32_t)vel);
    for (uint32_t i = 0; i < n; i += CTL) {
        memset(b, 0, sizeof b);
        track_render_audio(&trk[0], b, CTL, 0);
        memcpy(y + i, b, sizeof b);
    }
}

/* the share of the power above the 8th harmonic (110 Hz: above 880 Hz), crude: first differences' power against the
 * signal's (a brighter, more harmonic sound has a larger one) */
static double bright(const int32_t *y, uint32_t n)
{
    double d = 0, s = 0;
    for (uint32_t i = 4096; i < n; i++) { double a = y[i], b = y[i] - y[i - 1]; s += a * a; d += b * b; }
    return s > 0 ? d / s : 0;
}
static double mean(const int32_t *y, uint32_t n)
{
    double m = 0;
    for (uint32_t i = 4096; i < n; i++) m += y[i];
    return m / (n - 4096);
}
static double rms(const int32_t *y, uint32_t n)
{
    double m = 0;
    for (uint32_t i = 4096; i < n; i++) m += (double)y[i] * y[i];
    return sqrt(m / (n - 4096));
}

int main(void)
{
    enum { N = 24576 };
    static int32_t a[N], b[N], c[N], d[N];
    uint32_t e = ENGI_ANA4, k;
    /* DRIVE 0: the voice as it was */
    render(e, 0, DS_SOFT, 0, 100, a, N);
    render(e, 0, DS_FUZZ, 127, 100, b, N);
    check("DRIVE 0 leaves the voice as it was, whatever SHAPE and VEL", !memcmp(a, b, sizeof a));
    /* DRIVE adds harmonics */
    render(e, 110, DS_SOFT, 0, 100, b, N);
    printf("drive: brightness clean %.4f, SOFT 110 %.4f; rms %.0f, %.0f\n", bright(a, N), bright(b, N), rms(a, N), rms(b, N));
    check("DRIVE 110 (SOFT) brightens the voice (more power in its upper harmonics)", bright(b, N) > bright(a, N) * 1.15);
    /* each shape sounds, and differently */
    {
        static int32_t s[4][N];
        int differ = 1, loud = 1;
        for (k = 0; k < 4u; k++) render(e, 110, (int)k, 0, 100, s[k], N);
        for (k = 0; k < 4u; k++) {
            loud &= rms(s[k], N) > rms(a, N) * 0.25;
            for (uint32_t j = k + 1; j < 4u; j++) differ &= memcmp(s[k], s[j], sizeof s[k]) != 0;
        }
        check("SOFT HARD FOLD FUZZ each sound, each differently", differ && loud);
        check("FUZZ leaves no DC (its blocker), within 1 % of its rms", fabs(mean(s[DS_FUZZ], N)) < rms(s[DS_FUZZ], N) * 0.01);
    }
    /* VEL: a soft note drives less */
    render(e, 110, DS_HARD, 127, 20, c, N);
    render(e, 110, DS_HARD, 0, 20, d, N);
    {
        double rc = rms(c, N), rd = rms(d, N);
        printf("drive: velocity 20, VEL 127 brightness %.4f (rms %.0f), VEL 0 %.4f (rms %.0f)\n", bright(c, N), rc, bright(d, N), rd);
        check("VEL 127: a soft note is driven less than with VEL 0 (less bright)", bright(c, N) < bright(d, N) * 0.95);
    }
    render(e, 110, DS_HARD, 127, 127, c, N);
    render(e, 110, DS_HARD, 0, 127, d, N);
    check("VEL: a full-velocity note drives the same whatever VEL", !memcmp(c, d, sizeof c));
    /* a factory preset sets DRIVE to 0 */
    check("a factory preset sets DRIVE 0 (ui.c apply_preset_to: the common defaults, none of the three kept)",
          !param_kept(P_DRIVE) && !param_kept(P_DSHAPE) && !param_kept(P_DVEL) && TP[P_DRIVE].def == 0 && TP[P_DVEL].def == 0);
    /* the FX picker: no drive offered; a slot holding one keeps it */
    {
        int offered = 0;
        for (k = 0; k < NELEM(FX_ORDER); k++) offered |= FX_ORDER[k] >= FX_SOFT && FX_ORDER[k] <= FX_FUZZ;
        check("the FX picker no longer offers DRIVE (SOFT .. FUZZ)", !offered);
        check("a slot holding a DRIVE keeps it, placed in Dirt (as CRUSH)", fx_family(FX_HARD) == FXF_DIRT && fx_step(&TP[P_FXTYPE], FX_HARD, 0) == FX_HARD);
    }
    printf(bad ? "drive: %d FAILED\n" : "drive: all checks ok\n", bad);
    return bad != 0;
}
