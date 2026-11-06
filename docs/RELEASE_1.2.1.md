# Melodee Custom 1.2.1

A new noise engine from [MONO2K](https://github.com/InigoHernandez/mono2k), and a fix for ENS.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## NOIS

**NOIS** is MONO2K's GND-NOIS: noise that sounds the same on every key, shaped by the track's filter and envelope.
It comes after WAV in the engine list and plays six voices, each its own noise.

| Control | What it does |
|---|---|
| **ST** | Mixes in a second noise, coloured the same way (0: none; 127: as loud as the first). |
| **RED** | The colour: white at 0, pink at 64, red (brown) at 127. Redder noise is darker and quieter. |
| **STON** | ON sends the second noise across the stereo instead of into the middle: the left hears the first noise plus the second, the right the first less it. It needs the track's SPREAD above 0; without SPREAD the second noise drops out. |

The Stage knobs are ST, RED, STON and release. The track's filter modulation moves RED and its shape modulation
moves ST. Presets: WHITE (a gated snare noise), NOISHAT, SHAKER, WINDPAD (pink) and WHOOSH (red, stereo).

In MONO2K the noise goes through its own key-following filter; here the track's filter does that job (MONO2K's TUNE,
which moved that filter, is left out). NOIS runs 6 dB under MONO2K's level so the darker colours don't clip before
the filter; raise LEVEL if you need it louder.

NOIS shares a weight with WAV: **WEIGHTS** now shows **WAV+** (WAV and NOIS) in WAV's place.

## Fix: ENS glitch on the first note

ENS clicked or clipped on the first note after its sound had faded out (legato and added notes were fine). Its
chorus kept running while the track was silent and took in leftover audio from the mixer, which then played back on
the next note. It now takes in silence, and the first note after silence starts with an empty chorus.

## Upgrading

Install as usual; projects, presets, settings and backups are kept. As in 1.2.0, the engines were tested on the
computer build of the firmware, not yet on an FM-1.

## Credits

GND-NOIS and its tables are from MONO2K, Copyright (C) 2026 Sequencist (InigoHernandez); see
[LICENSING.md](../LICENSING.md).
