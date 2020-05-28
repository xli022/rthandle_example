

#!/bin/bash

CURDIR="`pwd`"

mkdir -p _tmp
cd _tmp

URL="https://cdn.mysql.com//Downloads/MySQL-8.0/mysql-boost-8.0.20.tar.gz"
DLNAME="mysql-boost-8.0.20.tar.gz"
MVNAME="${DLNAME}"
STPF="mysql-8.0.20"
TMPNAME="_tmp_mysql_lib"
DEST="dest/lib64/librabbitmq.a"

if [ ! -f "$MVNAME" ]; then
	echo "Downloading $URL"
	wget $URL
fi

if [ ! -d "$STPF" ]; then 
	echo "Extracting "
	unzip MVNAME
fi

if [ ! -f "$DEST" ]; then
	mkdir -p $TMPNAME
	cd $TMPNAME

	cmake -DWITH_BOOST=../$STPF/boost/boost_1_70_0/ -DCMAKE_INSTALL_PREFIX=../dest \
		-DCMAKE_BUILD_TYPE=Release ..
	make
	make install
fi

cd $CURDIR
