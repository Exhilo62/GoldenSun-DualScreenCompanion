cmake_minimum_required(VERSION 3.22)
project(GoldenSunDualScreen C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT SDL2_DIR)
	set(SDL2_DIR "$ENV{SDL2_DIR}")
endif()

# The same video format as the 3DS build: RGB565. The flags go in before mGBA is added so the library and the app agree.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCOLOR_16_BIT -DCOLOR_5_6_5")

add_subdirectory(${SDL2_DIR} SDL2)

set(LIBMGBA_ONLY ON)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
foreach(opt BUILD_QT BUILD_SDL BUILD_GL BUILD_GLES2 BUILD_GLES3 BUILD_LIBRETRO BUILD_HEADLESS BUILD_EXAMPLE BUILD_TEST BUILD_SUITE
		M_CORE_GB ENABLE_SCRIPTING ENABLE_GDB_STUB USE_FFMPEG USE_ZLIB USE_PNG USE_LIBZIP USE_MINIZIP USE_LZMA USE_SQLITE3 USE_ELF USE_EPOXY
		USE_EDITLINE USE_JSON_C USE_LUA USE_DISCORD_RPC USE_FREETYPE BUILD_LTO)
	set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(M_CORE_GBA ON CACHE BOOL "" FORCE)
set(ENABLE_DEBUGGERS ON CACHE BOOL "" FORCE)
add_subdirectory(${ROOT}/third_party/mgba mgba EXCLUDE_FROM_ALL)

add_library(main SHARED
	${ROOT}/desktop/platform_sdl.c ${ROOT}/desktop/gs_game_desktop.c ${ROOT}/desktop/bgwrite_sync.c
	${ROOT}/src/app.c ${ROOT}/src/pickscreen.c ${ROOT}/src/gs_state.c ${ROOT}/src/ui.c ${ROOT}/src/gfx.c ${ROOT}/src/font_data.c ${ROOT}/src/art.c ${ROOT}/src/art_data.c
	${ROOT}/src/logo_data.c ${ROOT}/src/icon_data.c ${ROOT}/src/mapicon_data.c ${ROOT}/src/mapview.c ${ROOT}/src/menunav.c ${ROOT}/src/gs_areas.c
	${ROOT}/src/gs_items.c ${ROOT}/src/gs_abilities.c ${ROOT}/src/bestiary.c ${ROOT}/src/gs_djinn.c ${ROOT}/src/dialoglog.c ${ROOT}/src/gs_field.c
	${ROOT}/src/gs_classes.c ${ROOT}/src/gs_itemdesc.c ${ROOT}/src/mapdecode.c)
# mGBA keeps its feature switches in a target property rather than exporting them: the app must be compiled with exactly the same ones.
get_target_property(MGBA_DEFS mgba COMPILE_DEFINITIONS)
target_compile_definitions(main PRIVATE ${MGBA_DEFS})
target_include_directories(main PRIVATE ${ROOT}/third_party/mgba/include ${CMAKE_BINARY_DIR}/mgba/include)
target_link_libraries(main SDL2 mgba log android m)
