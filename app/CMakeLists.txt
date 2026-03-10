cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
find_package(Python REQUIRED COMPONENTS Interpreter)
find_program(BUN_EXECUTABLE bun REQUIRED)
project(lnh_nordic_wifi)

set(APP_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(APP_SOURCES
	${APP_SOURCE_ROOT}/main.c
	${APP_SOURCE_ROOT}/app/bootstrap.c
	${APP_SOURCE_ROOT}/actions/actions.c
	${APP_SOURCE_ROOT}/network/network_supervisor.c
	${APP_SOURCE_ROOT}/network/wifi_lifecycle.c
	${APP_SOURCE_ROOT}/network/reachability.c
	${APP_SOURCE_ROOT}/panel/panel_auth.c
	${APP_SOURCE_ROOT}/panel/panel_http.c
	${APP_SOURCE_ROOT}/panel/panel_status.c
	${APP_SOURCE_ROOT}/ota/ota.c
	${APP_SOURCE_ROOT}/persistence/persistence.c
	${APP_SOURCE_ROOT}/relay/relay.c
	${APP_SOURCE_ROOT}/recovery/recovery.c
	${APP_SOURCE_ROOT}/scheduler/scheduler.c
)

target_sources(app PRIVATE ${APP_SOURCES})

target_include_directories(app PRIVATE ${APP_SOURCE_ROOT})

zephyr_linker_sources(SECTIONS sections-rom.ld)
zephyr_linker_section(NAME http_resource_desc_panel_http_service
	KVMA RAM_REGION GROUP RODATA_REGION)

set(PANEL_ASSET_SOURCE_DIR ${APP_SOURCE_ROOT}/panel/assets)
set(PANEL_ASSET_GENERATED_DIR ${ZEPHYR_BINARY_DIR}/include/generated/panel)
set(PANEL_ASSET_BUILD_DIR ${ZEPHYR_BINARY_DIR}/panel-assets)
set(PANEL_INDEX_HTML_SOURCE ${PANEL_ASSET_SOURCE_DIR}/index.html)
set(PANEL_LOGIN_HTML_SOURCE ${PANEL_ASSET_SOURCE_DIR}/login.html)
set(PANEL_MAIN_JS_SOURCE ${PANEL_ASSET_SOURCE_DIR}/main.js)
set(PANEL_MAIN_JS_MINIFIED ${PANEL_ASSET_BUILD_DIR}/main.min.js)

file(MAKE_DIRECTORY ${PANEL_ASSET_GENERATED_DIR})
file(MAKE_DIRECTORY ${PANEL_ASSET_BUILD_DIR})

add_custom_command(
	OUTPUT ${PANEL_MAIN_JS_MINIFIED}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${PANEL_ASSET_BUILD_DIR}
	COMMAND
		${BUN_EXECUTABLE}
		build
		${PANEL_MAIN_JS_SOURCE}
		--target=browser
		--format=iife
		--minify
		--outfile
		${PANEL_MAIN_JS_MINIFIED}
	DEPENDS ${PANEL_MAIN_JS_SOURCE}
	COMMENT "Minifying panel JavaScript with Bun"
	VERBATIM
)

set_source_files_properties(${PANEL_MAIN_JS_MINIFIED} PROPERTIES GENERATED TRUE)

generate_inc_file_for_target(
	app
	${PANEL_INDEX_HTML_SOURCE}
	${PANEL_ASSET_GENERATED_DIR}/index.html.gz.inc
	--gzip
)

generate_inc_file_for_target(
	app
	${PANEL_LOGIN_HTML_SOURCE}
	${PANEL_ASSET_GENERATED_DIR}/login.html.gz.inc
	--gzip
)

generate_inc_file_for_target(
	app
	${PANEL_MAIN_JS_MINIFIED}
	${PANEL_ASSET_GENERATED_DIR}/main.js.gz.inc
	--gzip
)
