#
# When building a package or installing otherwise in the system, make
# sure that the variable PREFIX is defined, e.g. make PREFIX=/usr/local
#
PROGNAME=beastblackbox

ifndef DUMP1090_VERSION
DUMP1090_VERSION=$(shell git describe --always --tags --match=v*)
endif

ifdef PREFIX
BINDIR=$(PREFIX)/bin
SHAREDIR=$(PREFIX)/share/$(PROGNAME)
EXTRACFLAGS=-DHTMLPATH=\"$(SHAREDIR)\"
endif

CPPFLAGS+=-DMODES_DUMP1090_VERSION=\"$(DUMP1090_VERSION)\"
CFLAGS+=-O2 -g -Wall -Werror -W -D_FILE_OFFSET_BITS=64
LIBS=-lpthread -lm
LIBS_LOW=-lm
LIBS_RTL=`pkg-config --libs librtlsdr libusb-1.0`
CC=gcc

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
LIBS+=-lrt
CFLAGS+=-std=c11 -D_DEFAULT_SOURCE -s -flto
endif
ifeq ($(UNAME), Darwin)
UNAME_R := $(shell uname -r)
ifeq ($(shell expr "$(UNAME_R)" : '1[012345]\.'),3)
CFLAGS+=-std=c11 -DMISSING_GETTIME -DMISSING_NANOSLEEP
COMPAT+=compat/clock_gettime/clock_gettime.o compat/clock_nanosleep/clock_nanosleep.o
else
# Darwin 16 (OS X 10.12) supplies clock_gettime() and clockid_t
CFLAGS+=-std=c11 -DMISSING_NANOSLEEP -DCLOCKID_T
COMPAT+=compat/clock_nanosleep/clock_nanosleep.o
endif
endif

ifeq ($(UNAME), OpenBSD)
CFLAGS+= -DMISSING_NANOSLEEP
COMPAT+= compat/clock_nanosleep/clock_nanosleep.o
endif

all: beastblackbox

%.o: %.c *.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRACFLAGS) -c $< -o $@

beastblackbox: beastblackbox.o mode_ac.o mode_s.o crc.o cpr.o icao_filter.o track.o util.o kmlexport.o $(COMPAT)
	$(CC) -g -o $@ $^ $(LIBS_LOW) $(LDFLAGS)

clean:
	rm -f *.o compat/clock_gettime/*.o compat/clock_nanosleep/*.o beastblackbox

test: cprtests
	./cprtests

cprtests: cpr.o cprtests.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRACFLAGS) -g -o $@ $^ -lm

crctests: crc.c crc.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRACFLAGS) -g -DCRCDEBUG -o $@ $<
