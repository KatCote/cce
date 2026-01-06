CC = clang

SRCS = \
	src/engine/window/window.c \
	src/engine/init/init.c \
	src/engine/render/render.c \
	src/engine/text/text.c \
	src/engine/timer/timer.c \

INCLUDES = \
	-Isrc \
	-Isrc/exteral \
	-Isrc/engine \
	-Isrc/engine/window \
	-Isrc/engine/init \
	-Isrc/engine/render \
	-Isrc/engine/text \
	-Isrc/engine/timer \

CFLAGS = -std=c23 -Wall -Wextra -fPIC

LIBS = -lglfw -lGL -lm

TARGET = libcce.so

BUILD_DIR = build

PUBLIC_HEADERS = src/cce.h

all: $(BUILD_DIR)/$(TARGET) install_headers

$(BUILD_DIR)/$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -shared -o $@ $(LIBS)

install_headers: | $(BUILD_DIR)/include
	cp $(PUBLIC_HEADERS) $(BUILD_DIR)/include/

test-window: all
	$(MAKE) -C examples test-window

test-pixel-grid: all
	$(MAKE) -C examples test-pixel-grid

test-moving-grid: all
	$(MAKE) -C examples test-moving-grid

test-text: all
	$(MAKE) -C examples test-text

test-chunk: all
	$(MAKE) -C examples test-chunk

test: all
	$(MAKE) -C examples test-all

deploy:
	sudo cp $(BUILD_DIR)/include/cce.h /usr/include
	sudo cp $(BUILD_DIR)/libcce.so /usr/lib

cleanup:
	sudo rm /usr/include/cce.h
	sudo rm /usr/lib/libcce.so

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/include: | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/include

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C examples clean

.PHONY: all clean install_headers test-all