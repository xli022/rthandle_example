#!/bin/bash

CURDIR="`pwd`"

mkdir -p _tmp
cd _tmp

if [ ! -f "mytmp.tar.gz" ]; then
	echo "Downloading https://cdn.mysql.com//Downloads/Connector-C++/mysql-connector-c++-8.0.20-src.tar.gz"
	wget https://cdn.mysql.com//Downloads/Connector-C++/mysql-connector-c++-8.0.20-src.tar.gz 
	mv mysql-connector-c++-8.0.20-src.tar.gz mytmp.tar.gz
fi

if [ ! -d "./mysql-connector-c++-8.0.20-src" ]; then 
	echo "Extracting "
	tar -xf mytmp.tar.gz
fi

if [ ! -f "dest/lib64/libmysqlcppconn8-static.a" ]; then
	mkdir -p _tmp_mypp_lib
	cd _tmp_mypp_lib
	cmake -DCMAKE_INSTALL_PREFIX=../dest -DBUILD_STATIC=ON -DCMAKE_BUILD_TYPE=Release \
		../mysql-connector-c++-8.0.20-src
	make -j
	make install
fi

cd $CURDIR
