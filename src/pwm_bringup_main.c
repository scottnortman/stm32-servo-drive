// Logic PWM scope test. Motor/Hall leads and INL high ties disconnected.
// EVM may be powered for input tracing only: PB0 ENABLE stays LOW throughout.
// PB0 ENABLE stays LOW. No DRV, ADC, Hall, encoder or motor-control code linked.
#include "clock.h"
#include "stm32f4xx.h"

#include <stdbool.h>
#include <stdint.h>

enum { PWM_ARR = 1250,
       PWM_PINS = (1u << 8) | (1u << 9) | (1u << 10),
       PWM_MODES = (3u << 16) | (3u << 18) | (3u << 20),
       PWM_OUTPUT_MODES = (1u << 16) | (1u << 18) | (1u << 20) };
static volatile uint32_t elapsed_ms, user_low_ms;
static volatile bool active;

static void outputs_low(void)
{
    // Works after TIM1 clock is enabled, before the SysTick interrupt is armed.
    GPIOB->BSRR = 1u << 16;
    TIM1->BDTR &= ~TIM_BDTR_MOE;
    TIM1->CCER = 0u;
    TIM1->CR1 &= ~TIM_CR1_CEN;
    GPIOA->BSRR = (uint32_t)PWM_PINS << 16;
    GPIOA->MODER = (GPIOA->MODER & ~(uint32_t)PWM_MODES) | PWM_OUTPUT_MODES;
    active = false;
}

void SysTick_Handler(void)
{
    ++elapsed_ms;
    GPIOB->BSRR = 1u << 16;  // ENABLE can never rise in this image.
    if (!active) { return; }
    if ((GPIOC->IDR & (1u << 13)) == 0u)
    {
        ++user_low_ms;
        if (user_low_ms >= 20u)
        {
            outputs_low();
            return;
        }
    }
    else { user_low_ms = 0u; }
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
    GPIOA->BSRR = ((uint32_t)PWM_PINS | (1u << 5)) << 16;
    GPIOA->MODER = (GPIOA->MODER & ~((uint32_t)PWM_MODES | (3u << 10))) |
                   PWM_OUTPUT_MODES | (1u << 10);
    GPIOA->OTYPER &= ~((uint32_t)PWM_PINS | (1u << 5));
    GPIOA->PUPDR &= ~((uint32_t)PWM_MODES | (3u << 10));
    GPIOC->BSRR = 1u << 8;  // Existing nCS stays deasserted, no SPI initialized.
    GPIOC->MODER = (GPIOC->MODER & ~(3u << 16)) | (1u << 16);
    GPIOC->OTYPER &= ~(1u << 8);
    GPIOC->PUPDR &= ~(3u << 16);
    GPIOC->MODER |= (3u << 0) | (3u << 2) | (3u << 4);
    GPIOC->PUPDR &= ~((3u << 0) | (3u << 2) | (3u << 4));
    GPIOA->MODER |= 3u << 8;  // Existing PA4 bus-sense wire stays analog.
    GPIOA->PUPDR &= ~(3u << 8);
    GPIOC->MODER &= ~(3u << 26);  // USER PC13: external pull-up, active LOW.
    GPIOC->PUPDR &= ~(3u << 26);
}

static void uart_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 4)) | (2u << 4);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFu << 8)) | (7u << 8);
    USART2->BRR = 0x01B2u;  // APB1 remains 50 MHz: 115200 baud.
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

static void timer_init(void)
{
    // Match the validated SPI-workaround APB2 domain without initializing SPI.
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2) | RCC_CFGR_PPRE2_DIV4;
    RCC->DCKCFGR &= ~RCC_DCKCFGR_TIMPRE;
    __DSB();
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;
    RCC->APB2RSTR |= RCC_APB2RSTR_TIM1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM1RST;
    // RM0383: TIMPRE=0, APB2 prescaler /4 => TIM1=2*25MHz=50MHz.
    // Center aligned counts 0..ARR..1: period=2*ARR, not 2*(ARR+1).
    TIM1->CR1 = TIM_CR1_CMS_0 | TIM_CR1_ARPE;
    TIM1->CR2 = 0u;  // OIS1/2/3=0, no ADC trigger.
    TIM1->PSC = 0u;
    TIM1->ARR = PWM_ARR;
    TIM1->RCR = 0u;
    TIM1->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE |
                   TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE;
    TIM1->CCMR2 = TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE;
    TIM1->CCR1 = 250u;  // A / PA8: 20%, nominal high width 10us.
    TIM1->CCR2 = 500u;  // B / PA9: 40%, nominal high width 20us.
    TIM1->CCR3 = 750u;  // C / PA10: 60%, nominal high width 30us.
    TIM1->BDTR = TIM_BDTR_OSSI | TIM_BDTR_OSSR;  // MOE=0, AOE=0, no complements.
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0u;
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~0xFFFu) | 0x111u;
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~(uint32_t)PWM_MODES) | PWM_OUTPUT_MODES;
    outputs_low();  // Remain GPIO LOW until the accepted USER press/release.
}

static void timer_start(void)
{
    __disable_irq();
    user_low_ms = 0u;
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E;
    GPIOA->MODER = (GPIOA->MODER & ~(uint32_t)PWM_MODES) |
                   (2u << 16) | (2u << 18) | (2u << 20);
    active = true;
    TIM1->BDTR |= TIM_BDTR_MOE;
    TIM1->CR1 |= TIM_CR1_CEN;
    __enable_irq();
}

int main(void)
{
    safe_gpio_init();
    clock_init();
    uart_init();
    timer_init();
    SysTick->LOAD = SystemCoreClock / 1000u - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
    puts_uart("\r\nDiagnostic: PWM logic scope v3 (continuous); ENABLE permanently LOW\r\n");
    puts_uart("Motor/Hall leads and INL high ties stay disconnected. Power OFF before wiring.\r\n");
    puts_uart("EVM may be powered at 12V for INHA/B/C tracing only; verify ENABLE=LOW.\r\n");
    puts_uart("No SPI/config writes, ADC, encoder, Hall or motor-control code.\r\n");
    puts_uart("APB2=25MHz TIMPRE=0 TIM1=50MHz PSC=0 ARR=1250 center-aligned 20kHz\r\n");
    puts_uart("A=PA8 CN10-23: 20%, 10us high; B=PA9 CN10-21: 40%, 20us high\r\n");
    puts_uart("C=PA10 CN10-33: 60%, 30us high; all periods nominally 50us.\r\n");
    puts_uart("Outputs LOW. Press/release USER to start; runs continuously until USER stop.\r\n");
    wait_button_level(false);
    wait_button_level(true);
    wait_button_level(false);
    timer_start();
    const uint32_t run_start = elapsed_ms;
    puts_uart("PWM ACTIVE; ENABLE LOW. Scope Nucleo pins or EVM INHA/B/C only.\r\n");
    uint32_t last_report = elapsed_ms;
    while (active)
    {
        const uint32_t now = elapsed_ms;
        if ((uint32_t)(now - last_report) >= 1000u)
        {
            puts_uart("PWM ACTIVE elapsed_ms="); put_uint(now - run_start);
            puts_uart(" ENABLE_raw="); put_uint(GPIOB->IDR & 1u);
            puts_uart("\r\n");
            last_report = now;
        }
    }
    puts_uart("STOP: USER pressed\r\n");
    puts_uart("PWM stopped; PA8/PA9/PA10 GPIO LOW; ENABLE LOW. Reset to repeat.\r\n");
    while (1) { __WFI(); }
}
