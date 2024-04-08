CC = clang
CFLAGS = -Wall -Wconversion --std=gnu2x
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2

ifeq ($(MODE),release)
	CFLAGS += $(RELEASE_FLAGS)
else
	CFLAGS += $(DEBUG_FLAGS)
endif

all: bin/main.o bin/garbagec

clean:
	rm -rf bin/*

bin/main.o: src/main.c src/common.h src/debug_utils.h
	$(CC) $(CFLAGS) -c src/main.c -o bin/main.o

bin/garbagec: bin/main.o
	$(CC) $(CFLAGS) bin/*.o -o bin/garbagec
