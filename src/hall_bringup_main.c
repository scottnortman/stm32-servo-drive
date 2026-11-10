// Manual Hall observation only. No DRV driver, PWM, ADC or control code linked.
// PB0 ENABLE stays LOW throughout; motor power phases must stay disconnected.
// Hall electrical compatibility must be established before powering the sensors.
#include "clock.h"
#include "stm32f4xx.h"

#include <stdbool.h>
#include <stdint.h>

enum { HALL_FILTER_MS = 5, RUN_MS = 120000 };
static volatile uint32_t elapsed_ms;
static uint32_t changes, multibit_changes, invalid_entries;
static uint8_t seen;

void SysTick_Handler(void)
{
    ++elapsed_ms;
    GPIOB->BSRR = 1u << 16;  // Never enable the power stage, even while printing.
}

static void safe_gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                      RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;
    GPIOB->BSRR = 1u << 16;
    GPIOB->MODER = (GPIOB->MODER & ~3u) | 1u;
    GPIOB->OTYPER &= ~1u;
    GPIOB->PUPDR &= ~3u;
    const uint32_t low_pins = (1u << 5) | (1u << 8) | (1u << 9) | (1u << 10);
    const uint32_t low_modes = (3u << 10) | (3u << 16) | (3u << 18) | (3u << 20);
    GPIOA->BSRR = low_pins << 16;
    GPIOA->MODER = (GPIOA->MODER & ~low_modes) |
                   (1u << 10) | (1u << 16) | (1u << 18) | (1u << 20);
    GPIOA->OTYPER &= ~low_pins;
    GPIOA->PUPDR &= ~low_modes;
    GPIOC->BSRR = 1u << 8;  // Keep the existing DRV nCS deasserted.
    GPIOC->MODER = (GPIOC->MODER & ~(3u << 16)) | (1u << 16);
    GPIOC->OTYPER &= ~(1u << 8);
    GPIOC->PUPDR &= ~(3u << 16);
    // Existing analog wires stay high impedance.
    GPIOC->MODER |= (3u << 0) | (3u << 2) | (3u << 4);
    GPIOC->PUPDR &= ~((3u << 0) | (3u << 2) | (3u << 4));
    GPIOA->MODER |= 3u << 8;
    GPIOA->PUPDR &= ~(3u << 8);
    // PB6/PB7/PB8 use external EVM Hall pull-ups; no internal pulls.
    const uint32_t hall_modes = (3u << 12) | (3u << 14) | (3u << 16);
    GPIOB->MODER &= ~hall_modes;
    GPIOB->PUPDR &= ~hall_modes;
    // MB1136 C04: PC13 USER is released HIGH, pressed LOW.
    GPIOC->MODER &= ~(3u << 26);
    GPIOC->PUPDR &= ~(3u << 26);
}

static void uart_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 4)) | (2u << 4);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFu << 8)) | (7u << 8);
    USART2->BRR = 0x01B2u;  // APB1=50 MHz, 115200 baud.
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void putc_uart(char c)
{
    while ((USART2->SR & USART_SR_TXE) == 0u) {}
    USART2->DR = (uint8_t)c;
}

static void puts_uart(const char *text)
{
    while (*text != '\0') { putc_uart(*text++); }
}

static void put_uint(uint32_t value)
{
    char digits[10];
    unsigned int count = 0u;
    do
    {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    while (count != 0u) { putc_uart(digits[--count]); }
}

static void put_state(uint8_t state)
{
    putc_uart((char)('0' + ((state >> 2) & 1u)));
    putc_uart((char)('0' + ((state >> 1) & 1u)));
    putc_uart((char)('0' + (state & 1u)));
}

static uint8_t hall_read(void)
{
    const uint32_t pins = GPIOB->IDR;  // One snapshot for all three inputs.
    return (uint8_t)((((pins >> 6) & 1u) << 2) |
                     (((pins >> 7) & 1u) << 1) | ((pins >> 8) & 1u));
}

static bool invalid_120deg(uint8_t state)
{
    return state == 0u || state == 7u;
}

static void wait_button_level(bool pressed)
{
    uint32_t stable_since = elapsed_ms;
    while ((uint32_t)(elapsed_ms - stable_since) < 25u)
    {
        if (((GPIOC->IDR & (1u << 13)) == 0u) != pressed)
        {
            stable_since = elapsed_ms;
        }
    }
}

static void report(uint32_t run_ms, uint8_t state, const char *kind)
{
    puts_uart(kind); puts_uart(" t_ms="); put_uint(run_ms);
    puts_uart(" UVW="); put_state(state);
    puts_uart(" changes="); put_uint(changes);
    puts_uart(" multibit_or_missed="); put_uint(multibit_changes);
    puts_uart(" invalid_120deg_entries="); put_uint(invalid_entries);
    puts_uart(" states_seen=");
    for (uint8_t code = 0u; code < 8u; ++code)
    {
        if ((seen & (1u << code)) != 0u)
        {
            put_state(code); putc_uart(' ');
        }
    }
    puts_uart("\r\n");
}

int main(void)
{
    safe_gpio_init();  // Hold ENABLE low before any clock waits.
    clock_init();
    uart_init();
    SysTick->LOAD = SystemCoreClock / 1000u - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
    puts_uart("\r\nDiagnostic: manual Hall reader v1; ENABLE permanently LOW\r\n");
    puts_uart("No PWM, SPI writes or motor control. Keep motor phase leads disconnected.\r\n");
    puts_uart("Do not power Hall wiring until its supply/output ratings are verified.\r\n");
    puts_uart("U=PB6 CN10-17; V=PB7 CN7-21; W=PB8 CN10-3; no internal pull-ups.\r\n");
    puts_uart("UVW is U-bit2/V-bit1/W-bit0; 000/111 flagged for 120-degree Halls.\r\n");
    puts_uart("5ms stable-state filter: turn slowly by hand; counts may miss fast edges.\r\n");
    puts_uart("After approved Hall power/wiring, press/release USER to observe for 120s.\r\n");
    puts_uart("Press USER again to stop (20ms active-low debounce).\r\n");
    wait_button_level(false);
    wait_button_level(true);
    wait_button_level(false);

    const uint32_t start = elapsed_ms;
    uint32_t candidate_since = start, last_report = start, stop_since = start;
    uint8_t candidate = hall_read(), accepted = 0u;
    bool have_state = false, stop_pending = false;
    while ((uint32_t)(elapsed_ms - start) < RUN_MS)
    {
        const uint32_t now = elapsed_ms;
        if ((GPIOC->IDR & (1u << 13)) == 0u)
        {
            if (!stop_pending) { stop_pending = true; stop_since = now; }
            if ((uint32_t)(now - stop_since) >= 20u)
            {
                puts_uart("STOP: USER pressed\r\n");
                break;
            }
        }
        else { stop_pending = false; }
        const uint8_t raw = hall_read();
        if (raw != candidate)
        {
            candidate = raw;
            candidate_since = now;
        }
        if ((!have_state || candidate != accepted) &&
            (uint32_t)(now - candidate_since) >= HALL_FILTER_MS)
        {
            if (have_state)
            {
                ++changes;
                const uint8_t delta = candidate ^ accepted;
                if ((delta & (delta - 1u)) != 0u) { ++multibit_changes; }
            }
            if (invalid_120deg(candidate)) { ++invalid_entries; }
            seen |= (uint8_t)(1u << candidate);
            accepted = candidate;
            have_state = true;
            report(now - start, accepted, "STATE");
        }
        if ((uint32_t)(now - last_report) >= 1000u)
        {
            if (have_state) { report(now - start, accepted, "STATUS"); }
            else { puts_uart("STATUS: no Hall state stable for 5ms\r\n"); }
            last_report = now;
        }
    }
    if (have_state) { report(elapsed_ms - start, accepted, "FINAL"); }
    else { puts_uart("FINAL: no stable Hall state observed\r\n"); }
    puts_uart("Hall observation complete; ENABLE LOW. Reset to repeat.\r\n");
    while (1) { __WFI(); }
}
