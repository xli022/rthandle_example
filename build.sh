#!/bin/bash

cd ./vendor
# too slow
# ./build_mysql_cpp.sh
# ./build_mysqlclient.sh
./build_rabbitmq.sh
./build_spdlog.sh

cd ..

mkdir -p _build
cd _build
cmake ..
make -j

