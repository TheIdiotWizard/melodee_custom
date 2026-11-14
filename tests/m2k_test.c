/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* MONO2K's three engines (src/eng_m2k.c), NOISE (src/eng_noise.c, in NOIS's place since 1.2.8; its own checks:
 * noise_test.c) and Mini-Tone's three (src/eng_mt.c) on the host, through hostsim.c.
 *   build/host/m2k_test
 * 1. every preset of SAW ENS WAV NOISE FMTONE DUALVCO ANALOG4 PULS sounds: one note at velocity 127, its peak short of the voice clip.
 * 2. the first note after silence: a note, its release run out, then idle blocks rendered the way the mixer does
 *    (track_render_audio, clear_idle 0) over a buffer full of other audio; the next note's first 50 ms peak no
 *    higher than the same note's from a fresh part (+25 %). ENS's chorus once took that buffer in and played it
 *    back at the next note (a burst, often a clip).
 * 3. (NOIS's colour and stereo: retired with it, 1.2.8)
 * 4. the FILTER page (TRIO's filter): OFF changes nothing (as before it, sample for sample); LP at CUT 40 takes 4.8 kHz
 *    at least 20 dB further down than 300 Hz, HP at CUT 100 300 Hz; ENV -> FLT moves the cutoff, not UNIW, with it on;
 *    on FMTONE, DUALVCO and ANALOG4 LP at CUT 30 takes their 4.8 kHz 12 dB or more down against MODE OFF.
 * 5. MIDI: CC 74 / 71 the FILTER's CUT / RES on these engines; a relative MIDI MODE (REL 1) steps CC 73 ATTACK and
 *    CC 74 from where they are, CC 7 LEVEL and CC 1 (the mod wheel) stay absolute. */
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

/* SAW SUPERSAW, one note held, its FILTER mode / cut / res; n samples of the voice */
static void saw_render(int mode, int cut, int res, int fenv, int32_t *y, uint32_t n)
{
    setup(ENGI_SAW, 0);
    trk[0].p[P_FMODE] = (int16_t)mode; trk[0].p[P_FCUT] = (int16_t)cut; trk[0].p[P_FRES] = (int16_t)res;
    trk[0].p[P_ED_FLT] = (int16_t)fenv; trk[0].p[P_ATK] = 0; trk[0].p[P_SUS] = 127; trk[0].p[P_E3] = 0;   /* (no drift) */
    trk_note_on(&trk[0], 48, 110);
    static int32_t b[CTL];
    uint32_t warm = FS / 10u / CTL;
    for (uint32_t i = 0; i < warm + n / CTL; i++) {
        track_render_audio(&trk[0], b, CTL, 1);
        if (i >= warm) for (uint32_t k = 0; k < CTL; k++) y[(i - warm) * CTL + k] = b[k];
    }
}

/* one of Mini-Tone's engines, its preset 0, one note held, its FILTER mode / cut; n samples of the voice */
static void mt_render(uint32_t e, int mode, int cut, int32_t *y, uint32_t n)
{
    setup(e, e == ENGI_DUAL ? 2u : 0u);          /* (DUALVCO: DUAL LEAD, a sustained bright sound; 808 KICK until 1.3.5) */
    trk[0].p[P_FMODE] = (int16_t)mode; trk[0].p[P_FCUT] = (int16_t)cut; trk[0].p[P_FRES] = 0;
    trk[0].p[P_ED_FLT] = 0; trk[0].p[P_ATK] = 0; trk[0].p[P_SUS] = 127;
    trk_note_on(&trk[0], 48, 110);
    static int32_t b[CTL];
    uint32_t warm = FS / 10u / CTL;
    for (uint32_t i = 0; i < warm + n / CTL; i++) {
        track_render_audio(&trk[0], b, CTL, 1);
        if (i >= warm) for (uint32_t k = 0; k < CTL; k++) y[(i - warm) * CTL + k] = b[k];
    }
}

int main(void)
{
    static const char *const NAME[8] = {"SAW", "ENS", "WAV", "NOISE", "FMTONE", "DUALVCO", "ANALOG4", "PULS"};
    static const uint8_t ENG7[8] = {ENGI_SAW, ENGI_SAW + 1u, ENGI_WAV, 11u, ENGI_FMTONE, ENGI_DUAL, ENGI_ANA4, ENGI_PULS};
    int ok = 1;
    for (uint32_t k = 0; k < 8u; k++) {
        const engine_t *e = ENGINES[ENG7[k]];
        ok &= e->npresets > 0 && !strcmp(e->name, NAME[k]);
        for (uint32_t pi = 0; pi < e->npresets; pi++) {
            setup(ENG7[k], pi);
            trk_note_on(&trk[0], 48, 127);
            int32_t pk = render_peak(FS / 2u, 0);
            if (!(pk > 500 && pk < 32000)) { printf("m2k: %s %s peak %d\n", NAME[k], e->presets[pi].name, (int)pk); ok = 0; }
        }
    }
    check("every preset of the eight sounds: one note at velocity 127, short of the voice's clip (32000)", ok);

    ok = 1;
    for (uint32_t k = 0; k < 8u; k++) {
        const engine_t *e = ENGINES[ENG7[k]];
        for (uint32_t pi = 0; pi < e->npresets; pi++) {
            setup(ENG7[k], pi);
            trk[0].p[P_REL] = 20;
            trk_note_on(&trk[0], 60, 110);
            int32_t fresh = render_peak(FS / 20u, 0);   /* (NOISE: a noise's peak varies, its level does not) */
            render_peak(FS / 4u, 0);
            trk_note_off(&trk[0], 60);
            uint32_t idle = 0;
            for (uint32_t s = 0; s < 10u * FS / CTL && idle < FS / 2u / CTL; s++) {
                render_peak(CTL, 1);
                idle = held() ? 0 : idle + 1u;
            }
            trk_note_on(&trk[0], 60, 110);
            int32_t again = render_peak(FS / 20u, 0);
            if (!idle || again > fresh + fresh / (ENG7[k] == 11u ? 2 : 4)) {
                printf("m2k: %s %s first note %d, after silence %d (idle %u)\n", NAME[k], e->presets[pi].name,
                       (int)fresh, (int)again, idle);
                ok = 0;
            }
        }
    }
    check("the first note after silence (idle blocks over a dirty mixer buffer) starts as from a fresh part", ok);

    {
        enum { NS = 4096u * 6u };
        static int32_t off[NS], y[NS];
        saw_render(0, 127, 0, 0, off, NS);
        double lo0 = band(off, NS, 300), hi0 = band(off, NS, 4800);
        saw_render(1, 40, 0, 0, y, NS);
        double lp = 10 * log10((band(y, NS, 4800) / band(y, NS, 300)) / (hi0 / lo0));
        saw_render(3, 100, 0, 0, y, NS);
        double hp = 10 * log10((band(y, NS, 300) / band(y, NS, 4800)) / (lo0 / hi0));
        printf("m2k: FILTER on SAW: LP CUT 40 takes 4.8 kHz %.1f dB against 300 Hz, HP CUT 100 300 Hz %.1f dB\n", lp, hp);
        check("FILTER LP at CUT 40: 4.8 kHz 20 dB or more under 300 Hz (against OFF); HP at CUT 100 the reverse", lp <= -20 && hp <= -20);
        saw_render(0, 40, 90, 0, y, NS);
        check("FILTER OFF: the sound as without it (CUT and RES ignored), sample for sample", !memcmp(off, y, sizeof y));
        saw_render(1, 30, 0, 0, y, NS); double still = band(y, NS, 4800);
        saw_render(1, 30, 0, 127, y, NS); double moved = band(y, NS, 4800);
        check("FILTER on: ENV -> FLT opens the cutoff (4.8 kHz up 10 dB or more)", 10 * log10(moved / still) >= 10);
        ok = 1;
        for (uint32_t k = 4; k < 7u; k++) {
            mt_render(ENG7[k], 0, 127, off, NS); double h0 = band(off, NS, 4800);
            mt_render(ENG7[k], 1, 30, y, NS); double h1 = band(y, NS, 4800);
            printf("m2k: FILTER on %s: LP CUT 30 takes 4.8 kHz %.1f dB against OFF\n", NAME[k], 10 * log10(h1 / h0));
            ok &= h0 > 0 && 10 * log10(h1 / h0) <= -12;
        }
        check("FILTER on FMTONE, DUALVCO, ANALOG4: LP at CUT 30 takes 4.8 kHz 12 dB or more down (against OFF)", ok);
    }
    {
        setup(ENGI_SAW, 0);
        uint8_t keep = settings_midi_knob_mode;
        settings_midi_knob_mode = MK_ABSOLUTE;
        midi_parameter(&trk[0], 74, 32); midi_parameter(&trk[0], 71, 127);
        ok = trk[0].p[P_FCUT] == 32 && trk[0].p[P_FRES] == 127;
        static const uint8_t MORE[] = {ENGI_SAW + 1u, ENGI_WAV, ENGI_FMTONE, ENGI_DUAL, ENGI_ANA4};
        for (uint32_t k = 0; k < NELEM(MORE); k++) {
            setup(MORE[k], 0); midi_parameter(&trk[0], 74, (uint8_t)(10 + k)); midi_parameter(&trk[0], 71, (uint8_t)(90 + k));
            ok &= trk[0].p[P_FCUT] == (int16_t)(10 + k) && trk[0].p[P_FRES] == (int16_t)(90 + k);
        }
        check("CC 74 / 71: the FILTER's CUT / RES on its six (Mini-Tone's three too)", ok);
        setup(11u, 0); int16_t fc = trk[0].p[P_FCUT], fr = trk[0].p[P_FRES];
        midi_parameter(&trk[0], 74, 20); midi_parameter(&trk[0], 71, 100);
        check("CC 74 / 71 on NOISE: its own FREQ / RES (no FILTER page)",
              trk[0].p[P_E2] == 20 && trk[0].p[P_E3] == 100 && trk[0].p[P_FCUT] == fc && trk[0].p[P_FRES] == fr);
        setup(ENGI_SAW, 0);
        settings_midi_knob_mode = MK_REL1;
        trk[0].p[P_ATK] = 40; trk[0].p[P_FCUT] = 100; trk[0].p[P_LEVEL] = 50;
        midi_parameter(&trk[0], 73, 65); midi_parameter(&trk[0], 73, 65); midi_parameter(&trk[0], 73, 63);
        midi_parameter(&trk[0], 74, 63);
        midi_parameter(&trk[0], 7, 65);
        ok = trk[0].p[P_ATK] == 41 && trk[0].p[P_FCUT] == 99 && trk[0].p[P_LEVEL] == TP[P_LEVEL].min + 65 * (TP[P_LEVEL].max - TP[P_LEVEL].min) / 127;
        trk[0].p[P_ATK] = 0; midi_parameter(&trk[0], 73, 60);
        ok &= trk[0].p[P_ATK] == 0;
        printf("m2k: REL 1: ATK %d CUT %d LEVEL %d\n", trk[0].p[P_ATK], trk[0].p[P_FCUT], trk[0].p[P_LEVEL]);
        check("REL 1: CC 73 and 74 step from where they are (63 down, 65 up, clamped); CC 7 LEVEL stays absolute", ok);
        midi_rel_src_init = 0;
        ok = midi_rel_source(0, 1, 65) == 65 && midi_rel_source(0, 1, 20) == 20;          /* the wheel: absolute */
        ok &= midi_rel_source(0, 11, 63) == 126 && midi_rel_source(0, 2, 65) == 1 && midi_rel_source(0, 2, 65) == 2;
        check("REL 1: CC 1 (the mod wheel) stays absolute; CC 2 and 11 step a kept position (expression from 127)", ok);
        settings_midi_knob_mode = keep;
    }

    printf(bad ? "m2k: FAILED\n" : "m2k: SAW ENS WAV NOISE FMTONE DUALVCO ANALOG4 PULS: presets, the first note after silence, the FILTER, CC 74 / 71 and REL 1: OK\n");
    return bad != 0;
}
