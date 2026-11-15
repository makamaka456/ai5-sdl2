# Native build for the Android port: SDL2, SDL2_ttf, SDL2_mixer, libpng
# (fetched by android/fetch-deps.sh into deps/), libai5 and the engine.
cmake_minimum_required(VERSION 3.18)
project(ai5_android C CXX)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/deps)

# --- SDL2 ---
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/SDL SDL)

# --- SDL2_ttf (vendored FreeType) ---
set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/SDL_ttf SDL_ttf)

# --- SDL2_mixer: WAV, OGG (stb_vorbis), MP3 (minimp3) only ---
set(SDL2MIXER_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_CMD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VORBIS "STB" CACHE STRING "" FORCE)
add_subdirectory(${DEPS}/SDL_mixer SDL_mixer)

# --- libpng (static, zlib from the system/NDK) ---
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
set(PNG_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(PNG_ARM_NEON "off" CACHE STRING "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/libpng libpng)

# --- libai5 + nulib ---
set(LIBAI5 ${ROOT}/subprojects/libai5)
add_library(ai5 STATIC
	${LIBAI5}/nulib/src/buffer.c
	${LIBAI5}/nulib/src/command.c
	${LIBAI5}/nulib/src/file.c
	${LIBAI5}/nulib/src/hashset.c
	${LIBAI5}/nulib/src/port.c
	${LIBAI5}/nulib/src/sds.c
	${LIBAI5}/nulib/src/system.c
	${LIBAI5}/nulib/src/utfsjis.c
	${LIBAI5}/src/a6.c
	${LIBAI5}/src/anim.c
	${LIBAI5}/src/arc/open.c
	${LIBAI5}/src/ccd.c
	${LIBAI5}/src/cg/akb.c
	${LIBAI5}/src/cg/cg.c
	${LIBAI5}/src/cg/gp4.c
	${LIBAI5}/src/cg/gp8.c
	${LIBAI5}/src/cg/gpr.c
	${LIBAI5}/src/cg/gpx.c
	${LIBAI5}/src/cg/g16_24_32.c
	${LIBAI5}/src/cg/gcc.c
	${LIBAI5}/src/cg/png.c
	${LIBAI5}/src/game.c
	${LIBAI5}/src/lzss.c
	${LIBAI5}/src/mes/codes.c
	${LIBAI5}/src/mes/parse.c
	${LIBAI5}/src/mes/parse_aiw.c
	${LIBAI5}/src/mes/print.c
	${LIBAI5}/src/mes/print_aiw.c
	${LIBAI5}/src/mes/system.c
)
set_property(TARGET ai5 PROPERTY C_STANDARD 11)
target_compile_definitions(ai5 PRIVATE _DEFAULT_SOURCE)
target_include_directories(ai5 PUBLIC ${LIBAI5}/include ${LIBAI5}/nulib/include)
target_link_libraries(ai5 PUBLIC png_static)
target_include_directories(ai5 PRIVATE ${DEPS}/libpng ${CMAKE_CURRENT_BINARY_DIR}/libpng)

# --- engine (libmain.so, loaded by SDLActivity) ---
file(READ ${ROOT}/version.h.in VERSION_H)
string(REPLACE "@VCS_TAG@" "android" VERSION_H "${VERSION_H}")
# main.c includes "../version.h": put it in gen/ and add gen/inc to the path
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/gen/version.h "${VERSION_H}")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen/inc)

set(ENGINE ${ROOT}/src)
add_library(main SHARED
	${ENGINE}/aishimai.c
	${ENGINE}/anim.c
	${ENGINE}/asset.c
	${ENGINE}/audio_sdl_mixer.c
	${ENGINE}/backlog.c
	${ENGINE}/beyond.c
	${ENGINE}/classics.c
	${ENGINE}/cmdline.c
	${ENGINE}/cp949_table.c
	${ENGINE}/cursor.c
	${ENGINE}/debug.c
	${ENGINE}/doukyuusei.c
	${ENGINE}/doukyuusei2.c
	${ENGINE}/dungeon.c
	${ENGINE}/effect.c
	${ENGINE}/encoding.c
	${ENGINE}/gfx.c
	${ENGINE}/hanahebi.c
	${ENGINE}/ini.c
	${ENGINE}/input.c
	${ENGINE}/isaku/isaku.c
	${ENGINE}/isaku/item_window.c
	${ENGINE}/kakyuusei.c
	${ENGINE}/main.c
	${ENGINE}/map.c
	${ENGINE}/menu.c
	${ENGINE}/popup_menu.c
	${ENGINE}/savedata.c
	${ENGINE}/shangrlia.c
	${ENGINE}/shuusaku/menu.c
	${ENGINE}/shuusaku/name.c
	${ENGINE}/shuusaku/schedule.c
	${ENGINE}/shuusaku/schedule_data.c
	${ENGINE}/shuusaku/shuusaku.c
	${ENGINE}/shuusaku/status.c
	${ENGINE}/shuusaku/view.c
	${ENGINE}/sys.c
	${ENGINE}/text.c
	${ENGINE}/texthook.c
	${ENGINE}/vm.c
	${ENGINE}/yuno.c
)
set_property(TARGET main PROPERTY C_STANDARD 11)
target_compile_definitions(main PRIVATE _DEFAULT_SOURCE USE_SDL_MIXER AI5_DATA_DIR=".")
target_compile_options(main PRIVATE -Wno-unused-parameter)
target_include_directories(main PRIVATE ${ROOT}/include ${CMAKE_CURRENT_BINARY_DIR}/gen/inc)
target_link_libraries(main PRIVATE ai5 SDL2::SDL2 SDL2_ttf::SDL2_ttf SDL2_mixer::SDL2_mixer m)
if(ANDROID)
	target_link_libraries(main PRIVATE log android)
endif()
