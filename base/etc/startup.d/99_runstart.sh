#!/bin/esh

export-cmd START kcmdline -g start

# We haven't actually hit a login yet, so make sure these are set here...
export USER=root
export HOME=/home/root
export PATH=/usr/bin:/bin

export-cmd TZ_OFFSET find-timezone
export-cmd GETTY_ARGS qemu-fwcfg opt/org.toaruos.gettyargs

echo -n "@razion:graphics" > /dev/pex/splash
echo -n "@razion:desktop" > /dev/pex/splash
echo -n "!ready" > /dev/pex/splash

# Let the splash finish its bounded exit transition before the compositor
# takes ownership of the framebuffer. Milestone progress itself is event-driven.
sleep 0.5

if [ "$START" = "--vga" ] then exec /bin/terminal-vga -l
if [ "$START" = "--headless" ] then exec /bin/getty ${GETTY_ARGS}
if [ -z "$START" ] then exec /bin/compositor --razion-boot-fade else exec /bin/compositor --razion-boot-fade -- $START
