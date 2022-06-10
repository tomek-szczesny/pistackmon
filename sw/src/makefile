# -------------------------------------------------------------------------
# Makefile for pistackmond (called by toplevel-makefile)
#
# Website: https://github.com/tomek-szczesny/pistackmon
# Authors: Tomek Szczesny, Bernhard Bablok
# License: GPL3
# -------------------------------------------------------------------------

DEBUG=off
GCCFLAGS_off=-w -O3
GCCFLAGS_on=-Wall -Wextra -g -Og
GCCFLAGS=-fcompare-debug-second -std=gnu++17 ${GCCFLAGS_${DEBUG}}

SHELL=/bin/bash
PREFIX=/usr/local
EXECS=pistackmond
GCC?=g++
SRC=pistackmond.cpp gpio_${PLATFORM}.cpp
LIBS=-pthread
LDLIBS=

include platform.mak

platform.mak:
	echo "PLATFORM=${PLATFORM}" > platform.mak

${EXECS}: ${SRC} gpio.h
	${GCC} ${LIBS} ${GCCFLAGS} -D${PLATFORM} -o ${EXECS} ${SRC} ${LDLIBS}

gpio.h: gpio_${PLATFORM}.h
	rm -f $@
	ln -s $<  $@

clean:
	rm -f ${EXECS} gpio.h platform.mak

install: ${EXECS}
	install -p -s ${EXECS} ${PREFIX}/sbin

uninstall:
	rm -f ${PREFIX}/sbin/${EXECS}
