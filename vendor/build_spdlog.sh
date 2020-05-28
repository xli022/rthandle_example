#!/bin/bash

CURDIR="`pwd`"

mkdir -p _tmp
cd _tmp

URL="https://github.com/gabime/spdlog/archive/v1.6.0.zip"
DLNAME="v1.6.0.zip"
MVNAME="spdlog.zip"
STPF="spdlog-1.6.0"
TMPNAME="_tmp_spdlog_lib"
DEST="dest/lib/libspdlog.a"

if [ ! -f "rabbitmq.zip" ]; then
	echo "Downloading $URL"
	wget $URL
	mv $DLNAME $MVNAME
fi

if [ ! -d "$STPF" ]; then 
	echo "Extracting "
	unzip $$MVNAME
fi

if [ ! -f "$DEST" ]; then
	mkdir -p $TMPNAME
	cd $TMPNAME
	cmake -DCMAKE_INSTALL_PREFIX=../dest -DCMAKE_BUILD_TYPE=Release \
		../$STPF
	make -j
	make install
fi

cd $CURDIR