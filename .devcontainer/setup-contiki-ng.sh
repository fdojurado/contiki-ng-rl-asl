#!/usr/bin/env bash
set -e

echo "🔧 Installing Contiki-NG dependencies..."

# Basic build tools
sudo apt-get update
sudo apt-get install -y \
  git build-essential automake autoconf \
  bison flex libtool libffi-dev python3 python3-pip \
  python3-setuptools python3-serial python3-yaml \
  python3-click python3-pyelftools cmake ninja-build \
  wget curl unzip

# Optional: tools for Cooja simulator
sudo apt-get install -y openjdk-17-jdk ant default-jre

# Optional: MSP430 / Z1 cross-compilers
sudo apt-get install -y gcc-msp430 msp430-libc msp430mcu

# Optional: Native build target dependencies (for simulation)
sudo apt-get install -y libpcap-dev

# Clean up
sudo apt-get clean

echo "✅ Contiki-NG native environment ready!"
