/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Drum machine fork: 2026 DEADACTIVE
 * Portions Copyright (C) 2026 Kerem Kilic (Ellic Studio) (Melodee Custom DRM: the model on a lane, drm_lane_t) */
/* ---- M1-C: integer ports of Mutable Instruments' drum algorithms (Plaits, stmlib), MIT licence:
 * Copyright 2012-2016 Emilie Gillet (emilie.o.gillet@gmail.com).
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do so, subject to the following
 * conditions: The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE. */

static void hh_start(hh_t *k, int ring, int32_t note, int32_t tone, int32_t decay, int32_t chr, int32_t vel, uint32_t seed)
{
    static const int32_t RAT[6] = {Q24(1.0), Q24(1.304), Q24(1.466), Q24(1.787), Q24(1.932), Q24(2.536)};
    int32_t n = qknob(chr), d = qknob(decay), t = qknob(tone), acc = qknob(vel);
    int32_t f0 = qnote(note), f = 2 * f0, cut, i;
    k->ring = (uint8_t)ring;
    k->edec = QONE - qm(qratio(Q24(0.003), -qm(d, Q24(84.0))), K44);
    k->cdec = QONE - qm(qratio(Q24(0.0025), -qm(d, Q24(36.0))), K44);
    k->env = qm(Q24(1.5) + (QONE - d) / 2, Q24(0.3) + qm(Q24(0.7), acc));
    cut = qlim(qratio(Q24(150.0 / FS), qm(t, Q24(72.0))), 0, Q24(16000.0 / FS));
    qsvf_set(&k->col, qtan_acc(cut), ring ? QONE : qdiv(QONE, Q24(3.0) + qm(Q24(3.0), t)));
    qsvf_set(&k->hpf, qtan_acc(cut), 2 * QONE);
    k->col.s1 = k->col.s2 = k->hpf.s1 = k->hpf.s2 = 0;
    n = qm(n, n);
    k->noisy = n;
    k->nf = qlim(qm(f0, Q24(16.0) + qm(Q24(16.0), QONE - n)), 0, QONE / 2);
    k->rng = seed;
    k->nclk = qrand(&k->rng);              /* the running module's state at a hit: sources run, the noise holds a value */
    k->nsmp = qrand(&k->rng) - QONE / 2;
    if (!ring) {
        uint64_t f2 = 2ull * qnote_inc(note);                      /* 2 f0, exact: the squares' edges depend on it */
        for (i = 0; i < 6; i++) {
            uint64_t fi = (f2 * (uint32_t)RAT[i]) >> 24;
            k->inc[i] = (uint32_t)(fi < 2143188679u ? fi : 2143188679u);   /* 0.499 * 2^32 */
            k->ph[i] = (uint32_t)qrand(&k->rng) << 8;
        }
    } else {
        int32_t r = qdiv(q48(f), Q24(0.01) + q48(f));
        static const int32_t HZ[6] = {Q24(200.0 / FS), Q24(7530.0 / FS), Q24(510.0 / FS), Q24(8075.0 / FS),
                                      Q24(730.0 / FS), Q24(10500.0 / FS)};
        for (i = 0; i < 6; i++) {
            k->osc[i].f = qlim(qm(HZ[i], r), 1, QONE / 4);
            k->osc[i].ph = qrand(&k->rng);
            k->osc[i].high = k->osc[i].ph >= QONE / 2;
            k->osc[i].next = i & 1 ? k->osc[i].ph : k->osc[i].high ? QONE : 0;   /* square even, saw odd */
            k->osc[i].lp = k->osc[i].hp = 0;
        }
    }
}

/* Plaits Oscillator, pw 0.5: square (sq = 1) or saw, polyBLEP, one sample late */
static int32_t qbosc_tick(qbosc_t *o, int sq)
{
    int32_t th = o->next, t;
    o->next = 0;
    o->ph += o->f;
    if (sq) {
        if (o->high ^ (o->ph >= QONE / 2)) {
            t = qdiv(o->ph - QONE / 2, o->f);
            th += qm(t, t) / 2;
            t = QONE - t;
            o->next -= qm(t, t) / 2;
            o->high = o->ph >= QONE / 2;
        }
        if (o->ph >= QONE) {
            o->ph -= QONE;
            t = qdiv(o->ph, o->f);
            th -= qm(t, t) / 2;
            t = QONE - t;
            o->next += qm(t, t) / 2;
            o->high = 0;
        }
        o->next += o->high ? QONE : 0;
        return 2 * th - QONE;
    }
    if (o->ph >= QONE) {
        o->ph -= QONE;
        t = qdiv(o->ph, o->f);
        th -= qm(t, t) / 2;
        t = QONE - t;
        o->next += qm(t, t) / 2;
    }
    o->next += o->ph;
    return 2 * th - QONE;
}

static int32_t hh_tick(hh_t *k)
{
    int32_t x, lp, bp, i;
    if (!k->ring) {
        int32_t s = 0;
        for (i = 0; i < 6; i++) {
            k->ph[i] += k->inc[i];
            s += (int32_t)(k->ph[i] >> 31);
        }
        x = qm(Q24(0.33), s * QONE) - QONE;
    } else {
        x = 0;
        for (i = 0; i < 6; i += 2)
            x += qm(qbosc_tick(&k->osc[i], 1), qbosc_tick(&k->osc[i + 1], 0));
    }
    qsvf_tick(&k->col, x, &lp, &bp);
    x = bp;
    k->nclk += k->nf;
    if (k->nclk >= QONE) {
        k->nclk -= QONE;
        k->nsmp = qrand(&k->rng) - QONE / 2;
    }
    x += qm(k->noisy, k->nsmp - x);
    k->env = qdecay(k->env, k->env > Q24(0.5) || !k->ring ? k->edec : k->cdec);
    if (!k->ring) {                                           /* SwingVCA */
        x = qm(x, x > 0 ? 4 * QONE : Q24(0.1));
        x = qm(qsat(x) + Q24(0.1), k->env);
    } else {
        x = qm(x, k->env);
    }
    return qsvf_tick(&k->hpf, x, &lp, &bp);
}

/* HMETL: TUNE DECAY TONE NOISE; six square oscillators, a resonant band-pass, a swing VCA (metallic, 808-like).
 * HNOIS: TUNE DECAY TONE NOISE; ring-modulated square x saw pairs, a two-stage envelope (noisy, trashy). */
#define HMETL_NOTE 60                                     /* MIDI note at TUNE 0 (262 Hz) */
#define HNOIS_NOTE 72                                     /* (523 Hz) */
static void hmetl_trigger(drm_lane_t *t, dvoice_t *v)
{
    const int16_t *p = t->k;
    hh_start(&v->ms.hh, 0, HMETL_NOTE + dm_tune(t), p[2], p[1], p[3], v->vel, (uint32_t)v->rng);
}

static void hnois_trigger(drm_lane_t *t, dvoice_t *v)
{
    const int16_t *p = t->k;
    hh_start(&v->ms.hh, 1, HNOIS_NOTE + dm_tune(t), p[2], p[1], p[3], v->vel, (uint32_t)v->rng);
}

static void hh_render(drm_lane_t *t, dvoice_t *v, int32_t *out, uint32_t n)
{
    uint32_t i;
    int32_t pk = 0;
    (void)t;
    for (i = 0; i < n; i++) {
        int32_t y = hh_tick(&v->ms.hh);
        dm_putq(v, out, i, y);
        pk = qmax(pk, qabs(y));
    }
    dm_qend(v, n, pk, FS / 50);
}

#define DM_HMETL_DEF {"METAL", 1, 0, {PD("TUNE", F_SEMI, -24, 24, 0), PD("DECAY", F_INT, 0, 127, 40), \
    PD("TONE", F_INT, 0, 127, 80), PD("NOISE", F_INT, 0, 127, 40), PD("-", F_INT, 0, 0, 0), \
    PD("-", F_INT, 0, 0, 0), PD("-", F_INT, 0, 0, 0), PD("-", F_INT, 0, 0, 0)}, hmetl_trigger, hh_render}
#define DM_HNOIS_DEF {"HISS", 1, 0, {PD("TUNE", F_SEMI, -24, 24, 0), PD("DECAY", F_INT, 0, 127, 40), \
    PD("TONE", F_INT, 0, 127, 80), PD("NOISE", F_INT, 0, 127, 40), PD("-", F_INT, 0, 0, 0), \
    PD("-", F_INT, 0, 0, 0), PD("-", F_INT, 0, 0, 0), PD("-", F_INT, 0, 0, 0)}, hnois_trigger, hh_render}
