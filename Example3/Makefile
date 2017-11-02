CFLAGS   = -g -O0 -pthread
DEFINES  =
INCLUDES = -I ../common
LIBS     = -pthread
CC       = gcc

TM_STATISTICS ?= 1

# enables TSX rtm
CFLAGS += -mrtm
LIBS   += -mrtm

DEFINES += -DTM_STATISTICS=$(TM_STATISTICS)

CFLAGS += $(DEFINES) $(INCLUDES)

all: main

main: main.o # the make file uses the default CFLAGS to build this
	$(CC) $(LIBS) -o $@ $<

%.c: %.o
