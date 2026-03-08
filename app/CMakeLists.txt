cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lnh_nordic_wifi)

set(APP_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(APP_SOURCES
	${APP_SOURCE_ROOT}/main.c
)

target_sources(app PRIVATE ${APP_SOURCES})

target_include_directories(app PRIVATE ${APP_SOURCE_ROOT})
