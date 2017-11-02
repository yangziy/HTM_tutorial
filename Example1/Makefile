CFLAGS   = -g -O0 -pthread
DEFINES  =
INCLUDES =
LIBS     = -pthread
CC       = gcc

# enables TSX rtm
CFLAGS += -mrtm
LIBS   += -mrtm

CFLAGS += $(DEFINES) $(INCLUDES)

all: main

main: main.o # the make file uses the default CFLAGS to build this
	$(CC) $(LIBS) -o $@ $<

%.c: %.o
