// -------------------------------------------------------------------------
// GPIO-specific functions
//
// gpio_N2.h: header file for Odroid N2
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#ifndef _GPIO_N2_H
#define _GPIO_N2_H

#include <cstdint>

#define PIN_DATA    3
#define PIN_CLK     4
#define PIN_LATCH   7
#define PIN_BLANK   2

extern volatile uint32_t * gpiomap;

void gpioInitImpl();
void gpioDeinitImpl();

inline void gpioSet(uint8_t pin) {
	*(gpiomap+117) |= (1 << pin); // Set pin high
}

inline void gpioClear(uint8_t pin) {
	*(gpiomap+117) &= ~(1 << pin); // Set pin low
}

#endif
