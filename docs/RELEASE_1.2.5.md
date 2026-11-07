# Melodee Custom 1.2.5

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## The mod wheel stays absolute

**CC 1** (the mod wheel) now ignores **MIDI MODE** and is always read as an absolute position, like CC 7 (LEVEL).
In 1.2.4 a relative MIDI MODE (REL 1, REL 2 or REL 3) made a wheel or fader on CC 1 step instead of jump to its
position.

Everything else is as in [1.2.4](RELEASE_1.2.4.md): in a relative MIDI MODE every other value CC steps from where it
is, including breath (CC 2), foot (CC 4) and expression (CC 11).

## Upgrading

Install as usual; projects, presets, settings and backups are kept.
