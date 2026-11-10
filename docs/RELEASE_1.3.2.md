# Melodee Custom 1.3.2

The sound pages now match the LOOM-8-style mockup that 1.3.0 was meant to follow.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## What changed from 1.3.0

- **LOOM-8's typeface.** The sound pages are set in Barlow Semi Condensed, as LOOM-8 is: 12 px labels and keycaps,
  15 px text and titles, 23 px values, 31 px effect names. Knob labels are small, dim and lower case. (Stage, lists
  and the other screens keep Rubik.)
- **Header.** The track's level as a small bar, a smaller track pill and section chip, the page title (EDIT pages
  say **Engine**, FX slots **Character**), the pattern, then transport and BPM in the title size.
- **Hints on two rows.** Four thin pill keys with their words: SELECT page, ALGO track, PRESETS section (or
  OCT+ sheet), KEYS play; on FX slots SELECT next slot, ALGO track, OCT− rack, KEYS play.
- **Tag line.** "T1 saw › unison › P1": the track and engine, the page, the pattern.
- **One colour.** Pictures, curves and gauges on these pages draw in the track's colour only, with dim lines; values
  are in the text colour.
- **Engine pictures in a frame.** A title row (the engine, the sound's name) and a caption under the picture that says
  what the values mean ("5 saws, 12.2 st apart, a sub", "OS14 · scan +17 through the table", "algorithm 2: ratios
  1 · 1 · 4"). SAW now draws its detuned saws, ENS its chord as a ladder, DRUM small pads, MIDI a socket.
- **FX "Character" pages.** The effect's name large, what it does and where it sits ("a soft saturation, after T1's
  sound, in slot 1"), and the effect's own curve: DRIVE and CRUSH their transfer at DRIVE / BITS and SHAPE, ROOM and
  REVERB their tails, the sweeps and pedals their waves or notches, NOTE ECHO its repeats. The TONE strip is gone from
  the picture (TONE is on the knob strip).

## Upgrading

Install as usual; projects, presets, settings and backups are kept. Tested on the computer build of the firmware;
the screens were checked as rendered images against the mockup.

## Credits

Palette and layout ideas from [LOOM-8](https://github.com/charlesvestal/fm1-loom) by Charles Vestal. New code by
Kerem Kilic (Ellic Studio).
