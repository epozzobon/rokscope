CC=gcc
PKG_CONFIG_LIBS=glib-2.0 gio-unix-2.0 libsigrok glfw3
PKG_CONFIG_CFLAGS=glfw3 glew
PKG_CONFIG=$(shell pkg-config --cflags $(PKG_CONFIG_CFLAGS) --libs $(PKG_CONFIG_LIBS))
CFLAGS=-g -Wall -Wextra $(PKG_CONFIG)

all: build/rokscope

build/rokscope: build rokscope.c gloscope.c console.c
	$(CC) $(CFLAGS) rokscope.c gloscope.c console.c -o build/rokscope

build:
	mkdir build


