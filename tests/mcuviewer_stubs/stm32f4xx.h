#pragma once
#include <stdint.h>
typedef struct { uint32_t CYCCNT; } TestDwt;
extern TestDwt test_dwt;
#define DWT (&test_dwt)
typedef struct { uint32_t ODR,MODER,PUPDR,IDR; } TestGpio;
typedef struct { uint32_t BDTR; } TestTim;
extern TestGpio test_gpiob,test_gpioc;
extern TestTim test_tim1;
#define GPIOB (&test_gpiob)
#define GPIOC (&test_gpioc)
#define TIM1 (&test_tim1)
#define TIM_BDTR_MOE (1u<<15)
static inline uint32_t __get_PRIMASK(void) {return 0;}
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t mask) {(void)mask;}
static inline void __DMB(void) {}
