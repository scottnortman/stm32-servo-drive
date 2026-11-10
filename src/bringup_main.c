// Board-only NUCLEO-F411RE smoke test.
// This initializes no motor-control peripherals.

#include "stm32f4xx.h"
#include "clock.h"

#include <stdint.h>

#define HEARTBEAT_HZ 10u

static void uart2_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;

    // PA2 = USART2_TX, connected to the Nucleo ST-LINK virtual COM port.
    GPIOA->MODER &= ~(3u << (2u * 2u));
    GPIOA->MODER |=  (2u << (2u * 2u));
    GPIOA->AFR[0] &= ~(0xFu << (2u * 4u));
    GPIOA->AFR[0] |=  (7u << (2u * 4u));

    // APB1 peripheral clock is 50 MHz. 0x1B2 gives approximately 115200 baud
    // with 16x oversampling (actual rate approximately 115207 baud).
    USART2->BRR = 0x01B2u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart2_putc(char c)
{
    while ((USART2->SR & USART_SR_TXE) == 0u) {}
    USART2->DR = (uint8_t)c;
}

static void uart2_puts(const char *text)
{
    while (*text != '\0')
    {
        uart2_putc(*text++);
    }
}

static void uart2_put_uint64(uint64_t value)
{
    char digits[20];
    uint32_t count = 0u;

    do
    {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);

    while (count != 0u)
    {
        uart2_putc(digits[--count]);
    }
}

int main(void)
{
    uint64_t heartbeat_count = 0u;

    clock_init();
    uart2_init();

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    // NUCLEO-F411RE LD2 is connected to PA5 and is active high.
    GPIOA->MODER &= ~(3u << (5u * 2u));
    GPIOA->MODER |=  (1u << (5u * 2u));
    GPIOA->OTYPER &= ~(1u << 5u);
    GPIOA->PUPDR  &= ~(3u << (5u * 2u));

    uart2_puts("\r\nSTM32F411RE bring-up: clock=100MHz uart=115200 heartbeat=10Hz\r\n");

    // Poll the hardware timer at 100 ms intervals. TICKINT stays clear;
    // this target does not need an interrupt handler or a calibrated busy loop.
    SysTick->CTRL = 0u;
    SysTick->LOAD = (SystemCoreClock / HEARTBEAT_HZ) - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (1)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0u) {}

        GPIOA->ODR ^= (1u << 5u);
        uart2_puts("heartbeat ");
        uart2_put_uint64(++heartbeat_count);
        uart2_puts("\r\n");
    }
}
