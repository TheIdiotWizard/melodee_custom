/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* DRM's factory kits (eng_drum.c drm_kit_put): for KICK SNARE CLAP HATCL HATOP TOM RIM BELL, a model and its seven
 * stored knobs in drm_knob's order (the model's own, the dropped one left out), then the lane's LEVEL (P_LN), set so
 * every lane sits where the 808 kit's did (tests/drm_test.c checks them). The presets' order is the kits' (stored:
 * append only). SUB and HOUSE take the 808's and 909's places, MD EFM the FM kit's, LO-FI the CR78's and SID's
 * (drm_upgrade). */
enum { DRMK_SUB, DRMK_HOUSE, DRMK_EFM, DRMK_TECHNO, DRMK_FORGE, DRMK_RUST, DRMK_HAND, DRMK_LOFI, DRMK_MICRO, DRMK_GLITCH };
#define L(m, a, b, c, d, e, f, g, lv) {DM_##m, {a, b, c, d, e, f, g}, lv}
static const drm_lane_def_t DRM_KITS[][DV_NLANE] = {
    [DRMK_SUB] = {          /* deep and round, after the 808: a long low kick, a bright snare, a cowbell */
        L(BDTRX, -7, 100, 30, 40, 8, 6, 10, 125),  L(SDTRX, 0, 20, 110, 20, 0, 64, 70, 127),
        L(CPTRX, 10, 100, 56, 7, 43, 56, 0, 127),  L(CHTRX, 0, 8, 0, 0, 127, 0, 0, 120),
        L(OHTRX, 0, 61, 57, 8, 127, 0, 0, 100),    L(TMTRX, 0, 60, 48, 102, 64, 0, 0, 115),
        L(RSTRX, -4, 10, 0, 0, 0, 0, 0, 100),      L(CBTRX, 0, 48, 127, 127, 0, 0, 105, 127),
    },
    [DRMK_HOUSE] = {        /* punchy, after the 909: a short clipped kick, a snappy snare, a ride */
        L(BDTRX, -3, 60, 50, 30, 40, 20, 40, 127), L(SDTRX, 2, 12, 127, 40, 20, 64, 92, 127),
        L(CPTRX, 10, 110, 56, 20, 43, 40, 0, 117), L(CHTRX, 0, 12, 0, 20, 127, 10, 0, 119),
        L(OHTRX, 0, 70, 57, 20, 127, 10, 0, 100),  L(TMTRX, 3, 50, 60, 90, 64, 0, 0, 120),
        L(RSTRX, 0, 10, 20, 0, 0, 0, 0, 96),      L(CYTRX, 0, 60, 127, 93, 107, 127, 0, 119),
    },
    [DRMK_EFM] = {          /* the FM machines as they come: glassy, metallic */
        L(BDEFM, -13, 68, 10, 29, 5, 4, 24, 127),  L(SDEFM, -9, 60, 14, 79, 0, 5, 29, 117),
        L(CPEFM, 24, 45, 91, 30, 75, 36, 61, 122), L(HHEFM, -2, 30, 0, 0, 3, 12, 26, 127),
        L(HHEFM, -2, 80, 0, 0, 3, 12, 26, 125),    L(TMEFM, 12, 85, 62, 36, 64, 4, 45, 119),
        L(RSEFM, 1, 10, 8, 15, 38, 21, 59, 112),   L(CBEFM, -2, 61, 92, 56, 17, 112, 42, 127),
    },
    [DRMK_TECHNO] = {       /* Plaits' kick and snare, metal hats, a roomy clap, a skin tom */
        L(BOOM, 0, 70, 50, 60, 0, 0, 0, 124),      L(SNAP, 0, 50, 64, 70, 0, 0, 0, 127),
        L(CPTRX, 10, 100, 56, 30, 43, 70, 40, 112), L(METAL, 0, 25, 90, 30, 0, 0, 0, 127),
        L(METAL, 0, 75, 80, 40, 0, 0, 0, 115),     L(SKIN, 0, 60, 64, 20, 40, 30, 24, 125),
        L(CLTRX, 0, 40, 0, 0, 64, 40, 0, 86),     L(CYTRX, 0, 70, 127, 93, 107, 127, 0, 118),
    },
    [DRMK_FORGE] = {        /* industrial: a dirty distorted kick, an FM crack, hiss, a bent metal head, an anvil */
        L(B2TRX, -5, 80, 40, 40, 30, 110, 100, 118), L(CRACK, 0, 60, 110, 90, 0, 0, 0, 127),
        L(CPEFM, 24, 45, 91, 30, 110, 60, 80, 118),  L(HISS, 5, 20, 100, 100, 0, 0, 0, 127),
        L(METAL, 0, 90, 60, 110, 0, 0, 0, 102),      L(SKIN, -5, 90, 90, 90, 100, 110, 80, 125),
        L(RSEFM, 1, 30, 60, 40, 90, 60, 59, 100),    L(CBEFM, -5, 100, 92, 120, 90, 112, 80, 115),
    },
    [DRMK_RUST] = {         /* power noise: an FM kick, a noisy snare, hard clap, tremolo hats, a clanging FM bell */
        L(PUNCH, 0, 70, 80, 120, 0, 0, 0, 127),     L(SDEFM, -6, 70, 90, 90, 60, 40, 60, 99),
        L(CPTRX, 10, 90, 90, 127, 80, 30, 0, 99),  L(HHEFM, 0, 30, 100, 90, 60, 40, 60, 103),
        L(CYEFM, -2, 90, 110, 80, 100, 100, 86, 97), L(TMEFM, 5, 90, 62, 36, 120, 40, 45, 113),
        L(RSTRX, -4, 30, 127, 0, 0, 0, 0, 93),     L(FM, 0, 70, 110, 9, 60, 30, 100, 116),
    },
    [DRMK_HAND] = {         /* hand percussion, no snare: a big skin, congas, claves, shakers, a bell */
        L(SKIN, -10, 80, 40, 20, 30, 40, 30, 123),  L(CGTRX, 11, 40, 30, 29, 80, 40, 107, 127),
        L(CLTRX, 0, 64, 0, 0, 64, 0, 0, 105),       L(SHTRX, 0, 40, 0, 0, 0, 64, 64, 120),
        L(SHTRX, 0, 90, 40, 0, 40, 64, 64, 113),    L(CGTRX, 0, 50, 30, 29, 105, 0, 0, 127),
        L(CLTRX, 7, 40, 64, 40, 64, 20, 0, 66),    L(CBTRX, 5, 40, 127, 127, 40, 0, 105, 123),
    },
    [DRMK_LOFI] = {         /* dusty: a dirty short kick, a clipped low snare, dark hats */
        L(B2TRX, -2, 60, 30, 10, 10, 60, 30, 127),  L(SDTRX, -3, 25, 80, 0, 20, 40, 110, 127),
        L(CPTRX, 5, 90, 56, 10, 30, 30, 0, 127),    L(CHTRX, 0, 10, 0, 0, 70, 0, 0, 116),
        L(OHTRX, 0, 55, 57, 0, 70, 0, 0, 98),      L(CGTRX, 0, 30, 30, 29, 105, 60, 107, 127),
        L(RSTRX, -4, 10, 60, 0, 0, 0, 0, 98),      L(CLTRX, 0, 50, 0, 0, 64, 0, 0, 114),
    },
    [DRMK_MICRO] = {        /* minimal: short clipped kick, clicks, tight hats, tuned FM */
        L(BDTRX, -2, 55, 20, 20, 40, 5, 90, 127),   L(RSEFM, 3, 20, 30, 40, 60, 30, 40, 127),
        L(CPEFM, 24, 20, 60, 15, 40, 36, 90, 127),  L(CHTRX, 5, 3, 0, 60, 127, 40, 0, 126),
        L(SHTRX, 0, 30, 0, 0, 0, 90, 90, 119),      L(FM, 0, 30, 40, 4, 30, 20, 0, 127),
        L(CLTRX, 12, 20, 0, 60, 64, 90, 0, 66),    L(FM, 12, 40, 50, 11, 60, 0, 40, 127),
    },
    [DRMK_GLITCH] = {       /* IDM: FM everywhere, tremolo hats, metallic noise */
        L(PUNCH, 0, 40, 50, 100, 0, 0, 0, 127),     L(CRACK, 3, 40, 100, 40, 0, 0, 0, 127),
        L(CPEFM, 24, 30, 91, 20, 100, 80, 61, 127), L(HHEFM, 4, 15, 127, 110, 60, 90, 40, 127),
        L(METAL, 7, 60, 100, 90, 0, 0, 0, 127),     L(FM, -5, 50, 100, 6, 40, 80, 90, 127),
        L(RSEFM, 8, 10, 90, 60, 70, 80, 30, 123),   L(FM, 12, 60, 90, 13, 80, 0, 60, 127),
    },
};
#undef L
/* {TUNE, DECY, ACC, DRV}: the track's knobs; the sound's envelope (held: the lanes are their own) */
static const preset_t DRUM_PRESETS[] = {
    {"SUB", {0, 64, 100, 0, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"HOUSE", {0, 64, 100, 0, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"MD EFM", {0, 64, 100, 0, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"TECHNO", {0, 64, 100, 0, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"FORGE", {0, 64, 100, 20, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"RUST", {0, 64, 100, 40, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"HAND", {0, 64, 100, 0, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"LO-FI", {0, 64, 80, 10, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"MICRO", {0, 64, 100, 0, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"GLITCH", {0, 64, 100, 0, 0, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
};
_Static_assert(sizeof DRM_KITS / sizeof DRM_KITS[0] == sizeof DRUM_PRESETS / sizeof DRUM_PRESETS[0], "a kit a preset");
