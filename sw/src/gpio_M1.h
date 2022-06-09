// -------------------------------------------------------------------------
// GPIO-specific functions
//
// gpio_M1.h: header file for Odroid M1
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#ifndef _GPIO_M1_H
#define _GPIO_M1_H

#include <cstdint>

#define PIN_DATA   16
#define PIN_CLK    17
#define PIN_LATCH  106
#define PIN_BLANK  121

extern volatile uint32_t * gpiomap;

void gpioInitImpl();
void gpioDeinitImpl();
void rk_gpio(bool g3, int offset, int bit, bool val);

inline void gpioSet(uint8_t pin) {
	if (pin < 96) 	rk_gpio(0,  pin     / 16,  pin     % 16, 1);
	else 		rk_gpio(1, (pin-96) / 16, (pin-96) % 16, 1);
}

inline void gpioClear(uint8_t pin) {
	if (pin < 96) 	rk_gpio(0,  pin     / 16,  pin     % 16, 0);
	else 		rk_gpio(1, (pin-96) / 16, (pin-96) % 16, 0);
}

#endif
