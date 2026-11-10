# Melodee Custom 1.2.8

NOISE replaces NOIS.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## NOISE in place of NOIS

MONO2K's NOIS (1.2.2) depended on MONO2K's own filters and LFOs for its character, which weren't part of the port,
so its presets sounded alike and its second EDIT page was empty. It is replaced by **NOISE**, the noise engine from
the earlier firmware, which has its own resonant filter. It sits where NOIS was in the engine list, after WAV.

| Page | Controls |
|---|---|
| **SRC** | **MODE** (ANLG hiss with clicks, DUST crackle, LFSR chip noise, META metallic tone), **COLR** (white, pink, brown), **FREQ**, **RES** (low-pass to narrow band-pass) |
| **MOVE** | **TRK** (FREQ follows the key), **DENS** (clicks; **LEN** in LFSR / META), **DRFT** (slow wander), **CRSH** (sample-and-hold and bits; **CLK** in LFSR / META) |

- **Presets:** WIND, RAIN, ARCADE, METAL.
- **Stage knobs** (CC 20–23): FREQ, RES, COLR, release. **CC 74 / 71** set FREQ / RES.
- The track's FLT moves FREQ and SHP moves COLR.
- NOISE has no FILTER page: its own filter does that job.
- In GLO > WEIGHTS it counts under **WAV+**, as NOIS did.

Tracks, user sounds and favorites saved with NOIS still load, but play silent until you pick another engine.

## Upgrading

Install as usual; projects, presets, settings and backups are kept. As before, this was tested on the computer build
of the firmware, not yet on an FM-1.
