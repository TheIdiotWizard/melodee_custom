/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* DRM (src/eng_drum.c, Hortator's models in src/drm/): every model at its defaults sounds, stays under full scale,
 * ends and renders the same twice; every kit's lanes sound near the 808 kit of 1.3.5 (tests/drm_test.c's own
 * reference levels); a kit is stored in drum_patch_t and its wire bytes exactly; the kit on one voice; a lane struck
 * again without a step; the hat choke; a recorded hit's length; DECY live; ACC; a DRUM kit of 1.3.5 converted (its
 * engine number, its tunings) and silent until it is; demos of every kit in argv[1]. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/stat.h>
static int fails;
static void check(const char *s, int ok) { printf("DRM: %-96s %s\n", s, ok ? "ok" : "FAIL"); fails += !ok; }

static void kit(uint32_t preset)                  /* part 1 on a DRM kit, nothing sounding */
{
    host_tracks_init();
    for (uint32_t p = 0; p < NPART; p++)
        memset(trk[p].v, 0, NVOICE * sizeof *trk[p].v);
    eng_state_reset();
    host_preset(&trk[0], ENGI_DRUM, preset);
}
static uint32_t voices_on(void)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++)
        n += trk[0].v[i].active;
    return n;
}
static int32_t run(int32_t *buf, uint32_t nb)     /* nb blocks of the part into buf (0: none); the peak */
{
    int32_t o[CTL], pk = 0;
    for (uint32_t i = 0; i < nb; i++) {
        track_render(&trk[0], o, CTL);
        for (uint32_t k = 0; k < CTL; k++) {
            if (buf)
                buf[i * CTL + k] = o[k];
            pk = abs(o[k]) > pk ? abs(o[k]) : pk;
        }
    }
    return pk;
}
static double rms(const int32_t *x, uint32_t a, uint32_t b)
{
    double s = 0;
    for (uint32_t i = a; i < b; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / (double)(b - a));
}
static int32_t largest_step(const int32_t *x, uint32_t n)
{
    int32_t pk = 0;
    for (uint32_t i = 1; i < n; i++)
        pk = abs(x[i] - x[i - 1]) > pk ? abs(x[i] - x[i - 1]) : pk;
    return pk;
}
static void wav(const char *path, const int32_t *x, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t bytes = n * 2u, v;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f); v = 36u + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 1u | 1u << 16; fwrite(&v, 4, 1, f); v = FS; fwrite(&v, 4, 1, f);
    v = FS * 2u; fwrite(&v, 4, 1, f); v = 2u | 16u << 16; fwrite(&v, 4, 1, f); fwrite("data", 1, 4, f);
    fwrite(&bytes, 4, 1, f);
    for (uint32_t i = 0; i < n; i++) {
        int16_t s = (int16_t)clamp(x[i] / 2, -32767, 32767);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

#define NB(ms) ((uint32_t)((ms) * FS / 1000u / CTL))
static int32_t buf[FS * 8];

int main(int argc, char **argv)
{
    static const uint8_t NOTE[DV_NLANE] = {36, 38, 39, 42, 46, 45, 37, 56};
    /* the 808 kit of 1.3.5: each lane's peak and its first 250 ms' RMS at velocity 100 (measured on 1.3.5) */
    static const int32_t REF[DV_NLANE][2] = {{47590, 15754}, {45148, 5250}, {41040, 4326}, {47226, 2632},
                                            {38442, 3761}, {41776, 8043}, {36768, 1431}, {35434, 6156}};
    uint32_t m, l, k;
    check("engine 10 is DRM, ten kits, 29 models with a name and a TUNE and DECAY first",
          !strcmp(ENGINES[ENGI_DRUM]->name, "DRM") && ENGINES[ENGI_DRUM]->npresets == 10u && DM_N == 29 &&
          !strcmp(DMODELS[DM_BDTRX].edit[0].label, "TUNE") && !strcmp(DMODELS[DM_FM].edit[1].label, "DECAY"));
    {   /* every model alone, on the KICK lane at its defaults */
        int sounds = 1, under = 1, ends = 1, same = 1, again = 1;
        for (m = 0; m < DM_N; m++) {
            int32_t pk, pk2;
            uint32_t nb;
            kit(0);
            drm_set_model(drum_patch_of(0), 0, m);
            trk[0].p[P_LN0] = 127;
            trk_note_on(&trk[0], 36, 127);
            pk = run(buf, NB(500));
            static int32_t first[NB(500) * CTL];
            memcpy(first, buf, sizeof first);
            for (nb = 0; nb < NB(7000) && voices_on(); nb++)
                run(0, 1);
            kit(0);
            drm_set_model(drum_patch_of(0), 0, m);
            trk[0].p[P_LN0] = 127;
            trk_note_on(&trk[0], 36, 127);
            pk2 = run(buf, NB(500));
            sounds &= pk > 8000;
            under &= pk < 65000;
            ends &= nb < NB(7000);
            same &= pk2 == pk && !memcmp(first, buf, sizeof first);
            if (pk <= 8000 || pk >= 65000 || nb >= NB(7000))
                printf("  %s: peak %d, %u blocks to its end\n", N_DRM_MODEL[m], pk, nb);
            again &= largest_step(buf, NB(500) * CTL) < 60000;
        }
        check("every model sounds at full velocity", sounds);
        check("every model stays under full scale (the output knee)", under);
        check("every model ends within 7 s", ends);
        check("every model renders the same twice (seeded per hit)", same);
    }
    {   /* every kit's lanes near the 808 kit's (within 9 dB of its peak, its RMS within 12 dB) */
        int near = 1;
        for (uint32_t p = 0; p < NELEM(DRUM_PRESETS); p++)
            for (l = 0; l < DV_NLANE; l++) {
                int32_t pk;
                double r;
                kit(p);
                trk_note_on(&trk[0], NOTE[l], 100);
                pk = run(buf, NB(250));
                r = rms(buf, 0, NB(250) * CTL);
                int ok = pk > REF[l][0] / 3 && pk < 65000 && r > REF[l][1] / 4.0 && r < REF[l][1] * 4.0;
                if (!ok)
                    printf("  %s %s: peak %d (808 %d), RMS %.0f (808 %d)\n", DRUM_PRESETS[p].name,
                           drum_lane_name(&trk[0], l), pk, REF[l][0], r, REF[l][1]);
                near &= ok;
            }
        check("every kit's lanes sit near the 808 kit's (peak within 9 dB, RMS within 12 dB)", near);
    }
    {   /* a kit in drum_patch_t: valid as a DRUM kit always was, the wire bytes back exactly; every knob's range */
        int ok = 1, all = 1;
        for (uint32_t p = 0; p < NELEM(DRUM_PRESETS); p++) {
            uint8_t b[DRUM_WIRE_BYTES];
            drum_patch_t back;
            kit(p);
            drum_patch_pack(b, drum_patch_of(0));
            ok &= drum_patch_valid(drum_patch_of(0)) && drum_patch_unpack(&back, b) &&
                  !memcmp(&back, drum_patch_of(0), sizeof back);
            for (l = 0; l < DV_NLANE; l++)
                all &= drm_model(drum_patch_of(0), l) == DRM_KITS[p][l].m;
        }
        drum_patch_t d;
        memset(&d, 0, sizeof d);
        for (m = 0; m < DM_N; m++)
            for (l = 0; l < DV_NLANE; l++) {
                drm_set_model(&d, l, m);
                for (k = 0; k < 7u; k++) {
                    const param_desc_t *e = drm_knob_desc(m, k);
                    int32_t lo = e ? e->min : 0, hi = e ? e->max : 0;
                    drm_put(&d, l, k, lo);
                    all &= drm_get(&d, l, k) == lo;
                    drm_put(&d, l, k, hi);
                    all &= drm_get(&d, l, k) == hi;
                }
                uint8_t b[DRUM_WIRE_BYTES];
                drum_patch_t back;
                drum_patch_pack(b, &d);
                ok &= drum_patch_valid(&d) && drum_patch_unpack(&back, b) && !memcmp(&back, &d, sizeof d);
                all &= drm_model(&d, l) == m;
            }
        check("a kit is a valid DRUM record, its wire bytes read back exactly", ok);
        check("every model on every lane, every stored knob at its ends, read back", all);
    }
    {   /* the whole kit on one voice; a lane struck again without a step */
        kit(DRMK_SUB);
        for (l = 0; l < DV_NLANE; l++)
            trk_note_on(&trk[0], NOTE[l], 100);
        run(buf, NB(20));
        check("eight lanes at once on the part's one voice", voices_on() == 1u);
        kit(DRMK_SUB);
        trk_note_on(&trk[0], 36, 127);
        run(buf, NB(60));
        trk_note_on(&trk[0], 36, 127);
        run(buf + NB(60) * CTL, NB(20));
        int32_t step = abs(buf[NB(60) * CTL] - buf[NB(60) * CTL - 1]);
        check("a ringing kick struck again: no step at the restrike", step < 4000);
    }
    {   /* a closed hat ends the open one */
        double open, choked;
        kit(DRMK_SUB);
        trk_note_on(&trk[0], 46, 100);
        run(buf, NB(100));
        open = rms(buf, NB(60) * CTL, NB(100) * CTL);
        kit(DRMK_SUB);
        trk_note_on(&trk[0], 46, 100);
        run(buf, NB(20));
        trk[0].p[P_LN3] = 0;                          /* (the closed hat itself silent: only the open one heard) */
        trk_note_on(&trk[0], 42, 100);
        run(buf, NB(100));
        choked = rms(buf, NB(60) * CTL, NB(100) * CTL);
        check("a closed hat chokes the open hat", choked < open / 20.0);
    }
    {   /* a recorded hit's length ends its drum; DECY live; ACC */
        int32_t full, cut;
        kit(DRMK_SUB);
        trk_note_on(&trk[0], 36, 100);
        run(buf, NB(400));
        full = (int32_t)rms(buf, NB(300) * CTL, NB(400) * CTL);
        kit(DRMK_SUB);
        drum_event_length = (uint32_t)(FS / 10u);
        trk_note_on(&trk[0], 36, 100);
        drum_event_length = 0;
        run(buf, NB(400));
        cut = (int32_t)rms(buf, NB(300) * CTL, NB(400) * CTL);
        check("a recorded hit's length ends its drum", cut < full / 50 + 1);
        kit(DRMK_SUB);
        trk[0].p[P_E1] = 0;                           /* DECY down: the next hit is shorter (models set their
                                                       * envelopes at the strike, as Hortator's) */
        trk_note_on(&trk[0], 36, 100);
        run(buf, NB(400));
        int32_t live = (int32_t)rms(buf, NB(300) * CTL, NB(400) * CTL);
        check("DECY down shortens every lane's next hit", live < full / 2);
        kit(DRMK_SUB);
        trk[0].p[P_E2] = 0;                           /* ACC 0: every hit full */
        trk_note_on(&trk[0], 38, 20);
        int32_t soft0 = run(buf, NB(100));
        kit(DRMK_SUB);
        trk_note_on(&trk[0], 38, 20);
        int32_t soft = run(buf, NB(100));
        check("ACC 0 plays a soft hit as loud as a hard one; ACC 100 leaves it soft", soft0 > soft * 2);
    }
    {   /* a DRUM kit of 1.3.5 (P_DRMFMT 0): silent until converted; the 909 kit becomes HOUSE, its tunings kept */
        kit(DRMK_SUB);
        drum_patch_t *d = drum_patch_of(0);
        memset(d, 0, sizeof *d);
        trk[0].p[P_DRMFMT] = 0;
        trk[0].p[P_E0] = 5;                           /* (KIT 909) */
        trk[0].p[P_E3] = 80;                          /* (its DECY) */
        d->c[0][0] = 3;                               /* KICK 3 semitones up */
        d->c[1][0] = -2;                              /* SNARE 2 down */
        trk_note_on(&trk[0], 36, 100);
        int32_t silent = run(buf, NB(50));
        trk[0].v[0].active = 0;
        drm_upgrade(&trk[0]);
        check("a 1.3.5 kit is silent until converted", silent == 0);
        check("the 909 kit becomes HOUSE, the kick's +3 and the snare's -2 kept, DECY kept",
              trk[0].p[P_DRMFMT] == DRM_FMT && trk[0].preset == DRMK_HOUSE &&
              drm_model(d, 0) == DRM_KITS[DRMK_HOUSE][0].m && drm_get(d, 0, 0) == DRM_KITS[DRMK_HOUSE][0].k[0] + 3 &&
              drm_get(d, 1, 0) == DRM_KITS[DRMK_HOUSE][1].k[0] - 2 && trk[0].p[P_E1] == 80 && trk[0].p[P_E0] == 0);
        trk_note_on(&trk[0], 36, 100);
        check(".. and then it plays", run(buf, NB(50)) > 8000);
    }
    if (argc > 1) {   /* demos: a two-bar beat on every kit */
        static const uint16_t BEAT[DV_NLANE] = {0x1111, 0x1010, 0x1000, 0x5555, 0x0404 << 1, 0x0C00, 0x0040, 0x0008};
        for (uint32_t p = 0; p < NELEM(DRUM_PRESETS); p++) {
            uint32_t per = FS / 8u / CTL, nb = 0;     /* 16ths at 120 BPM, in blocks */
            kit(p);
            for (uint32_t s = 0; s < 32u; s++) {
                for (l = 0; l < DV_NLANE; l++)
                    if ((BEAT[l] >> (s % 16u)) & 1u)
                        trk_note_on(&trk[0], NOTE[l], s % 4u ? 90 : 120);
                run(buf + nb * CTL, per);
                nb += per;
            }
            run(buf + nb * CTL, NB(1000));
            nb += NB(1000);
            char path[256];
            snprintf(path, sizeof path, "%s/drm_%02u_%s.wav", argv[1], p, DRUM_PRESETS[p].name);
            for (char *c = path + strlen(argv[1]) + 1; *c; c++)
                if (*c == ' ')
                    *c = '_';
            wav(path, buf, nb * CTL);
        }
    }
    printf(fails ? "DRM TEST FAILED\n" : "DRM test passed\n");
    return !!fails;
}
