# Melodee Custom 1.3.4

A vocal engine, a drive on every voice, and darker tape colours.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## VOX

MONO2K's **VOX-6**, after the Elektron Monomachine's VO-6: a buzz and noise through four formant resonators, the first
two setting the vowel.

| Control | What it does |
|---|---|
| **VOC1** | The first formant, 199–702 Hz: higher is a more open vowel ("ah") |
| **VOC2** | The second formant, 457–2757 Hz: higher is a brighter vowel ("ee") |
| **VOIC** | Voiced (0) to whispered (127) |
| **TUNE** | ±2 semitones |

Every vowel is about as loud (VO-6's own level control), a new note fades in quickly, and a legato note glides from the
last vowel. The track's ENV and LFO **FLT** amounts move VOC2 (a wah), **SHP** moves VOC1. Up to four voices. Presets:
AAH, EH, EE, OH, OO, WAH LEAD, WHISPER. VO-6's consonant controls are not in MONO2K yet, so they are left out.

## ENV > DRIVE

The drive moves from the FX slots to each voice, on a new **DRIVE** page in the ENV section: after the engine and its
FILTER, before the amp envelope (MONO2K's order).

| Control | What it does |
|---|---|
| **DRIVE** | How hard (0: off) |
| **SHAPE** | SOFT, HARD, FOLD or FUZZ (the former DRIVE effect's shapes) |
| **VEL** | How much velocity sets the drive: 0 every note alike; turned up, soft notes drive less |

What changes against a DRIVE effect: each note of a chord is driven on its own (no intermodulation grit), a fading note
keeps its grit, and the FILTER before it is its tone (there is no TONE control). Loading a factory sound sets DRIVE to 0.

DRIVE is no longer offered in the FX picker. A project that has one in a slot keeps it and plays as before. CRUSH and
SPIT stay FX slot effects.

## Colours

- **TAPE A** and **TAPE B**: their backgrounds and surfaces are 30 % darker (the panel shows them brighter than a
  screenshot does).
- **LOOM A** and **LOOM B**: TAPE A's and TAPE B's track colours on black.

## Upgrading

Install as usual; projects, presets, settings and backups are kept. Tested on the computer build of the firmware.

## Credits

VOX-6 is MONO2K's (Sequencist, GPL-3.0); its tables are MONO2K's, unchanged. The colours are from
[LOOM-8](https://github.com/charlesvestal/fm1-loom) by Charles Vestal.
