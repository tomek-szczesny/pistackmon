// -------------------------------------------------------------------------
// GPIO-specific functions
//
// gpio_C2.h: header file for Odroid C2
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#ifndef _GPIO_C2_H
#define _GPIO_C2_H

#include <cstdint>

#define PIN_DATA   19
#define PIN_CLK    11
#define PIN_LATCH   9
#define PIN_BLANK   3

extern volatile uint32_t * gpiomap;

void gpioInitImpl();
void gpioDeinitImpl();

inline void gpioSet(uint8_t pin) {
	*(gpiomap+119) |= (1 << pin); // Set pin high
}

inline void gpioClear(uint8_t pin) {
	*(gpiomap+119) &= ~(1 << pin); // Set pin low
}

#endif
