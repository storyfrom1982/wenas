#!/bin/bash


PREFIX=/tmp/$(uname -i)

CPPFLAGS="-D__SR_LOG_DEBUG__ -D___SR_MALLOC___ -D___SR_MALLOC_DEBUG___" \
CFLAGS="-I$PREFIX/include -L$PREFIX/lib" \
./configure --prefix=$PREFIX \
	--disable-shared
