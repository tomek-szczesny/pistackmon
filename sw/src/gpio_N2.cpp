#include <sys/mman.h>	// mmap()

#include "gpio_N2.h"

// Map of a part of memory that provides access to GPIO registers
volatile uint32_t *gpiomap;

void gpioInitImpl() {
	int gpiomem = open("/dev/mem", O_RDWR|O_SYNC);
	void * map = mmap(NULL, 4096, (PROT_READ | PROT_WRITE), MAP_SHARED, gpiomem, 0xFF634000);
	gpiomap = reinterpret_cast<volatile uint32_t *> (map);
	close(gpiomem);

	// Set pins as outputs
	*(gpiomap+0x116) &= ~(1<<3);		// Pin X.3
	*(gpiomap+0x116) &= ~(1<<4);		// Pin X.4
	*(gpiomap+0x116) &= ~(1<<7);		// Pin X.7
	*(gpiomap+0x116) &= ~(1<<2);		// Pin X.2
	__sync_synchronize();
}

void gpioDeinitImpl() {
	*(gpiomap+0x116) |= (1<<3);		// Pin X.3
	*(gpiomap+0x116) |= (1<<4);		// Pin X.4
	*(gpiomap+0x116) |= (1<<7);		// Pin X.7
	*(gpiomap+0x116) |= (1<<2);		// Pin X.2
	__sync_synchronize();
}
