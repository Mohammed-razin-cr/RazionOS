#!/bin/esh

# This daemonizes and falls back to the existing console logging path when
# graphics are unavailable or boot-animation=off / boot-verbose is selected.
exec razion-splash
