#!/bin/sh
clear
rm -rf build
cmake -B build -D CMAKE_BUILD_TYPE=$5 && cmake --build build

# ./build/$1 $2 $3 $4
# ./build/xserver 47.99.146.226 9256

./build/xserver

# sudo systemctl daemon-reload
# sudo systemctl enable mtpd
# sudo systemctl start mtpd
# sudo systemctl status mtpd
