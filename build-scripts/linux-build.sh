#!/bin/bash


PREFIX=/tmp/$(uname -i)

CPPFLAGS="-D___LOG_DEBUG___ -D___MEMORY_MANAGER___ -D___MEMORY_DEBUG___" \
CFLAGS="-I$PREFIX/include -L$PREFIX/lib" \
./configure --prefix=$PREFIX \
	--disable-shared
