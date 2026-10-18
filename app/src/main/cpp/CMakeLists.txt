cmake_minimum_required(VERSION 3.22.1)
project(fbneo_android C CXX)

if(NOT FBNEO_DIR)
    message(FATAL_ERROR "Set -DFBNEO_DIR=/path/to/FBNeo")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -fsigned-char -D_FILE_OFFSET_BITS=64 -DINCLUDE_LUA_SUPPORT -DBUILD_ANDROID")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fsigned-char -D_FILE_OFFSET_BITS=64 -DINCLUDE_LUA_SUPPORT -DBUILD_ANDROID -Wno-narrowing -fno-strict-aliasing")

file(GLOB_RECURSE BURN_SRC CONFIGURE_DEPENDS
    "${FBNEO_DIR}/src/burn/*.cpp" "${FBNEO_DIR}/src/burn/*.c")
file(GLOB_RECURSE CPU_SRC CONFIGURE_DEPENDS
    "${FBNEO_DIR}/src/cpu/*.cpp" "${FBNEO_DIR}/src/cpu/*.c"
    "${FBNEO_DIR}/src/cpu/*.cc")

list(FILTER BURN_SRC EXCLUDE REGEX ".*/cyclone/.*")
list(FILTER CPU_SRC  EXCLUDE REGEX ".*/cyclone/.*")
list(FILTER CPU_SRC  EXCLUDE REGEX ".*/m68k_asm.*")

set(BURNER_DIR "${FBNEO_DIR}/src/burner")
set(BURNER_SRC
    ${BURNER_DIR}/cdlist.cpp
    ${BURNER_DIR}/conc.cpp
    ${BURNER_DIR}/cong.cpp
    ${BURNER_DIR}/dat.cpp
    ${BURNER_DIR}/dynhuff.cpp
    ${BURNER_DIR}/gamc.cpp
    ${BURNER_DIR}/gami.cpp
    ${BURNER_DIR}/image.cpp
    ${BURNER_DIR}/inputbuf.cpp
    ${BURNER_DIR}/ioapi.c
    ${BURNER_DIR}/luaengine.cpp
    ${BURNER_DIR}/luasav.cpp
    ${BURNER_DIR}/misc.cpp
    ${BURNER_DIR}/neocdlist.cpp
    ${BURNER_DIR}/nvram.cpp
    ${BURNER_DIR}/sshot.cpp
    ${BURNER_DIR}/state.cpp
    ${BURNER_DIR}/statec.cpp
    ${BURNER_DIR}/un7z.cpp
    ${BURNER_DIR}/unzip.c
    ${BURNER_DIR}/zipfn.cpp
    ${BURNER_DIR}/sdl/bzip.cpp
    ${BURNER_DIR}/sdl/stringset.cpp
    ${BURNER_DIR}/sdl/romdata.cpp
    ${BURNER_DIR}/sdl/ips_manager.cpp
)

set(INTF_SRC
    ${FBNEO_DIR}/src/intf/video/vid_interface.cpp
    ${FBNEO_DIR}/src/intf/audio/aud_interface.cpp
    ${FBNEO_DIR}/src/intf/input/inp_interface.cpp
)

file(GLOB ZLIB_SRC CONFIGURE_DEPENDS "${FBNEO_DIR}/src/dep/libs/zlib/*.c")
list(FILTER ZLIB_SRC EXCLUDE REGEX ".*(gzip|example|minigzip).*")

file(GLOB SPNG_SRC CONFIGURE_DEPENDS "${FBNEO_DIR}/src/dep/libs/libspng/*.c")

file(GLOB LZMA_SRC CONFIGURE_DEPENDS "${FBNEO_DIR}/src/dep/libs/lzma/*.c")
list(FILTER LZMA_SRC EXCLUDE REGEX ".*(Main|Util|Test).*")

file(GLOB_RECURSE LIB7Z_SRC CONFIGURE_DEPENDS "${FBNEO_DIR}/src/dep/libs/lib7z/*.c")
list(FILTER LIB7Z_SRC EXCLUDE REGEX ".*(Main|Test|Precomp).*")

file(GLOB LUA_SRC CONFIGURE_DEPENDS "${FBNEO_DIR}/src/dep/libs/lua/*.c")
list(FILTER LUA_SRC EXCLUDE REGEX ".*/(lua|luac|print)\\.c$")

set(STB_SRC "${FBNEO_DIR}/src/dep/libs/stb_vorbis/stb_vorbis.c")
if(NOT EXISTS "${STB_SRC}")
    set(STB_SRC "")
endif()

set(ANDROID_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/burner_android.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vid_android.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aud_android.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inp_android.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_bridge.cpp
)

add_library(fbneo SHARED
    ${BURN_SRC} ${CPU_SRC} ${BURNER_SRC} ${INTF_SRC}
    ${ZLIB_SRC} ${SPNG_SRC} ${LZMA_SRC} ${LIB7Z_SRC} ${LUA_SRC} ${STB_SRC}
    ${ANDROID_SRC}
)

target_include_directories(fbneo PRIVATE
    ${FBNEO_DIR}/src
    ${FBNEO_DIR}/src/burn
    ${FBNEO_DIR}/src/burner
    ${FBNEO_DIR}/src/cpu
    ${FBNEO_DIR}/src/dep/libs/lua
    ${FBNEO_DIR}/src/dep/libs/zlib
    ${FBNEO_DIR}/src/dep/libs/libspng
    ${FBNEO_DIR}/src/dep/libs/lzma
    ${FBNEO_DIR}/src/dep/libs/lib7z
    ${FBNEO_DIR}/src/intf/video
    ${FBNEO_DIR}/src/intf/audio
    ${FBNEO_DIR}/src/intf/input
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_library(liblog log)
find_library(libandroid android)
target_link_libraries(fbneo ${liblog} ${libandroid})
