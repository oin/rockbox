#!/usr/bin/env bash

set -e

# Install the base tools
apt-get -y update && apt-get -y install build-essential texinfo git automake libtool perl zip flex bison xserver-xorg-core libsdl1.2-dev 


# Install the custom ARM toolchain
mkdir -p build-rbdev && cd /vagrant/tools && echo a | RBDEV_BUILD=/vagrant/build-rbdev ./rockboxdev.sh #&& rm -rf ../build-rbdev
# Configure Rockbox to perform a standard build
cd /vagrant && mkdir -p build-vagrant && cd build-vagrant && ../tools/configure --target=65 --type=n

# Install the ALSA sound driver
usermod -a -G audio vagrant
apt-get install -y alsa-base alsa-utils libasound2-plugins

# Configure sshd for X11 forwarding
if [[ ! -f /.sshd-x11-forward-done ]]; then
	echo "X11Forwarding yes" >>/etc/ssh/sshd_config
	echo "X11UseLocalhost no" >>/etc/ssh/sshd_config
	touch /.sshd-x11-forward-done
	echo "The virtual machine will now reboot to enable X11 forwarding."
	reboot
fi
