# Melodee Custom 1.0.1

Relative encoders now work with every MIDI control, not only MIDI KNOBS.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## Relative MIDI MODE for every control

With **MIDI MODE** (HOME menu) on **REL 1**, **REL 2** or **REL 3**, these now move their value from where it
is, one step per encoder step, like turning the knob on the panel:

- **CC 20–23:** the four Stage knobs of the channel's track.
- **CC 91 / 93:** MIX of the shared rack's REVERB (or ROOM) / CHORUS (or PEDAL CHORUS), stopping at its ends.
- **MIDI LEARN bindings:** every step counts, even when a controller sends several between screen updates.

They used to read every value as an absolute position, so an endless encoder in a relative mode sent the
control to about the middle of its range. **ABSOLUTE** works as before. CC 24 and MIDI KNOBS already followed
MIDI MODE and are unchanged.

To use it with a MiniLab 3: set its knobs to a relative mode in Arturia's MIDI Control Center, then choose
the matching MIDI MODE (REL 1: 63/65, REL 2: 127/1, REL 3: 15/17).

## Upgrading

- **From 1.0.0:** install as usual. Projects, presets, settings and backups are unchanged.
- **From Melodee 1.6.x:** see Upgrading in the [1.0.0 release notes](RELEASE_1.0.0.md).
