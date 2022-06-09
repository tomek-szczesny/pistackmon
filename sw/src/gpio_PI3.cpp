// -------------------------------------------------------------------------
// GPIO-specific functions
//
// gpio_PI3.cpp: implementation file for PI3 (and PI4)
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#include <cstdint>      // uint32_t
#include <fcntl.h>      // open
#include <unistd.h>     // close
#include <sys/mman.h>	// mmap()

#include "gpio_PI3.h"

// Map of a part of memory that provides access to GPIO registers
volatile uint32_t *gpiomap;

void gpioInitImpl() {
	int gpiomem = open("/dev/mem", O_RDWR|O_SYNC);
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, REG_GPIOMAP);
	gpiomap = reinterpret_cast<volatile uint32_t *> (map);
	close(gpiomem);

	// Set pins as inputs (reset 3-bit pin mode to 000)
	*(gpiomap+1) &= ~(7<<(7*3));		// Pin 17
	*(gpiomap+2) &= ~(7<<(2*3));		// Pin 22
	*(gpiomap+2) &= ~(7<<(5*3));	        // Pin 25
	*(gpiomap+2) &= ~(7<<(7*3));		// Pin 27

	// Set pins as outputs (sets 3-bit pin mode to 001)
	*(gpiomap+1) |=  (1<<(7*3));		// Pin 17
	*(gpiomap+2) |=  (1<<(2*3));		// Pin 22
	*(gpiomap+2) |=  (1<<(5*3));    	// Pin 25
	*(gpiomap+2) |=  (1<<(7*3));		// Pin 27
	__sync_synchronize();
}

void gpioDeinitImpl() {
	// Set pins as inputs (default state)
	*(gpiomap+1) &= ~(7<<(7*3));	// Pin 17
	*(gpiomap+2) &= ~(7<<(2*3));	// Pin 22
	*(gpiomap+2) &= ~(7<<(5*3));	// Pin 25
	*(gpiomap+2) &= ~(7<<(7*3));	// Pin 27
	__sync_synchronize();
}
