cmake_minimum_required(VERSION 3.10.0)

PROJECT("3D 803 Emulator" C)

ADD_EXECUTABLE(803 Gtk.c Gles.c Shaders.c ShaderDefinitions.c LoadPNG.c Main.c
  ObjLoader.c Keyboard.c Parse.c 3D.c WGbuttons.c Hands.c Sound.c Common.c
  Wiring.c Cpu.c PowerCabinet.c Charger.c Logging.c Emulate.c E803ops.c PTS.c
  config.h Gtk.h Gles.h Shaders.h ShaderDefinitions.h LoadPNG.h ObjLoader.h Keyboard.h
  Parse.h 3D.h WGbuttons.h wg-definitions.h Hands.h Sound.h Common.h
  Wiring.h Cpu.h PowerCabinet.h Charger.h Logging.h Emulate.h E803ops.h PTS.h)  

SET(CMAKE_C_FLAGS "-std=gnu99  -g  -Wall -Wextra -Wunused -Wconversion"
"-Wundef -Wcast-qual -Wmissing-prototypes "
"-Wredundant-decls -Wunreachable-code -Wwrite-strings -Warray-bounds"
"-Wstrict-aliasing=3 -Wstrict-overflow=1 -Wstrict-prototypes -Winline"
"-Wshadow -Wswitch -Wmissing-include-dirs -Woverlength-strings -Wpacked"
"-Wdisabled-optimization  -Wformat=2 -Winit-self"
"-Wunused-parameter -Wlogical-op -Wuninitialized"
"-Wnested-externs -Wpointer-arith -Wdouble-promotion -Wunused-macros"
"-Wunsafe-loop-optimizations -rdynamic ")
# -fdiagnostics-color")



STRING(REGEX REPLACE ";" " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")


find_package( PkgConfig REQUIRED)


pkg_check_modules ( GLIB REQUIRED glib-2.0 )
include_directories(${GLIB_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GLIB_LIBRARIES})

#pkg_check_modules ( GDK REQUIRED gdk-3.0 )
#include_directories(${GDK_INCLUDE_DIRS})
#set(LIBS ${LIBS} ${GDK_LIBRARIES})

# This gets V3 as well !
pkg_check_modules ( GLES REQUIRED glesv2 )
include_directories(${GLES_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GLES_LIBRARIES})

pkg_check_modules ( EGL REQUIRED egl )
include_directories(${EGL_INCLUDE_DIRS})
set(LIBS ${LIBS} ${EGL_LIBRARIES})


pkg_check_modules ( PNG REQUIRED libpng16 )
include_directories(${PNG_INCLUDE_DIRS})
set(LIBS ${LIBS} ${PNG_LIBRARIES})


pkg_check_modules ( GTK REQUIRED cglm )
include_directories(${GTK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GTK_LIBRARIES})

pkg_check_modules ( CGLM REQUIRED gtk+-3.0 )
include_directories(${CGLM_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CGLM_LIBRARIES})

pkg_check_modules ( ALSA REQUIRED alsa )
include_directories(${ALSA_INCLUDE_DIRS})
set(LIBS ${LIBS} ${ALSA_LIBRARIES})



target_link_libraries(803 ${LIBS} iberty m )


install(TARGETS 803 DESTINATION /usr/local/bin)
install(DIRECTORY 803-Resources DESTINATION /usr/local/share/ )

# THis is used to detect if the emulator is being run from witin the
# build tree and then to choose where to get resources from .
configure_file(
    ${CMAKE_SOURCE_DIR}/config.h.in
    ${CMAKE_BINARY_DIR}/config.h
)
