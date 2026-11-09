/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* GLO > WEIGHTS with the shipping engines (MELODEE_FELUCCA_ENGINES with MONO2K's and Mini-Tone's): SAW ENS FMT WAV+ /
 * DRUM LOFI DUAL A4 / WHEEL PHYS BUDGET GUARD; each weight reaches its engine's voices only (WAV+: WAV NOIS); the
 * version 4 record (ten nibbles), a version 3 record's all but FM+ VOICE TRIO, a version 2 record's DRUM LOFI WHEEL
 * PHYS and a version 1 record's DRUM. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static const uint8_t kinds[VOICE_KINDS] = {ENGI_SAW, ENGI_SAW + 1u, ENGI_FMTONE, ENGI_WAV, ENGI_DRUM, 3u, ENGI_DUAL, ENGI_ANA4, 7u, 9u};
static const char *const names[VOICE_KINDS] = {"SAW", "ENS", "FMT", "WAV+", "DRUM", "LOFI", "DUAL", "A4", "WHEEL", "PHYS"};
static const char *value(const char *title, uint32_t k, int16_t **vp, const char **label)
{
    static char v[12]; const char *u;
    go_title(title);
    const param_desc_t *d = page_desc(cur_page(), k, vp);
    if (!d || !*vp) { *label = ""; return ""; }
    *label = d->label;
    param_format(d, **vp, v, &u);
    return v;
}
int main(void)
{
    int bad = 0, ok = 1;
    _Static_assert(VOICE_KINDS == 10u, "the shipping engines' weights");
    ui_power_on(); memset(settings_vpoints, 0, sizeof settings_vpoints);
    for (uint32_t k = 0; k < VOICE_KINDS; k++) {
        set_engine_of(&trk[0], kinds[k]); trk[0].engine = trk[0].eng_req;
        uint32_t autov = voice_units(&trk[0]);
        settings_vpoints[k] = 7;
        ok &= voice_units(&trk[0]) == 7u;
        settings_vpoints[k] = 0; settings_vpoints[(k + 1u) % VOICE_KINDS] = 9;
        ok &= voice_units(&trk[0]) == autov;                  /* another engine's weight never applies */
        settings_vpoints[(k + 1u) % VOICE_KINDS] = 0;
    }
    set_engine_of(&trk[0], ENGI_SAW + 1u); trk[0].engine = trk[0].eng_req; ok &= voice_units(&trk[0]) == 3u;   /* ENS AUTO */
    set_engine_of(&trk[0], ENGI_NOIS); trk[0].engine = trk[0].eng_req;   /* WAV+: WAV and NOIS */
    settings_vpoints[3] = 6; ok &= voice_units(&trk[0]) == 6u; settings_vpoints[3] = 0;
    bad += check("each of the ten weights reaches its own engine's voices only (WAV+: WAV NOIS); AUTO keeps the built-in rule", ok);

    int16_t *vp; const char *label;
    static const struct { const char *page; uint8_t col, kind; } at[VOICE_KINDS] = {
        {"WEIGHTS", 0, 0}, {"WEIGHTS", 1, 1}, {"WEIGHTS", 2, 2}, {"WEIGHTS", 3, 3},
        {"WEIGHTS 2", 0, 4}, {"WEIGHTS 2", 1, 5}, {"WEIGHTS 2", 2, 6}, {"WEIGHTS 2", 3, 7}, {"WEIGHTS 3", 0, 8}, {"WEIGHTS 3", 1, 9}};
    ok = 1;
    for (uint32_t k = 0; k < VOICE_KINDS; k++)
        ok &= str_eq(value(at[k].page, at[k].col, &vp, &label), "AUTO") && str_eq(label, names[at[k].kind]) &&
              vp == &settings_vpoints[at[k].kind];
    value("WEIGHTS 3", 2, &vp, &label); ok &= str_eq(label, "BUDGET") && vp == &settings_budget;
    ok &= str_eq(value("WEIGHTS 3", 3, &vp, &label), "OFF") && str_eq(label, "GUARD") && vp == &settings_safety;
    value("WEIGHTS 2", 2, &vp, &label); turn(EN_K3, 2); ok &= settings_vpoints[6] == 2;   /* DUAL */
    for (uint32_t i = 0; i < NPAGES; i++)
        for (uint32_t c = 0; c < 4; c++) if (str_eq(PAGES[i].title, "WEIGHTS") || str_eq(PAGES[i].title, "WEIGHTS 2"))
            ok &= PAGES[i].id[c] == 0xFF || global_reserved(PAGES[i].id[c]) || PAGES[i].id[c] == G_BUDGET;
    bad += check("WEIGHTS: SAW ENS FMT WAV+; 2: DRUM LOFI DUAL A4; 3: WHEEL PHYS BUDGET GUARD; on reserved ids", ok);

    persist_t p; memset(&p, 0, sizeof p);
    static const int16_t custom[VOICE_KINDS] = {3, 0, 1, 15, 2, 9, 4, 6, 12, 7};
    memcpy(settings_vpoints, custom, sizeof custom); settings_safety = 0;
    settings_export(&p);
    const uint8_t *w = p.favorites.factory[8] + 8;
    ok = w[0] == 'W' && w[1] == 4u && w[7] == 1u && settings_legacy_valid(&p);
    memset(settings_vpoints, 0, sizeof settings_vpoints); settings_safety = 1;
    ok &= settings_import(&p, (int)sizeof p) && !memcmp(settings_vpoints, custom, sizeof custom) && !settings_safety;
    bad += check("version 4: ten weights as nibbles and GUARD survive export / import", ok);

    persist_t q3 = p; q3.favorites.factory[8][8 + 1] = 3u;   /* the same nibbles as version 3 (1.2.5's slots) */
    ok = settings_import(&q3, (int)sizeof q3);
    for (uint32_t k = 0; k < VOICE_KINDS; k++) ok &= settings_vpoints[k] == (k == 2u || k == 6u || k == 7u ? 0 : custom[k]);
    bad += check("a version 3 record keeps all but FM+ VOICE TRIO (retired); FMT DUAL A4 start AUTO", ok);

    persist_t q = p; uint8_t *v2 = q.favorites.factory[8] + 8;   /* version 2: P5 FM6 DRUM PHASE LOFI VOICE TRIO WHEEL PHYS */
    static const uint8_t old[9] = {3, 4, 5, 6, 7, 8, 9, 10, 11};
    memset(v2, 0, 8); v2[0] = 'W'; v2[1] = 2u; v2[7] = 1u;
    for (uint32_t k = 0; k < 9u; k++) v2[2u + k / 2u] |= (uint8_t)(old[k] << (k & 1u ? 4u : 0u));
    ok = settings_import(&q, (int)sizeof q) && !settings_vpoints[0] && !settings_vpoints[1] && !settings_vpoints[2] &&
         !settings_vpoints[3] && settings_vpoints[4] == 5 && settings_vpoints[5] == 7 && !settings_vpoints[6] &&
         !settings_vpoints[7] && settings_vpoints[8] == 10 && settings_vpoints[9] == 11 && !settings_safety;
    bad += check("a version 2 record keeps DRUM LOFI WHEEL PHYS; the P5 FM6 PHASE VOICE TRIO points go, the new engines AUTO", ok);

    persist_t r = p; uint8_t *v1 = r.favorites.factory[8] + 8;
    memset(v1, 0, 8); v1[0] = 'W'; v1[1] = 1u; v1[2] = 5; v1[3] = 4; v1[4] = 3; v1[5] = 2; v1[6] = 1; v1[7] = 0;
    ok = settings_import(&r, (int)sizeof r) && settings_vpoints[4] == 1 && !settings_vpoints[0] && !settings_vpoints[2] &&
         !settings_vpoints[9] && settings_safety == 1;
    bad += check("a version 1 record (P5 CZ-1 FM6 SID DRUM) keeps DRUM only", ok);
    printf(bad ? "voice weights (shipping): %d FAILED\n" : "GLO > WEIGHTS, shipping engines: ten weights, three pages, v4 record, v3 v2 and v1 migration: OK\n", bad);
    return bad != 0;
}
