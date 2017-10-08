cmake_minimum_required(VERSION 3.0)
project(rokscope)

set(CMAKE_C_STANDARD 11)

# Include OpenGL stuff for rendering
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})
link_libraries(${OPENGL_gl_LIBRARY})

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})
link_libraries(${GLEW_LIBRARIES})

# Include PkgConfig for GLIB and Sigrok
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 REQUIRED glib-2.0 gio-unix-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_libraries(${GLIB2_LIBRARIES})

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
link_libraries(${GTK3_LIBRARIES})

pkg_check_modules(SIGROK REQUIRED libsigrok)
include_directories($${SIGROK_INCLUDE_DIRS})
link_libraries(${SIGROK_LIBRARIES})

# Set rource files
set(SOURCE_FILES
        console.c
        gloscope.c
        gloscope.h
        rokscope.c
        rokscope.h
        gui_window.c)

add_executable(rokscope ${SOURCE_FILES})
