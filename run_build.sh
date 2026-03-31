#!/bin/bash
sudo apt update && sudo apt install -y build-essential cmake qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools libkf6wallet-dev libkf6notifications-dev libkf6coreaddons-dev libkf6xmlgui-dev libkf6configwidgets-dev libkf6i18n-dev libgl1-mesa-dev
mkdir -p build && cd build && cmake .. && make -j$(nproc)
