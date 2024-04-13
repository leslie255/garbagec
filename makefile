CC = clang
CFLAGS = -Wall -Wconversion --std=gnu2x
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2

ifeq ($(MODE),release)
	CFLAGS += $(RELEASE_FLAGS)
else
	CFLAGS += $(DEBUG_FLAGS)
endif

all: bin/main.o bin/gc.o bin/garbagec

clean:
	rm -rf bin/*

bin/gc.o: src/gc.c src/gc.h src/common.h src/debug_utils.h
	$(CC) $(CFLAGS) -c src/gc.c -o bin/gc.o

bin/main.o: src/main.c src/common.h src/debug_utils.h
	$(CC) $(CFLAGS) -c src/main.c -o bin/main.o

bin/garbagec: bin/main.o bin/gc.o
	$(CC) $(CFLAGS) bin/main.o bin/gc.o -o bin/garbagec
