# RazionOS splash source assets

The runtime splash is rendered procedurally by `apps/razion-splash.c` with the
existing RazionOS graphics and text libraries. This directory intentionally
contains source references rather than a bitmap sequence, keeping the boot
path small and resolution-independent.
