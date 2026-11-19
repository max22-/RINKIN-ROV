#!/bin/bash
set -euxo pipefail

HOST=raspberrypi

if [ ! -f setup/mediamtx/mediamtx ]; then
  pushd setup/mediamtx
  wget https://github.com/bluenviron/mediamtx/releases/download/v1.15.1/mediamtx_v1.15.1_linux_arm64.tar.gz
  tar -xvf mediamtx_v1.15.1_linux_arm64.tar.gz mediamtx
  rm mediamtx_v1.15.1_linux_arm64.tar.gz
  popd
fi

pushd rinkin
GOOS=linux GOARCH=arm64 go build
popd

rsync -avh setup pi@${HOST}:~
rsync -avh rinkin pi@${HOST}:~
ssh pi@${HOST} "cd setup && bash setup.sh"

