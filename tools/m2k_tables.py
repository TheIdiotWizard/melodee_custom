#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Extract the tables MONO2K's seven ported machines need (MONO2K's generated firmware/gen/m2k_tables.h and
m2k_waves64.h, https://github.com/InigoHernandez/mono2k) into firmware/src/m2k_tables.h, unchanged.

  m2k_tables.py MONO2K/firmware/gen/m2k_tables.h MONO2K/firmware/gen/m2k_waves64.h firmware/src/m2k_tables.h REV
"""

import re, sys
src, waves, out, rev = sys.argv[1:5]
want = ["T_SIN", "T_SW1LO", "T_SW1HI",
        "T_SW2LO", "T_SW2HI", "T_SWSUBX", "T_SWHP", "T_ENS_PCH", "T_ENS_PIDX", "T_ENS_PNAME", "T_ENS_G", "T_ENS_W",
        "T_ENV_COEF", "T_VOL", "T_EXP2F",   # (1.2.6: the FM+ tables gone with STAT PAR DYN; 1.2.8: T_RED_W with NOIS)
        "T_LPK", "T_HPK", "T_LPC", "T_HPC", "T_DIST_H", "T_DIST_C", "T_DIST_M"]   # (1.3.6: the chain, FILTER's M2K)
quarter = ["T_FG", "T_FG1"]      # the chain's cutoff laws (1.3.6): every fourth entry (1/24 octave; read between them)
wavewant = ["T_WAVES64", "T_WAVES64_DC", "T_WAVES64_G", "T_WAVE64_NAME"]
defs = ["ENS_OFF", "ENS_CINC", "ENS_CDEP", "F_TOP", "F_LP2", "F_HP1"]   # (1.2.8: GND-NOIS's GN_* gone with NOIS)
def grab(text, names):
    got = {}
    for n in names:
        m = re.search(r"^static const [^\n]*?\b" + n + r"\[[^\n]*?=\s*\{.*?\};", text, re.S | re.M)
        if not m: raise SystemExit("missing " + n)
        got[n] = m.group(0)
    return got
t = open(src).read(); w = open(waves).read()
a = grab(t, want); b = grab(w, wavewant)
d = [re.search(r"^#define " + n + r"\b.*$", t, re.M).group(0) for n in defs]
with open(out, "w") as f:
    f.write("/* SPDX-License-Identifier: GPL-3.0-only\n"
            " * Copyright (C) 2026 Sequencist (Inigo Hernandez), MONO2K: https://github.com/InigoHernandez/mono2k */\n"
            "/* The tables of MONO2K's SWAV-SAW, SWAV-PUL, SWAV-ENS and DIGI-WAV machines (eng_m2k.c) and its chain (eng_uflt.c),\n"
            " * extracted unchanged from MONO2K's generated firmware/gen/m2k_tables.h and m2k_waves64.h at commit " + rev + "\n"
            " * by tools/m2k_tables.py. The 64 waves are the MONO2K owner's (assets/waves/mono2k64), in the resource region. */\n"
            "#ifndef M2K_TABLES_H\n#define M2K_TABLES_H\n")
    for x in d: f.write(x + "\n")
    for n in want: f.write(a[n].replace("static const", "static const __attribute__((unused))", 1) + "\n")
    for n in quarter:
        m = re.search(r"^static const (\w+) " + n + r"\[\d+\] = \{(.*?)\};", t, re.S | re.M)
        v = [int(x) for x in re.findall(r"\d+", m.group(2))]
        q = v[::4] + ([v[-1]] if (len(v) - 1) % 4 else [])
        f.write("static const __attribute__((unused)) %s %s4[%d] = {%s};   /* %s's every fourth (its last too) */\n"
                % (m.group(1), n, len(q), ", ".join(map(str, q)), n))
    for n in wavewant:
        x = b[n].replace("static const", "static const __attribute__((unused))", 1)
        if n == "T_WAVES64":         # 32 KiB: the resource region (res.c), read in place
            x = x.replace("[64 * 256] =", "[64 * 256] MELODEE_RES =", 1)
        f.write(x + "\n")
    f.write("#endif\n")
