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
        "T_ENV_COEF", "T_VOL", "T_EXP2F", "T_RED_W"]   # (1.2.6: the FM+ tables gone with STAT PAR DYN)
wavewant = ["T_WAVES64", "T_WAVES64_DC", "T_WAVES64_G", "T_WAVE64_NAME"]
defs = ["ENS_OFF", "ENS_CINC", "ENS_CDEP", "GN_G0", "GN_G1", "GN_G2", "GN_GT", "GN_KW", "GN_K0", "GN_K1", "GN_K2", "GN_KB"]
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
            "/* The tables of MONO2K's SWAV-SAW, SWAV-ENS, DIGI-WAV and GND-NOIS machines (eng_m2k.c),\n"
            " * extracted unchanged from MONO2K's generated firmware/gen/m2k_tables.h and m2k_waves64.h at commit " + rev + "\n"
            " * by tools/m2k_tables.py. The 64 waves are the MONO2K owner's (assets/waves/mono2k64), in the resource region. */\n"
            "#ifndef M2K_TABLES_H\n#define M2K_TABLES_H\n")
    for x in d: f.write(x + "\n")
    for n in want: f.write(a[n].replace("static const", "static const __attribute__((unused))", 1) + "\n")
    for n in wavewant:
        x = b[n].replace("static const", "static const __attribute__((unused))", 1)
        if n == "T_WAVES64":         # 32 KiB: the resource region (res.c), read in place
            x = x.replace("[64 * 256] =", "[64 * 256] MELODEE_RES =", 1)
        f.write(x + "\n")
    f.write("#endif\n")
