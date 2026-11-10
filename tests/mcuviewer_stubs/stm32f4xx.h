#pragma once
#include <stdint.h>
typedef struct { uint32_t CYCCNT; } TestDwt;
extern TestDwt test_dwt;
#define DWT (&test_dwt)
static inline uint32_t __get_PRIMASK(void) {return 0;}
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t mask) {(void)mask;}
static inline void __DMB(void) {}
