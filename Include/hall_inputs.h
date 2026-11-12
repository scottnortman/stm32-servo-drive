#pragma once
#include "stm32f4xx.h"
#include <stdint.h>

enum { HALL_GPIO_MASK=(3u<<12)|(3u<<14)|(3u<<16),
       HALL_GPIO_PULLUPS=(1u<<12)|(1u<<14)|(1u<<16) };

// Caller enables GPIOB clock. Open-collector outputs, pulled to MCU 3.3 V.
// Do not use with external pull-ups above VDD + 0.3 V.
static inline void hall_inputs_init(void)
{
    GPIOB->MODER &= ~HALL_GPIO_MASK;
    GPIOB->PUPDR = (GPIOB->PUPDR & ~HALL_GPIO_MASK) | HALL_GPIO_PULLUPS;
}

// One simultaneous snapshot: PB6=U/bit2, PB7=V/bit1, PB8=W/bit0.
static inline uint32_t hall_inputs_read(void)
{
    const uint32_t pins=GPIOB->IDR;
    return (((pins>>6)&1u)<<2) | (((pins>>7)&1u)<<1) | ((pins>>8)&1u);
}
