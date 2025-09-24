#!/bin/bash
set -exuo pipefail

IMAGE=rov-rpi

docker build -t $IMAGE .
# https://unix.stackexchange.com/questions/331645/extract-file-from-docker-image
container_id=$(docker create "$IMAGE")
mkdir -p build
docker cp "$container_id:/home/work/packages/mediamtx_1.15.0_arm64.deb" ./build/