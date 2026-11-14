# x0x native fp32 math

Imported from Charles Vestal's fm1-x0x, commit
`201e5c5c1ac056a028a006bd1eabb23ac1bb174d`:
https://github.com/charlesvestal/fm1-x0x/tree/201e5c5c1ac056a028a006bd1eabb23ac1bb174d

GPL-3.0-only. Preserve the original notices in these files.
`fastmath.h` contains x0x's pi32v2-friendly math substrate. The build enables
`-mfprev1`; both cores disable floating-point exception traps while retaining
stack protection, matching x0x's arithmetic requirements.

The x0x 909 drum models Melodee once carried here (credited by x0x to 9W9 by
athousanddetails, derived from ER99 by Matthew Cieplak) left with the DRUM
engine in Melodee Custom 1.3.6; DRM (`../eng_drum.c`, `../drm/`) replaced them.
