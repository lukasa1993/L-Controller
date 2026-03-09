cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lnh_nordic_wifi)

set(APP_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(APP_SOURCES
	${APP_SOURCE_ROOT}/main.c
	${APP_SOURCE_ROOT}/app/bootstrap.c
	${APP_SOURCE_ROOT}/network/network_supervisor.c
	${APP_SOURCE_ROOT}/network/wifi_lifecycle.c
	${APP_SOURCE_ROOT}/network/reachability.c
	${APP_SOURCE_ROOT}/panel/panel_http.c
	${APP_SOURCE_ROOT}/persistence/persistence.c
	${APP_SOURCE_ROOT}/recovery/recovery.c
)

target_sources(app PRIVATE ${APP_SOURCES})

target_include_directories(app PRIVATE ${APP_SOURCE_ROOT})

zephyr_linker_sources(SECTIONS sections-rom.ld)
zephyr_linker_section(NAME http_resource_desc_panel_http_service
	KVMA RAM_REGION GROUP RODATA_REGION)
