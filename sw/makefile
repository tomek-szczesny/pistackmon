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
	@echo "make clean          - cleans build environment"
	@echo "sudo make install   - installs pistackmond (you need to build it first!)"
	@echo "sudo make uninstall - removes pistackmond"

rpi3: src/pistackmond.cpp
	${GCC} ${LIBS} ${GCCFLAGS} -D'RASPBERRY_PI3' -o ${EXECS} ${SRC} ${LDLIBS} 

rpi4: src/pistackmond.cpp
	${GCC} ${LIBS} ${GCCFLAGS} -D'RASPBERRY_PI4' -o ${EXECS} ${SRC} ${LDLIBS} 

clean:
	rm ${EXECS}

install:
	cp ${EXECS} /usr/local/sbin
	chown root:root /usr/local/sbin/${EXECS}
	cp ${SERVICE} ${SERVICEPATH}
	chown root:root ${SERVICEPATH}/${SERVICE}
	chmod 644 ${SERVICEPATH}/${SERVICE}
	systemctl daemon-reload
	systemctl enable pistackmond
	systemctl start pistackmond

uninstall:
	systemctl stop pistakmond
	systemctl disable pistackmond
	rm /usr/local/sbin/${EXECS} 
