#!/bin/sh
clear
rm -rf build
cmake -B build -D CMAKE_BUILD_TYPE=$1 && cmake --build build
./build/kang
# ./build/server
# ./build/client



# sudo systemctl daemon-reload
# sudo systemctl enable mtpd
# sudo systemctl start mtpd
# sudo systemctl status mtpd