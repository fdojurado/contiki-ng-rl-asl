#!/usr/bin/env bash
set -e

echo "🔧 Installing Contiki-NG native toolchains and dependencies..."

# Update system
sudo apt-get update

# 1️⃣ Core development tools
sudo apt-get install -y \
  build-essential doxygen git git-lfs curl wireshark python3-serial srecord rlwrap \
  automake autoconf bison flex libtool libffi-dev python3-setuptools \
  cmake ninja-build wget unzip net-tools

# Allow non-root packet capture
sudo usermod -a -G wireshark vscode || true

# 2️⃣ ARM GCC (for CC2538/Zoul platforms)
echo "⬇️ Installing ARM GCC toolchain..."
wget -nv https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/9-2020q2/gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2
tar -xjf gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2
sudo cp -r gcc-arm-none-eabi-9-2020-q2-update /opt/
echo "export PATH=\$PATH:/opt/gcc-arm-none-eabi-9-2020-q2-update/bin" >> ~/.bashrc
source ~/.bashrc
rm -f gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2

# 3️⃣ MSP430 GCC (for Cooja/MSPSim)
echo "⬇️ Installing MSP430 GCC 4.7.2..."
wget -nv http://simonduq.github.io/resources/mspgcc-4.7.2-compiled.tar.bz2
tar xjf mspgcc-4.7.2-compiled.tar.bz2 -C /tmp/
sudo cp -rf /tmp/msp430/* /usr/local/
rm -rf /tmp/msp430 mspgcc-4.7.2-compiled.tar.bz2

# 4️⃣ Java for Cooja Simulator
sudo apt-get install -y default-jdk ant
sudo update-alternatives --set java /usr/lib/jvm/java-1.17.0-openjdk-amd64/bin/java || true

# 5️⃣ Optional: MQTT/CoAP tools (for IoT apps)
sudo apt-get install -y npm mosquitto mosquitto-clients
sudo npm install -g coap-cli
sudo ln -sf /usr/bin/nodejs /usr/bin/node || true

# 6️⃣ User group setup (for USB devices, optional)
sudo usermod -a -G plugdev vscode || true
sudo usermod -a -G dialout vscode || true

# 7️⃣ Ignore modem-manager interference (optional)
echo 'ATTRS{idVendor}=="0451", ATTRS{idProduct}=="16c8", ENV{ID_MM_DEVICE_IGNORE}="1"' | sudo tee -a /lib/udev/rules.d/77-mm-usb-device-blacklist.rules

# 8️⃣ Clean up
sudo apt-get clean

echo "✅ Contiki-NG development environment fully installed!"
echo "You may need to restart your terminal or run 'source ~/.bashrc' to load toolchains."
