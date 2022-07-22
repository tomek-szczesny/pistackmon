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

LED_G = 0.35
LED_Y = 1.0
LED_R = 1.0
LED_B = 1.0
LED_GAMMA = 3.75
LED =-DLED_G=${LED_G} -DLED_Y=${LED_Y} -DLED_R=${LED_R} -DLED_B=${LED_B} -DLED_GAMMA=${LED_GAMMA}

SHELL=/bin/bash
PREFIX=/usr/local
EXECS=pistackmond
GCC?=g++
SRC=pistackmond.cpp gpio_${PLATFORM}.cpp
LIBS=-pthread
LDLIBS=-lrt

include platform.mak

platform.mak:
	echo "PLATFORM=${PLATFORM}" > platform.mak

${EXECS}: ${SRC} gpio.h
	${GCC} ${LIBS} ${GCCFLAGS} -D${PLATFORM} ${LED} -o ${EXECS} ${SRC} ${LDLIBS}

gpio.h: gpio_${PLATFORM}.h
	rm -f $@
	ln -s $<  $@

clean:
	rm -f ${EXECS} gpio.h platform.mak

install: ${EXECS}
	install -p -s ${EXECS} ${PREFIX}/sbin

uninstall:
	rm -f ${PREFIX}/sbin/${EXECS}
