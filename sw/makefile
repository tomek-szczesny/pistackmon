# -------------------------------------------------------------------------
# Toplevel makefile for pistackmon
#
# Website: https://github.com/tomek-szczesny/pistackmon
# Authors: Tomek Szczesny, Bernhard Bablok
# License: GPL3
# -------------------------------------------------------------------------

DEBUG=off

SHELL=/bin/bash
PREFIX=/usr/local
EXECS=pistackmond

SERVICE=pistackmond.service
SERVICEPATH=/etc/systemd/system

none:
	@echo ""
	@echo "Pick one of the options:"
	@echo "make rpi3           - builds a Raspberry Pi 3 version of pistackmond"
	@echo "make rpi4           - builds a Raspberry Pi 4 version of pistackmond"
	@echo "make c1             - builds a Odroid C1(+) version of pistackmond"
	@echo "make c2             - builds a Odroid C2 version of pistackmond"
	@echo "make m1             - builds a Odroid M1 version of pistackmond"
	@echo "make n2             - builds a Odroid N2(+)(L) version of pistackmond"
	@echo "make clean          - cleans build environment"
	@echo "sudo make install   - installs pistackmond (you need to build it first!)"
	@echo "sudo make uninstall - removes pistackmond"

rpi3:
	${MAKE} -C src $(EXECS) PLATFORM=PI3

rpi4:
	${MAKE} -C src $(EXECS) PLATFORM=PI4

n2:
	${MAKE} -C src $(EXECS) PLATFORM=N2

c1:
	${MAKE} -C src $(EXECS) PLATFORM=C1

c2:
	${MAKE} -C src $(EXECS) PLATFORM=C2

m1:
	${MAKE} -C src $(EXECS) PLATFORM=M1

clean:
	${MAKE} -C src clean
	rm -f ${SERVICE}

${SERVICE}: ${SERVICE}.in
	sed -e "s,PREFIX,${PREFIX}," $< > $@

install: ${SERVICE}
	${MAKE} -C src install
	install -p -m 0664  ${SERVICE} ${SERVICEPATH}
	echo -e "#default brightness-factor\nBRIGHTNESS=1.0" > /etc/default/pistackmond
	systemctl daemon-reload
	-systemctl enable pistackmond
	-systemctl start pistackmond

uninstall:
	systemctl stop pistackmond
	systemctl disable pistackmond
	rm -f ${SERVICEPATH}/${SERVICE}
	systemctl daemon-reload
	systemctl reset-failed
	${MAKE} -C src uninstall
