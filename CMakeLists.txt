cmake_minimum_required(VERSION 3.0)
project(rokscope)

set(CMAKE_C_STANDARD 11)

# Include OpenGL stuff for rendering
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})
link_libraries(${OPENGL_gl_LIBRARY})

find_package(glfw3 3.2 REQUIRED)
link_libraries(glfw)

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})
link_libraries(${GLEW_LIBRARIES})

# Include PkgConfig for GLIB and Sigrok
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 REQUIRED glib-2.0 gio-unix-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_libraries(${GLIB2_LIBRARIES})

pkg_check_modules(SIGROK REQUIRED libsigrok)
include_directories($${SIGROK_INCLUDE_DIRS})
link_libraries(${SIGROK_LIBRARIES})

# Set rource files
set(SOURCE_FILES
        console.c
        gloscope.c
        gloscope.h
        rokscope.c
        rokscope.h)

add_executable(rokscope ${SOURCE_FILES})
