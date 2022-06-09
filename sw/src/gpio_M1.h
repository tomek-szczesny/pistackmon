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

inline void gpioSet(uint8_t pin) {
  //	rk_gpio(1,0,PIN_OFF-pin,1); // Set pin high
}

inline void gpioClear(uint8_t pin) {
  //	rk_gpio(1,0,PIN_OFF-pin,0); // Set pin high
}

#endif
