SHELL=/bin/bash
PREFIX=/usr/local
EXECS=pistackmond
GCC?=g++
SRC=src/pistackmond.cpp
LIBS=-pthread
LDLIBS=
GCCFLAGS=-Wall -Wextra -g -std=gnu++17 -O3
SERVICE=pistackmond.service
SERVICEPATH=/etc/systemd/system

none:
	@echo ""
	@echo "Pick one of the options:"
	@echo "make rpi3           - builds a Raspberry Pi 3 version of pistackmond"
	@echo "make rpi4           - builds a Raspberry Pi 4 version of pistackmond"
	@echo "make n2             - builds a Odroid N2 version of pistackmond"
	@echo "make c1             - builds a Odroid C1(+) version of pistackmond"
	@echo "make clean          - cleans build environment"
	@echo "sudo make install   - installs pistackmond (you need to build it first!)"
	@echo "sudo make uninstall - removes pistackmond"

rpi3:
	${MAKE} $(EXECS) PLATFORM=RASPBERRY_PI3

rpi4:
	${MAKE} $(EXECS) PLATFORM=RASPBERRY_PI4

n2:
	${MAKE} $(EXECS) PLATFORM=ODROID_N2

c1:
	${MAKE} $(EXECS) PLATFORM=ODROID_C1

${EXECS}: src/pistackmond.cpp
	${GCC} ${LIBS} ${GCCFLAGS} -D=${PLATFORM} -o ${EXECS} ${SRC} ${LDLIBS}

clean:
	rm -f ${EXECS} ${SERVICE}

${SERVICE}: ${SERVICE}.in
	sed -e "s,PREFIX,${PREFIX}," $< > $@

install: ${EXECS} ${SERVICE}
	install -p -s ${EXECS} ${PREFIX}/sbin
	install -p -m 0664  ${SERVICE} ${SERVICEPATH}
	systemctl daemon-reload
	-systemctl enable pistackmond
	-systemctl start pistackmond

uninstall:
	systemctl stop pistackmond
	systemctl disable pistackmond
	rm -f ${SERVICEPATH}/${SERVICE}
	systemctl daemon-reload
	systemctl reset-failed
	rm -f ${PREFIX}/sbin/${EXECS}
