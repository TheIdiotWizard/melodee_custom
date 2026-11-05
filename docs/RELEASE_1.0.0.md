# Melodee Custom 1.0.0

The first release of Melodee Custom, a customised build of
[Melodee 1.6.1](https://github.com/keremimo/melodee/releases/tag/v1.6.1) for the M-VAVE FM-1.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB). Back up first.
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

**Not validated on hardware beyond normal use.** The changes are covered by Melodee's host tests and new
ones, but the new engines' CPU cost and the per-track effects have not been measured on a device. Please
report what works and what does not.

## Engines

- Felucca's **WHEEL** organ is back.
- Felucca's **PHASE** (with FOLD), **LOFI**, **VOICE**, **TRIO** and **PHYS** are back, at their original
  engine numbers, so their presets and projects load as they were.
- **CZ-1** and **SID** are retired. DRUM keeps its KIT SID.

## MIDI CCs

Each CC acts on the track that the channel it arrives on plays, not on the screen's selection:

- **CC 20–23:** that track's four Stage knobs.
- **CC 24:** that track's sound, stepped as the PRESETS knob does.
- **CC 91 / 93:** MIX of the shared rack's REVERB (or ROOM) / CHORUS (or PEDAL CHORUS).

## Per-track effects

Each track's three FX slots take any audio effect again (drive, crush, phaser, flanger, chorus, room,
reverb and so on) as well as NOTE TAIL and NOTE ECHO. They run in series on the track, before its level, pan
and shared sends. This is Melodee's own insert code, restored. Effect buffers come from the on-demand memory
arena. One REVERB runs across all racks: choosing it in a track's slot or a shared slot turns it off where it
was. A track's effects use CPU, so watch for overloads with heavy effects on several tracks.

## Other changes

- **DUSK colour theme:** CONTRAST with muted purple, pink, yellow and green tracks (GLO > COLOR).
- **GLO > WEIGHTS:** voice weights for P5, FM6, DRUM, PHASE, LOFI, VOICE, TRIO, WHEEL and PHYS on three
  pages; GUARD and BUDGET on the third.
- **Fix:** a key, or a ROUT SEL MIDI note, could play another track's sound while the screen briefly
  selected that track.

## Upgrading

- **From Melodee 1.6.x:** back up in the installer first. Projects, presets and settings load, with two
  exceptions: tracks saved with CZ-1 play silent, and tracks saved with SID load as LOFI (SID's old engine
  number), so they sound different. Older projects' per-track audio effects play again.
- **Going back to Melodee:** restore the backup you made there. Melodee 1.6.1 does not play the restored
  engines' tracks as saved here, and it turns per-track audio effects off.
- The version number restarts at 1.0.0 for this build; the installer and the menu show **v1.0.0**.
