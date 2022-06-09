// -------------------------------------------------------------------------
// GPIO-specific functions
//
// gpio_C1.cpp: implementation file for Odroid C1
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#include <cstdint>      // uint32_t
#include <fcntl.h>      // open
#include <unistd.h>     // close
#include <sys/mman.h>	// mmap()

#include "gpio_C1.h"

// Map of a part of memory that provides access to GPIO registers
volatile uint32_t *gpiomap;

void gpioInitImpl() {
	int gpiomem = open("/dev/mem", O_RDWR|O_SYNC);
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xC1108000);
	gpiomap = reinterpret_cast<volatile uint32_t *> (map);
	close(gpiomem);

	// Set pins as outputs
	*(gpiomap+0x0F) &= ~(1<<8);		// Pin Y.8
	*(gpiomap+0x0C) &= ~(1<<19);		// Pin X.19
	*(gpiomap+0x0C) &= ~(1<<18);		// Pin X.18
	*(gpiomap+0x0C) &= ~(1<<6);	        // Pin X.6
	__sync_synchronize();
}

void gpioDeinitImpl() {
	// Set pins as inputs (default state)
	*(gpiomap+0x0F) |= (1<<(8));		// Pin Y.8
	*(gpiomap+0x0C) |= (1<<(19));		// Pin X.19
	*(gpiomap+0x0C) |= (1<<(18));		// Pin X.18
	*(gpiomap+0x0C) |= (1<<(6));	        // Pin X.6
	__sync_synchronize();
}
