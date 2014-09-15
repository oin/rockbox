#!/usr/bin/env bash

# Install the base tools
pacman -Sy --noconfirm git base-devel texinfo zip libunistring
# Install the custom ARM toolchain
cd /vagrant/tools && echo a | ./rockboxdev.sh
# Configure Rockbox to perform a standard build
cd /vagrant && mkdir build-vagrant && cd build-vagrant && ../tools/configure --target=65 --type=n
