#!/bin/esh

if [ ! -b /dev/cdrom0 ] then exit 0

echo -n "Mounting CD..." > /dev/pex/splash

insmod /mod/iso9660.ko
mount iso /dev/cdrom0 /cdrom

