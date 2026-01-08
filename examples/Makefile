CC = clang

BUILD_DIR = ../build
INCLUDE_DIR = $(BUILD_DIR)/include
LIB_DIR = $(BUILD_DIR)

CFLAGS = -std=c23 -Wall -Wextra
LDFLAGS = -L$(LIB_DIR) -I$(INCLUDE_DIR) -lcce -Wl,-rpath,$(LIB_DIR) -lGL -lglfw -lm

test-window:
	$(CC) $@/main.c $(LDFLAGS) -o $@/$@.out
	$@/$@.out

test-moving-grid:
	$(CC) $@/main.c $(LDFLAGS) -o $@/$@.out
	$@/$@.out

test-chunk:
	$(CC) $@/main.c $(LDFLAGS) -o $@/$@.out
	$@/$@.out

test-sprite:
	$(CC) $@/main.c $(LDFLAGS) -o $@/$@.out
	$@/$@.out

test-shader:
	$(CC) $@/main.c $(LDFLAGS) -o $@/$@.out
	$@/$@.out

test-demo:
	$(CC) $@/main.c $(LDFLAGS) -o $@/$@.out
	$@/$@.out

test-all: test-window test-chunk test-moving-grid test-sprite test-shader test-demo

clean:
	rm -f test_window/test_*.out

.PHONY: clean test-all test-window test-moving-grid test-chunk test-sprite test-shader test-demo