#!/bin/esh

export-cmd HOSTNAME cat /etc/hostname

echo -n "@razion:core" > /dev/pex/splash

if [ -z "$HOSTNAME" ] then exec hostname "localhost" else exec hostname "$HOSTNAME"
