# Melodee Custom 1.3.0

A new look, after LOOM-8's tape-deck design.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## Palettes

| Palette | Look |
|---|---|
| **TAPE A** (default) | Warm brown, off-white text; tracks yellow, orange, green, teal |
| **TAPE B** | The same base; tracks blue, purple, tan, sage |
| **LOOM A / LOOM B** | Black with LOOM-8 track colours (1.2.7) |
| **MONO** | White on black (the old default) |

Tracks 5–8 reuse the four colours. A project saved with MONO selected opens in TAPE A; pick MONO again in HOME →
COLOR if you prefer it.

## Sound pages in bands

EDIT, ENV, LFO, FX slots, ARP, CHORD, GLO and the other sound pages now read top to bottom:

1. **Header:** track pill, section chip and page name; octave, ROW, HOST/BLE, battery, REC, transport and BPM.
2. **Picture:** what the page does.
3. **Tag line:** SOUND › ENGINE › pattern, with dots for the pages.
4. **Knob strip:** the four values, each with a gauge.
5. **Hints:** the keys that act on this page and what they do.

The header is new on every screen. Stage, the mixer, lists, the sound browser, patterns, song and notes keep their
layout with the new colours and header; Stage in the band layout is the next step.

## Engine pictures

Each engine's EDIT pages draw its sound from the values you are turning, using the engine's own tables:

- **WAV:** the selected wavetable wave (with its neighbours), its name and bits.
- **SAW:** the saw and sub, with the unison detune spread.
- **ENS:** the oscillator wave and the chord ladder.
- **NOISE:** the spectrum: colour tilt through the filter at FREQ / RES, and the mode.
- **FMTONE:** the operator routing for the algorithm, carriers filled, modulator levels, ratios and feedback.
- **DUALVCO:** both oscillators under their decay envelopes, and the mode.
- **ANALOG4:** the oscillators at their pulse width, tuning and sub, scaled by level.
- **LOFI:** the stepped chip wave.
- **WHEEL:** the drawbar levels.
- **PHYS:** the partials for the model.
- **DRUM:** the lanes, lighting as they play.
- **MIDI:** the channel.

## Upgrading

Install as usual; projects, presets, settings and backups are kept. As before, this was tested on the computer build
of the firmware; the screens were checked as rendered images.

## Credits

Palette and layout ideas from [LOOM-8](https://github.com/charlesvestal/fm1-loom) by Charles Vestal. New code by
Kerem Kilic (Ellic Studio).
