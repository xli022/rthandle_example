#!/bin/bash

CURDIR="`pwd`"

mkdir -p _tmp
cd _tmp

URL="https://github.com/alanxz/rabbitmq-c/archive/v0.10.0.zip"
DLNAME="v0.10.0.zip"
MVNAME="rabbitmq.zip"
STPF="rabbitmq-c-0.10.0"
TMPNAME="_tmp_rabbitmq_lib"
DEST="dest/lib64/librabbitmq.a"

if [ ! -f "$MVNAME" ]; then
	echo "Downloading $URL"
	wget $URL
	mv $DLNAME $MVNAME
fi

if [ ! -d "$STPF" ]; then 
	echo "Extracting "
	unzip MVNAME
fi

if [ ! -f "$DEST" ]; then
	mkdir -p $TMPNAME
	cd $TMPNAME
	cmake -DCMAKE_INSTALL_PREFIX=../dest -DCMAKE_BUILD_TYPE=Release \
	    -DBUILD_EXAMPLES=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_STATIC_LIBS=ON \
        -DBUILD_TESTS=OFF \
        -DBUILD_TOOLS=OFF \
        -DBUILD_TOOLS_DOCS=OFF \
        -DBUILD_API_DOCS=OFF \
        -DRUN_SYSTEM_TESTS=OFF \
		../$STPF
	make -j
	make install
fi

cd $CURDIR
