# Melodee Custom 1.2.3

A fix for saving user sounds with the MONO2K engines.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## Fix: SAVE > USER with SAW, ENS, STAT, PAR, DYN, WAV and NOIS

Saving a user sound from any of the MONO2K engines failed: the slot stayed empty. The check that decides whether a
user slot holds a sound only accepted the engines numbered below 16, and the MONO2K engines are 22 to 28. It now
accepts them, so these sounds save, load, and show up in PRESETS like any other user sound.

Everything else is as in [1.2.2](RELEASE_1.2.2.md) (the NOIS engine and the ENS first-note fix) and
[1.2.0](RELEASE_1.2.0.md) (the MONO2K engines and the MONO palette).

## Upgrading

Install as usual; projects, presets, settings and backups are kept. Saves you tried with these engines on 1.2.0 to
1.2.2 were not kept: save them again after upgrading.
