#!/bin/sh
clear
rm -rf build
cmake -B build -D CMAKE_BUILD_TYPE=$1 && cmake --build build
# ./build/kapp
#./build/xleader
./build/xfollower 47.99.146.226 9256



# sudo systemctl daemon-reload
# sudo systemctl enable mtpd
# sudo systemctl start mtpd
# sudo systemctl status mtpd
