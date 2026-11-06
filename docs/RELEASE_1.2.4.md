# Melodee Custom 1.2.4

A filter for the MONO2K engines, and relative MIDI encoders on every value CC.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## FILTER for SAW, ENS, STAT, PAR, DYN, WAV and NOIS

These seven engines get a **FILTER** page under EDIT: TRIO's filter, after the engine. Other engines (LOFI, VOICE,
TRIO, WHEEL, PHYS, DRUM, MIDI Track) don't show it and don't change.

| Control | What it does |
|---|---|
| **MODE** | OFF (default), LP, BP, HP or NOT (notch). |
| **CUT** | The cutoff. It wanders very slightly, as TRIO's does. |
| **RES** | Resonance: strong, but it never self-oscillates. |
| **ENV** | How far the envelope moves the cutoff (the same control as ENV DEST's FLT). |

- With MODE **OFF** nothing changes: every sound plays exactly as in 1.2.3. With it on, the track's ENV and LFO
  routings to FLT move the cutoff instead of the engine control they used to move.
- **CC 74** sets CUT and **CC 71** sets RES on these engines (they did nothing on them before).
- **STAT** and **PAR** lose their **TONE** control, a low-pass on their modulators; it is now fully open and the
  slot shows "-". Their Stage knob 3 is the FILTER's CUT. FM BASS, PAR ORGN and PAR BASS, whose TONE was low, start
  with a low-pass instead; the other STAT and PAR presets are a little brighter than before. A saved STAT or PAR
  sound plays with TONE open.
- Correction: the 1.2.2 notes said NOIS is shaped by "the track's filter". Melodee has no filter shared by every
  engine, so until now NOIS (and the other MONO2K engines) had none. The FILTER page is that filter.

The filter costs nothing when OFF. When it is on, it adds a fixed amount per voice: on the computer build about a
quarter more for SAW and ENS and up to about three quarters more for the lighter WAV and NOIS. If a dense pattern
stutters with the filter on, use fewer voices, lower BUDGET, or raise that engine's weight in WEIGHTS.

## Relative MIDI MODE on every value CC

With **MIDI MODE** (HOME menu) set to REL 1, REL 2 or REL 3, every CC that sets a value now steps from where the value
is, except **CC 7** (LEVEL), which stays absolute for faders. Before, only CC 20–24, 91, 93 and MIDI LEARN bindings
did; CC 72 / 73 (release / attack, an Arturia MiniLab's default) jumped to the middle of their range instead.

Now relative too: CC 72 / 73 / 75 (release, attack, decay), CC 74 / 71, CC 5 (glide), CC 10 (pan), and CC 1, 2, 4
and 11 (the wheel, breath, foot and expression, which keep a position the encoder moves; CC 121 resets it).
Switches and system CCs (64 sustain, 65, bank select, RPN / NRPN, 120–127) are unchanged.

## Upgrading

Install as usual; projects, presets, settings and backups are kept, all with the FILTER OFF. As before, tested on
the computer build of the firmware, not yet on an FM-1.
