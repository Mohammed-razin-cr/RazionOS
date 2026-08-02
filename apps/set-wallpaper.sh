#!/bin/esh

# Validate argument, make sure the image exists and can be loaded
if [ -z "$1" ] then exec sh -c "echo 'usage: $0 WALLPAPER [fill|center|stretch]'"
export-cmd NEW_WALLPAPER realpath "$1"
if [ ! -e "$NEW_WALLPAPER" ] then exec sh -c "echo '$0: $1: no such file or directory'"
if not check-image -q "$NEW_WALLPAPER" then exec sh -c "echo '$0: $1: not a valid image'"

# Validate and persist the display mode.
export WALLPAPER_MODE fill
if [ "$2" = "center" ] then export WALLPAPER_MODE center
if [ "$2" = "stretch" ] then export WALLPAPER_MODE stretch

# Write the wallpaper to the config
echo "wallpaper=$NEW_WALLPAPER" > $HOME/.wallpaper.conf
echo "mode=$WALLPAPER_MODE" >> $HOME/.wallpaper.conf

# Tell the desktop to reload it
export-cmd DESKTOP cat $HOME/.wallpaper.pid
if [ -z "$DESKTOP" ] then exec sh -c "echo '$0: No wallpaper running?'"
kill -SIGUSR1 $DESKTOP
killall -SIGUSR1 panel
