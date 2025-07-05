cmake_minimum_required(VERSION 4.0)
project(pflog C)

set(CMAKE_C_STANDARD 11)

if(APPLE)
    set(PLATFORM_SRC platform/apple/apple.c)
elseif(UNIX)
    set(PLATFORM_SRC platform/linux/linux.c)
endif()

add_library(pflog SHARED pflog.c platform/platform.h platform/platform.h ${PLATFORM_SRC})
