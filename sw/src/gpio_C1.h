// -------------------------------------------------------------------------
// GPIO-specific functions
//
// gpio_C1.h: header file for Odroid C1
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#ifndef _GPIO_C1_H
#define _GPIO_C1_H

#include <cstdint>

#define PIN_DATA   88
#define PIN_CLK   116
#define PIN_LATCH 115
#define PIN_BLANK 103

#define OFF_X   0x0D
#define OFF_Y   0x10
#define START_X   97
#define START_Y   80

extern volatile uint32_t * gpiomap;

void gpioInitImpl();
void gpioDeinitImpl();

inline void gpioSet(uint8_t pin) {
	if (pin > START_X) {
		*(gpiomap+OFF_X) |= (1 << (pin-START_X)); // Set pin high
	} else {
		*(gpiomap+OFF_Y) |= (1 << (pin-START_Y)); // Set pin high
	}
}

inline void gpioClear(uint8_t pin) {
	if (pin > START_X) {
		*(gpiomap+OFF_X) &= ~(1 << (pin-START_X)); // Set pin low
	} else {
		*(gpiomap+OFF_Y) &= ~(1 << (pin-START_Y)); // Set pin low
  }
}

#endif
