# vt - video tracking
# See LICENSE file for copyright and license details.

# include config.mk

INC:=$(shell pkg-config --cflags libavdevice libavformat libavcodec libswresample libswscale libavutil sdl2)
LDFLAGS:=$(shell pkg-config --libs libavdevice libavformat libavcodec libswresample libswscale libavutil sdl2) -lm -lpthread
CFLAGS = -Wall
SRC = vb.c avCtx.c sdlCtx.c show.c
OBJ = ${SRC:.c=.o}
HEADER = avCtx.h sdlCtx.h show.h

CC = clang

all: vb

format: .clang-format
	clang-format -i ${SRC}

%.o: %.c ${HEADER}
	${CC} -c -ggdb -O0 ${CFLAGS} $<

vb: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f vb ${OBJ}

.PHONY: all clean format
