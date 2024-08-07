#!/bin/sh
clear
rm -rf build
cmake -B build -D CMAKE_BUILD_TYPE=$3 && cmake --build build

./build/$1 $2
# ./build/xserver 47.99.146.226 9256

# sudo systemctl daemon-reload
# sudo systemctl enable mtpd
# sudo systemctl start mtpd
# sudo systemctl status mtpd
