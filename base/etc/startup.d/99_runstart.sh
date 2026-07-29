#!/bin/esh

export-cmd START kcmdline -g start
export-cmd BOOT_ANIMATION kcmdline -g boot-animation

# We haven't actually hit a login yet, so make sure these are set here...
export USER=root
export HOME=/home/root
export PATH=/usr/bin:/bin

export-cmd TZ_OFFSET find-timezone
export-cmd GETTY_ARGS qemu-fwcfg opt/org.toaruos.gettyargs

echo -n "Launching startup application..." > /dev/pex/splash
echo -n "!quit" > /dev/pex/splash

if [ "$START" = "--vga" ] then exec /bin/terminal-vga -l
if [ "$START" = "--headless" ] then exec /bin/getty ${GETTY_ARGS}
if [ "$BOOT_ANIMATION" = "off" ] then
	if [ -z "$START" ] then exec /bin/compositor else exec /bin/compositor -- $START
fi
if kcmdline -q boot-verbose then
	if [ -z "$START" ] then exec /bin/compositor else exec /bin/compositor -- $START
fi
if kcmdline -q debug then
	if [ -z "$START" ] then exec /bin/compositor else exec /bin/compositor -- $START
fi
if [ -z "$START" ] then exec /bin/compositor --razion-boot-fade else exec /bin/compositor --razion-boot-fade -- $START
