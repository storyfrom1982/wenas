#!/bin/bash

if [ -z "$1" ]; then
    BUILD_TYPE=Debug
else
    BUILD_TYPE=$1
fi

rm -rf build
rm -rf tmp
cmake -B build -D CMAKE_BUILD_TYPE=$BUILD_TYPE && cmake --build build

systemctl stop xltpd.service
systemctl disable xltpd.service

cp -rfv xltpd.service /etc/systemd/system/xltpd.service
systemctl daemon-reload

systemctl enable xltpd.service
systemctl start xltpd.service



