// -------------------------------------------------------------------------
// GPIO-specific functions
//
// gpio_PI3.h: header file for PI3 (and PI4)
//
// Website: https://github.com/tomek-szczesny/pistackmon
// Authors: Tomek Szczesny, Bernhard Bablok
// License: GPL3
// -------------------------------------------------------------------------

#ifndef _GPIO_PI3_H
#define _GPIO_PI3_H

#include <cstdint>

#define PIN_DATA  17
#define PIN_CLK   27
#define PIN_LATCH 22
#define PIN_BLANK 25

#if defined(PI3)
  #define REG_GPIOMAP 0x3F200000
#elif defined(PI4)
  #define REG_GPIOMAP 0xFE200000
#endif

extern volatile uint32_t *gpiomap;

void gpioInitImpl();
void gpioDeinitImpl();

inline void gpioSet(uint8_t pin) {
	*(gpiomap+7) = (1 << pin);		// Set pin high
}

inline void gpioClear(uint8_t pin) {
	*(gpiomap+10) = (1 << pin);		// Set pin low
}

#endif
