# Melodee Custom 1.3.5

A fix for PRESETS browsing, and a new engine order.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## PRESETS browsing

In 1.3.4, VOX was given engine number 16, which the sound browser also uses to mean "a user preset". Turning PRESETS
past VOX loaded user slots instead, so the list jumped between unrelated sounds and did not come back the same way.
VOX now uses retired VOICE's number (5):

- Songs and user presets saved in 1.3.4 with VOX load as VOX.
- Old songs that used VOICE (silent since 1.2.6) now play VOX.
- VOX favourites starred in 1.3.4 need starring again.

## Engine order

PRESETS, the ENG knob and the web editor list the engines as: **FMTONE, ANALOG4, SAW, ENS, LOFI, DUALVCO, WAV, WHEEL,
VOX, PHYS, NOISE, DRUM**, then MIDI Track. Engine numbers in saved songs are unchanged.

Everything from [1.3.4](RELEASE_1.3.4.md) (VOX, ENV > DRIVE, the LOOM and TAPE colours) is included.
