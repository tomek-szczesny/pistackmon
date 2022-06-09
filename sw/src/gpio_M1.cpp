// -------------------------------------------------------------------------
// GPIO-specific functions
//
// gpio_M1.cpp: implementation file for Odroid M1
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#include <cstdint>      // uint32_t
#include <fcntl.h>      // open
#include <unistd.h>     // close
#include <sys/mman.h>	// mmap()

#include "gpio_M1.h"


// Map of a part of memory that provides access to GPIO registers
volatile uint32_t *gpiomap;
volatile uint32_t *gpiomap3; // Extra map for reaching GPIO bank 3 in Odroid M1


void gpioInitImpl() {
	int gpiomem = open("/dev/mem", O_RDWR|O_SYNC);
        void *map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xFE760000);
        gpiomap3 = reinterpret_cast<volatile uint32_t *> (map);
        map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xFDD60000);
	gpiomap = reinterpret_cast<volatile uint32_t *> (map);
	close(gpiomem);

	rk_gpio(0, 0x02 + 0x01,  0, 1);         // Pin 0C.0
	rk_gpio(0, 0x02 + 0x01,  1, 1);         // Pin 0C.1
	rk_gpio(1, 0x02       , 10, 1);         // Pin 3B.2
	rk_gpio(1, 0x02 + 0x01,  9, 1);         // Pin 3D.1
	__sync_synchronize();
}

void gpioDeinitImpl() {
	rk_gpio(0, 0x02 + 0x01,  0, 0);         // Pin 0C.0
	rk_gpio(0, 0x02 + 0x01,  1, 0);         // Pin 0C.1
	rk_gpio(1, 0x02       , 10, 0);         // Pin 3B.2
	rk_gpio(1, 0x02 + 0x01,  9, 0);         // Pin 3D.1
	__sync_synchronize();
}

// A special function for accessing RockChip's GPIOs - "it's complicated"...
// It requires simultaneous write to two bits for whatever reason.
// Perhaps future release of full datasheet would explain this.

void rk_gpio(bool g3, int offset, int bit, bool val) {
	volatile uint32_t * gm = g3 ? gpiomap3 : gpiomap;
	uint32_t buf = *(gm + offset);
	buf |= (1 << (bit + 16));		// <- this thing - what is that?
	if (val) buf |=  (1 << bit);
	else     buf &= ~(1 << bit);
	*(gm + offset) = buf;
}

