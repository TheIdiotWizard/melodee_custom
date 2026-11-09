# Melodee Custom 1.2.6

Three engines from Mini-Tone in place of five old ones, with the FILTER page on all seven synth engines.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## New: FMTONE, DUALVCO and ANALOG4

Ported from [Mini-Tone](https://github.com/eldontyrellcorp/Mini-Tone) by Stephane Schmitz (GPL-3.0). They sound the
same as in Mini-Tone, except for the filter (see below).

| Engine | What it is | Voices | Stage knobs (CC 20–23) | Presets |
|---|---|---|---|---|
| **FMTONE** | Four-operator FM (C, A, B1, B2) with 8 algorithms | 6 | LEV A, LEV B, FB, HARM | TINE EP, BELL, FM BASS, BRASS, GLASS PAD, PLUCK LEAD, RESO PLUCK, FIFTH RING |
| **DUALVCO** | Two oscillators, each with its own decay, mixed, ring-modulated or FM'd, with a pitch sweep and overdrive | 4 | DET, CNFG, BEND, OVER | 808 KICK, FM BELL, RING METAL, SAW+SINE BASS, DUAL LEAD, FM BASS, F+R GLASS, TOM, RESO LEAD |
| **ANALOG4** | Two oscillators plus sub and noise, two filters (series or parallel), a filter envelope and a second envelope | 4 | WAV1, PW1, WAV2, FIN2 | SAW BASS, PWM LEAD, SYNC LEAD, ACID, WARM PAD, KICK, ZAP TOM, NOISE HAT |

- **FMTONE** has four extra EDIT pages: **A ENV** and **B ENV** (ATK, DEC, END, LEV for operators A and B),
  **DEL TRK** (each envelope's delay and key tracking) and **FINE** (fine tuning in cents).
- **ANALOG4** has five: **MIX** (LEV1, LEV2, NOIS, SYNC), **F1 ENV** (filter envelope), **F1 MOD** (F1EN, the
  envelope's amount; DRV, the drive), **FLT 2** (F2MD, CUT2, RES2, KEY) and **ENV 2** (E2AT, E2DC, E2AM, E2DS).
- Track 2 now powers on with **FMTONE FM BASS** (it was STAT FM BASS).
- They answer the same MIDI CCs as the other engines: CC 20–23 (Stage knobs), 24 (sound), 72 / 73 / 75 (release,
  attack, decay), 5 (glide), 10 (pan), 7 (level), and CC 74 / 71 for the FILTER's CUT / RES.

## The FILTER on all seven

The FILTER page (MODE, CUT, RES, ENV; TRIO's filter) is now on SAW, ENS, WAV, NOIS, FMTONE, DUALVCO and ANALOG4.
DRUM, LOFI, WHEEL and PHYS don't have it.

- **FMTONE** and **DUALVCO**: Mini-Tone's resonant voice filter is replaced by the FILTER. MODE is OFF on most
  presets, which sounds the same as Mini-Tone. RESO PLUCK, FIFTH RING and RESO LEAD start with it on. The old
  filter's key tracking is gone.
- **ANALOG4**: filter 1 is now the FILTER page (MODE, CUT, RES). Its envelope (F1 ENV, F1EN) and KEY still move the
  cutoff. Filter 2 (FLT 2) runs on the same kind of filter. Every ANALOG4 preset starts with the FILTER on.
- Resonance is mapped approximately: the FILTER never self-oscillates, so Mini-Tone's screaming settings come out
  tamer.
- With MODE on, the track's ENV / LFO → FLT moves the cutoff. With MODE OFF on FMTONE, FLT moves its modulation
  levels instead, as SHP does.

## Removed: STAT, PAR, DYN, TRIO and VOICE

These five engines are gone. FMTONE covers what STAT, PAR and DYN did, and the FILTER page replaces TRIO's filter.
A project, user sound or favorite that uses one of them still loads, but that track plays silent until you pick
another engine.

## GLO > WEIGHTS

The weights are now **SAW ENS FMT WAV+** / **DRUM LOFI DUAL A4** / **WHEEL PHYS BUDGET GUARD**. FMT, DUAL and A4
take the places of FM+, VOICE and TRIO. Those three start at AUTO when you upgrade; the others are kept.

## Smaller firmware

Code and tables that only the removed engines used are no longer in the firmware (about 7.5 KB of program and
8.6 KB of resources).

## Upgrading

Install as usual. Projects, presets, settings and backups are kept. As before, this was tested on the computer build
of the firmware, not yet on an FM-1.
