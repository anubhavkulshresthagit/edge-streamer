#!/bin/bash
set -euxo pipefail
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config \
    libavformat-dev libavcodec-dev libavutil-dev

apt-get clean
rm -rf /var/lib/apt/lists/*