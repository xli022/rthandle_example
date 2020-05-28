cmake_minimum_required (VERSION 3.10)

project(rthandle)

# get version to git describe --tags
execute_process(COMMAND git describe --tags
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_DESCRIBE
                ERROR_QUIET)

string(REGEX REPLACE "\n" "" GIT_DESCRIBE "${GIT_DESCRIBE}")
if("${GIT_DESCRIBE}" STREQUAL "")
	set(GIT_DESCRIBE "N/A")
endif()
# write version to file version.c
set(VERSION_STR
"// This file is generated automatically, all edit will be ignore.

// version of program, content is the output of git describe --tags
const char* git_version = \"${GIT_DESCRIBE}\";

")
if(EXISTS ${PROJECT_SOURCE_DIR}/src/version.c)
	file(READ ${PROJECT_SOURCE_DIR}/src/version.c VERSION_OLD)
else()
	set(VERSION_OLD "")
endif()
if(NOT "${VERSION_STR}" STREQUAL "${VERSION_OLD}")
	file(WRITE ${PROJECT_SOURCE_DIR}/src/version.c "${VERSION_STR}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS OFF)

# C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
find_package(Threads REQUIRED)  

find_package(OpenSSL)

set(VENDOR_TMP_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/_tmp/dest)
include_directories(${VENDOR_TMP_DEST_DIR}/include)
set(VENDOR_SPDLOG ${VENDOR_TMP_DEST_DIR}/lib/libspdlog.a)
set(VENDOR_MYSQLCPP ${VENDOR_TMP_DEST_DIR}/lib64/libmysqlcppconn8-static.a)
set(VENDOR_RABBITMQ ${VENDOR_TMP_DEST_DIR}/lib64/librabbitmq.a)

file(GLOB
     SRC_C_H
     "src/*.c"
	 "src/*.cc"
	 "src/*.hpp"
     "src/*.h")
include_directories(${PROJECT_SOURCE_DIR}/src)
add_executable(rthandle ${SRC_C_H})
set(SM_CC_ARGS
    -Wall
    -Wextra
    -pedantic
    -Werror)

target_compile_options(rthandle PRIVATE ${SM_CC_ARGS})
target_link_libraries(rthandle ${VENDOR_SPDLOG})
target_link_libraries(rthandle ${VENDOR_MYSQLCPP})
target_link_libraries(rthandle ${VENDOR_RABBITMQ})
target_link_libraries(rthandle Threads::Threads)
target_link_libraries(rthandle crypto ssl resolv mysqlclient)
