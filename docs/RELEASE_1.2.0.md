# Melodee Custom 1.2.0

Six polyphonic synth engines ported from [MONO2K](https://github.com/InigoHernandez/mono2k) replace PROPHET,
FM6 and PHASE, and the screen has one black-and-white palette. (The number 1.1.0 belongs to an old Melodee
release in this repository's history, so this release follows 1.0.3 as 1.2.0.)

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## New engines

The engines are MONO2K's SWAV machines: their sound code and tables are carried over unchanged, and each
one plays chords on a Melodee track. They come first in the engine list.

| Engine | Voices | Page 1 | Page 2 | What it is |
|---|---|---|---|---|
| **SAW** | 6 | UNISON: UNIL UNIW UNIX DRFT | SUBS: SUBX SUB1 SUB2 TUNE | Supersaw with drift and two sub oscillators |
| **ENS** | 3 | CHORD: PCH2 PCH3 PCH4 WAVE | CHORUS: PW CHRL CHRW TUNE | Four-oscillator chord (intervals in semitones or ratios) through a stereo chorus |
| **STAT** | 6 | OP 1: 1FRQ 1FIN 1ENV 1FB | OP 2: 2FRQ 2VOL TONE TUNE | Two-operator FM with fixed ratios |
| **PAR** | 6 | OPS: 1FRQ 1ENV 2FRQ 2ENV | TONE: 3FRQ 3ENV TONE TUNE | Three modulators on one carrier |
| **DYN** | 6 | OP 1: 1FRQ 1FEN 1VOL 1VEN | OP 2: 2FRQ 2ENV 2FB TUNE | FM whose ratio and depth follow an envelope |
| **WAV** | 6 | WAVE: WAVE SCAN STIM SYNC | SHAPE: BITS DTUN MIX2 STEP | Wavetable scanning across 64 waves (OS01–OS64), with sync and bit reduction |

Each engine has 4 or 5 presets (SAW: SUPERSAW, BIG PAD, MONO SAW, DRIFT LD, SUB BASS, and so on). The track's
filter and shape modulation move one of the engine's parameters, for example UNIW and UNIX on SAW and the
wave and scan on WAV. ENS uses 3 voices because each voice runs four oscillators.

The engines use more processing per voice than LOFI and about as much as VOICE. Each counts as 2 units of the
voice budget (ENS: 3), so the BUDGET weight keeps the total in bounds. They were tested on the computer build
of the firmware, not yet on an FM-1: if a dense pattern stutters, lower BUDGET or use fewer voices.

**WEIGHTS** now lists SAW, ENS, FM+ (STAT, PAR and DYN together) and WAV on its first page, DRUM, LOFI, VOICE
and TRIO on the second, and WHEEL, PHYS, BUDGET and SAFETY on the third. Saved weights for DRUM, LOFI, VOICE, TRIO,
WHEEL and PHYS carry over; the new engines start at AUTO.

## Removed engines

PROPHET, FM6 and PHASE are gone, which made room for the new engines. A project, preset or favorite that uses
one of them still loads, but that track plays silent until you choose another engine. The new default project
uses SAW, STAT, LOFI and DRUM.

## One palette

COLOR offers only **MONO**: white on black, with all four tracks drawn in white. Your COLOR setting changes to
MONO on upgrade.

User preset slots are unchanged (the new engines fit without halving them).

## Upgrading

- **From Melodee Custom 1.0.x:** install as usual. Projects, presets, settings and backups are kept; the
  parts that used PROPHET, FM6 or PHASE play silent, as above.
- **From Melodee 1.6.x:** see Upgrading in the [1.0.0 release notes](RELEASE_1.0.0.md).

The recovery tools are attached as in 1.0.3 (the Apple Silicon Mac tool may be missing).

## Credits

The SWAV engines and their tables are from MONO2K, Copyright (C) 2026 Sequencist (InigoHernandez); see
[LICENSING.md](../LICENSING.md).
