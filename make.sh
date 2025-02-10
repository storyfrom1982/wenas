g!/bin/sh
clear
# rm -rf build
rm -rf build/xpeer
rm -rf tmp
# rm -rf /tmp/wenas
cmake -B build -D CMAKE_BUILD_TYPE=$2 && cmake --build build

./build/$1
# ./build/xserver 47.99.146.226 9256

# ./build/xserver

# sudo systemctl daemon-reload
# sudo systemctl enable mtpd
# sudo systemctl start mtpd
# sudo systemctl status mtpd
