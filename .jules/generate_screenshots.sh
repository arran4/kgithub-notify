#!/bin/bash
set -e

echo "Building using vfs docker with testing repository from Debian..."
cat << 'DOCKERFILE' > .jules/Dockerfile.test
FROM debian:testing

ENV DEBIAN_FRONTEND=noninteractive
ENV QT_QPA_PLATFORM=offscreen

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    qt6-base-dev \
    qt6-svg-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    libkf6wallet-dev \
    libkf6notifications-dev \
    libkf6coreaddons-dev \
    libkf6xmlgui-dev \
    libkf6configwidgets-dev \
    libkf6i18n-dev \
    libgl1-mesa-dev

WORKDIR /workspace
DOCKERFILE

docker build -t kgithub-notify-builder -f .jules/Dockerfile.test .

docker run --rm -v $(pwd):/workspace -w /workspace kgithub-notify-builder /bin/bash -c "
mkdir -p build && cd build &&
cmake .. &&
make -j\$(nproc) kgithub-notify-mock &&
QT_QPA_PLATFORM=offscreen ./kgithub-notify-mock
"
