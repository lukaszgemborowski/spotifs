cmake_minimum_required(VERSION 2.8)
project(spotifs)
include(FindPkgConfig)

# FUSE & pthread
pkg_search_module(FUSE fuse)
pkg_search_module(GLIB2 glib-2.0)
find_package(Threads)

set(SOURCE_FILES
    libspotify-12.1.51-Linux-x86_64-release/include/libspotify/api.h
    src/context.c
    src/context.h
    src/fs.c
    src/fs.h
    src/spotify.c
    src/spotify.h
    src/spotify_appkey.h
    src/logger.c
    src/logger.h
    src/support.h
    src/support.c
    src/sfs.h
    src/sfs.c
    src/wave.c
    src/wave.h)

# libspotify
include_directories(${spotifs_SOURCE_DIR}/libspotify-12.1.51-Linux-x86_64-release/include)
include_directories(${spotifs_SOURCE_DIR}/src)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${spotifs_SOURCE_DIR}/libspotify-12.1.51-Linux-x86_64-release/lib)

add_definitions(-D_FILE_OFFSET_BITS=64)
add_executable(spotifs ${SOURCE_FILES} src/main.c)
add_executable(spotify_cli ${SOURCE_FILES} src/main_spotify_cli.c)

target_link_libraries(spotifs ${CMAKE_THREAD_LIBS_INIT} spotify ${FUSE_LIBRARIES} ${GLIB2_LIBRARIES} m)
target_link_libraries(spotify_cli ${CMAKE_THREAD_LIBS_INIT} spotify ${FUSE_LIBRARIES} ${GLIB2_LIBRARIES} m)