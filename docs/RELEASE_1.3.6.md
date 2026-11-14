# Melodee Custom 1.3.6

A new drum engine, **DRM**; a new synth engine, **PULS**; MONO2K's FM sounds on FMTONE; fewer duplicate drum presets.

- **Install:** [web installer](https://theidiotwizard.github.io/melodee_custom/webapp/installer/)
  (Chrome or Edge, USB).
- **Edit:** [web editor](https://theidiotwizard.github.io/melodee_custom/webapp/editor/)

## DRM

DRM replaces the DRUM engine, in its place (engine 10). It plays the drum models of
[Hortator](https://github.com/DeadActive/hortator) by DEADACTIVE on DRUM's eight lanes (KICK, SNARE, CLAP, HATCL,
HATOP, TOM, RIM, BELL), the whole kit on the track's one voice.

- **29 models:** Machinedrum-style TRX and EFM voices (built from measurements; no code or data of the machine),
  Mutable Instruments Plaits' drums (BOOM, PUNCH, SNAP, CRACK, METAL, HISS), SKIN (a struck drum head) and FM.
- **Every lane its own model and knobs:** EDIT › Drum › DRUM LANE: LANE, MODEL, TUNE, DECAY; DRUM LANE 2 and 3: the
  model's other knobs and LEVEL. A model with eight knobs keeps seven; the eighth stays at its default.
- **The kit's knobs:** TUNE (every lane), DECY (every lane's decay), ACC (how much velocity matters), DRV.
- **Ten kits:** SUB, HOUSE, MD EFM, TECHNO, FORGE, RUST, HAND, LO-FI, MICRO, GLITCH. FORGE and RUST are the
  industrial ones.

The 808, 909, SID, CR78 and FM kits are gone. Songs and sounds that used them load on the nearest DRM kit (808: SUB,
909: HOUSE, FM: MD EFM, CR78 and SID: LO-FI) with each lane's tuning, decay and level, and their patterns as they
were. A kit is stored in the same space as before, so nothing else in a song changes. Firmware older than 1.3.6 plays
a DRM kit wrongly; keep a backup if you may go back.

The MPC pad layout is the same for every kit now.

## PULS

MONO2K's SWAV-PUL pulse machine, listed between SAW and ENS. UNISON: UNIL, UNIW, SUB1, SUB2 (sines an octave and two
down). PULSE: PW (a square at 64), PWAD (the width swept back and forth), PWRS (ON: the sweep restarts at each note),
TUNE. LFO or ENV to SHP moves the width.

PULS takes engine number 24, which was FM+STAT's until 1.2.6: a STAT sound from 1.2.0 to 1.2.5, silent since, now
plays as PULS.

## FMTONE: MONO2K's FM sounds

Ten sounds from MONO2K's library (its FM+STAT, FM+PAR and FM+DYN machines), remade on FMTONE by matching their
renders: EPIANO, FMBELL, DXBASS and GLASPAD in the places of TINE EP, BELL, FM BASS and GLASS PAD, and GROWL,
GLASSKY, FMPAD, DRONE, BUBBLES and ZAP after FIFTH RING. Some of them also set the track's LFO, DRIVE and ENV › PIT /
SHP when loaded. The power-on bass is DXBASS (in FM BASS's place).

## Fewer duplicates

Drum presets on the synth engines that DRM covers are gone: DUALVCO 808 KICK, RING METAL and TOM; ANALOG4 KICK,
ZAP TOM and NOISE HAT; PHYS TOMS (HAND DRUM stays). A project keeps its sounds (it stores their values), but a track
that loaded a moved or removed preset may show another preset's name, and a starred factory preset on these engines
may have moved its star.

Everything from [1.3.5](RELEASE_1.3.5.md) is included.
