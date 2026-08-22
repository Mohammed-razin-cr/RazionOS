#!/bin/esh

# A raw ext2 disk attached as the first ATA hard disk becomes the durable
# home volume. The live ISO remains fully usable when no disk is attached.
if [ ! -e /dev/hda ] then
	echo "not-present" > /tmp/razion-persistence-status
	exit 0
fi

echo "disk-detected" > /tmp/razion-persistence-status

echo -n "Mounting persistent home..." > /dev/pex/splash

# Preserve the live-image account skeleton before covering /home.
mkdir -p /tmp/razion-home-seed
cp -r /home/. /tmp/razion-home-seed

insmod /mod/ext2.ko
if ! mount ext2 /dev/hda,rw /home then
	echo "mount-failed" > /tmp/razion-persistence-status
	echo "RazionOS: persistent disk detected but could not be mounted; using live home" > /dev/console
	exit 0
fi

echo "mounted" > /tmp/razion-persistence-status

# Initialize a newly-created data disk exactly once. Existing user data wins.
# The account skeleton itself is also a completion record, so an interrupted
# first boot never overwrites files that were already copied successfully.
if [ ! -e /home/.razion-persistent-home ] && [ ! -e /home/local/.yutanirc ]; then
	if cp -r /tmp/razion-home-seed/. /home then
		if touch /home/.razion-persistent-home; then
			chmod 600 /home/.razion-persistent-home
		fi
	else
		echo "initialization-failed" > /tmp/razion-persistence-status
		echo "RazionOS: persistent home initialization failed" > /dev/console
		exit 1
	fi
fi

echo "ready" > /tmp/razion-persistence-status
echo "RazionOS: persistent home mounted from /dev/hda" > /dev/console
