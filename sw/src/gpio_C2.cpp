// -------------------------------------------------------------------------
// GPIO-specific functions
//
// gpio_C2.cpp: implementation file for Odroid C2
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#include <cstdint>      // uint32_t
#include <fcntl.h>      // open
#include <unistd.h>     // close
#include <sys/mman.h>	// mmap()

#include "gpio_C2.h"

// Map of a part of memory that provides access to GPIO registers
volatile uint32_t *gpiomap;

void gpioInitImpl() {
	int gpiomem = open("/dev/mem", O_RDWR|O_SYNC);
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xC8834000);
	gpiomap = reinterpret_cast<volatile uint32_t *> (map);
	close(gpiomem);

	// Set pins as outputs
	*(gpiomap+0x118) &= ~(1<<19);		// Pin X.19
	*(gpiomap+0x118) &= ~(1<<11);		// Pin X.11
	*(gpiomap+0x118) &= ~(1<<9);		// Pin X.9
	*(gpiomap+0x118) &= ~(1<<3);		// Pin X.3
	__sync_synchronize();
}

void gpioDeinitImpl() {
	*(gpiomap+0x118) |= (1<<(19));		// Pin X.19
	*(gpiomap+0x118) |= (1<<(11));		// Pin X.11
	*(gpiomap+0x118) |= (1<<(9));		// Pin X.9
	*(gpiomap+0x118) |= (1<<(3));		// Pin X.9
	__sync_synchronize();
}
