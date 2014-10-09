#!/usr/bin/env bash

set -e

# Install the base tools
apt-get -y update && apt-get -y install build-essential texinfo git automake libtool perl zip flex bison xserver-xorg-core libsdl1.2-dev 


# Install the custom ARM toolchain
mkdir -p /home/vagrant/rbdev-dl && mkdir -p /home/vagrant/rbdev-build && cd /vagrant/tools && echo a | sudo RBDEV_BUILD=/home/vagrant/rbdev-build RBDEV_DOWNLOAD=/home/vagrant/rbdev-dl ./rockboxdev.sh && rm -rf /home/vagrant/rbdev-build && rm -rf /home/vagrant/rbdev-dl
# Configure Rockbox to perform a standard + simulator build
cd /vagrant && mkdir -p build-vagrant && cd build-vagrant && ../tools/configure --target=65 --type=n
# Configure Rockbox to perform a simulator build
cd /vagrant && mkdir -p build-simulator && cd build-simulator && ../tools/configure --target=65 --type=s

# Install the ALSA sound driver
usermod -a -G audio vagrant
apt-get install -y alsa-base alsa-utils libasound2-plugins

# Configure sshd for X11 forwarding
if [[ ! -f /.sshd-x11-forward-done ]]; then
	echo "X11Forwarding yes" >>/etc/ssh/sshd_config
	echo "X11UseLocalhost no" >>/etc/ssh/sshd_config
	touch /.sshd-x11-forward-done
fi

echo "Please vagrant reload before using this virtual machine."
