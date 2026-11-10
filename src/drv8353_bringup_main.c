// Logic-only STM32F411RE <-> DRV8353RS-EVM bring-up.
//
// Safety boundary for this target:
//   - TIM1 is never initialized.
//   - The current-sense ADC is never initialized.
//   - Only the separate DRV_SPI_WRITE_TEST target writes a configuration field:
//     CSA gain is changed, checked, then restored with the motor disconnected.
//   - PA8/PA9/PA10 are held low as GPIOs before ENABLE is asserted.
//
// The target only enables the DRV8353, reads its seven implemented SPI
// registers, samples nFAULT, and reports the results on the Nucleo ST-LINK
// virtual COM port (USART2, 115200 8-N-1).

#include "clock.h"
#include "drv8353.h"
#include "stm32f4xx.h"

#include <stdint.h>

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

    // APB1 = 50 MHz; 0x1B2 is approximately 115200 baud at 16x oversampling.
    USART2->BRR = 0x01B2u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart2_putc(char c)
{
    while ((USART2->SR & USART_SR_TXE) == 0u)
    {
    }
    USART2->DR = (uint8_t)c;
}

static void uart2_puts(const char *text)
{
    while (*text != '\0')
    {
        uart2_putc(*text++);
    }
}

static void uart2_put_hex11(uint16_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    value &= 0x07FFu;
    uart2_puts("0x");
    uart2_putc(digits[(value >> 8) & 0x0Fu]);
    uart2_putc(digits[(value >> 4) & 0x0Fu]);
    uart2_putc(digits[value & 0x0Fu]);
}

static void hold_pwm_inputs_low(void)
{
    const uint32_t pwm_mask = (1u << 8) | (1u << 9) | (1u << 10);

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    // Set the output data low before changing the pin modes to outputs.
    GPIOA->BSRR = pwm_mask << 16;
    GPIOA->MODER &= ~((3u << (8u * 2u)) |
                      (3u << (9u * 2u)) |
                      (3u << (10u * 2u)));
    GPIOA->MODER |=  ((1u << (8u * 2u)) |
                      (1u << (9u * 2u)) |
                      (1u << (10u * 2u)));
    GPIOA->OTYPER &= ~pwm_mask;
    GPIOA->PUPDR &= ~((3u << (8u * 2u)) |
                      (3u << (9u * 2u)) |
                      (3u << (10u * 2u)));
}

static void drv_bus_init_disabled(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;

    // PB0 = DRV ENABLE. Set its output latch low before enabling output mode,
    // then keep the DRV asleep until the operator presses the blue USER button.
    GPIOB->BSRR = (1u << 0) << 16;
    GPIOB->MODER &= ~(3u << (0u * 2u));
    GPIOB->MODER |=  (1u << (0u * 2u));
    GPIOB->OTYPER &= ~(1u << 0);
    GPIOB->PUPDR &= ~(3u << (0u * 2u));
}

static void user_button_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;

    // MB1136 C04 sheet 3: R30 pulls PC13 high; blue USER presses it low.
    GPIOC->MODER &= ~(3u << (13u * 2u));
    GPIOC->PUPDR &= ~(3u << (13u * 2u));
}

static void wait_for_user_button(void)
{
    uart2_puts("Power the EVM, then press the blue USER button to assert ENABLE.\r\n");

    while ((GPIOC->IDR & (1u << 13)) != 0u)
    {
    }
    delay_ms(25u);
    while ((GPIOC->IDR & (1u << 13)) == 0u)
    {
    }
    delay_ms(25u);
}

#if !defined(DRV_SPI_COMPARE) && !defined(DRV_SPI_WRITE_TEST)
static void print_register(const char *name, uint8_t address)
{
    uart2_puts(name);
    uart2_putc('=');
    uart2_put_hex11(drv8353_read_reg(address));
    uart2_puts("\r\n");
}

static void print_snapshot(void)
{
    uart2_puts("nFAULT=");
    uart2_puts(drv8353_fault_pin_ok() ? "HIGH (no fault)\r\n"
                                     : "LOW (FAULT ACTIVE)\r\n");

    print_register("R0_FAULT_STATUS_1", DRV8353_REG_FAULT_STATUS_1);
    print_register("R1_VGS_STATUS_2",   DRV8353_REG_VGS_STATUS_2);
    print_register("R2_DRIVER_CONTROL", DRV8353_REG_DRIVER_CONTROL);
    print_register("R3_GATE_DRIVE_HS",  DRV8353_REG_GATE_DRIVE_HS);
    print_register("R4_GATE_DRIVE_LS",  DRV8353_REG_GATE_DRIVE_LS);
    print_register("R5_OCP_CONTROL",    DRV8353_REG_OCP_CONTROL);
    print_register("R6_CSA_CONTROL",    DRV8353_REG_CSA_CONTROL);
}
#endif

#ifdef DRV_SPI_COMPARE
// A separate target uses the same seven wires to compare peripheral SPI with
// deliberately slow GPIO clocks. Every command has R/W=1; no register writes.
static void gpio_spi_settle(void)
{
    // Deliberately much longer than the DRV's nanosecond setup/hold times.
    // Exact frequency is unimportant for this diagnostic.
    for (volatile uint32_t cycles = 0u; cycles < 1000u; ++cycles)
    {
        __NOP();
    }
}

static void gpio_spi_read(uint8_t address, uint16_t *before_fall,
                          uint16_t *after_fall)
{
    const uint16_t command = 0x8000u | ((uint16_t)address << 11);
    uint16_t early = 0u;
    uint16_t late = 0u;

    // PC8 = nSCS; PA5 = SCLK; PA7 = SDI; PA6 = SDO input.
    GPIOA->BSRR = (1u << 5) << 16;
    GPIOC->BSRR = (1u << 8) << 16;
    gpio_spi_settle();

    for (uint16_t mask = 0x8000u; mask != 0u; mask >>= 1)
    {
        GPIOA->BSRR = (command & mask) ? (1u << 7) : ((1u << 7) << 16);
        gpio_spi_settle();
        GPIOA->BSRR = 1u << 5;  // Rising edge: DRV propagates SDO.
        gpio_spi_settle();
        early = (uint16_t)((early << 1) | ((GPIOA->IDR >> 6) & 1u));
        GPIOA->BSRR = (1u << 5) << 16;  // Falling edge: DRV captures SDI.
        gpio_spi_settle();
        late = (uint16_t)((late << 1) | ((GPIOA->IDR >> 6) & 1u));
    }

    // Both samples, including bit 0, were taken with nSCS still low.
    GPIOC->BSRR = 1u << 8;
    gpio_spi_settle();
    *before_fall = early & 0x07FFu;
    *after_fall = late & 0x07FFu;
}

static void run_spi_comparison_passes(void)
{
    uint16_t hardware[7];
    uint16_t early[7];
    uint16_t late[7];
    const uint32_t spi_modes = (3u << (5u * 2u)) |
                               (3u << (6u * 2u)) |
                               (3u << (7u * 2u));

    for (unsigned int pass = 0u; pass < 2u; ++pass)
    {
        uart2_puts("--- SPI comparison pass ---\r\n");
        uart2_puts("nFAULT=");
        uart2_puts(drv8353_fault_pin_ok() ? "HIGH\r\n" : "LOW\r\n");

        for (uint8_t address = 0u; address < 7u; ++address)
        {
            hardware[address] = drv8353_read_reg(address);
        }

        // The peripheral transfer waits for BSY=0 before returning. Take over
        // only with CS high and SCLK low; preserve unrelated GPIO configuration.
        const uint32_t saved_cr1 = SPI1->CR1;
        const uint32_t saved_modes = GPIOA->MODER;
        SPI1->CR1 &= ~SPI_CR1_SPE;
        GPIOC->BSRR = 1u << 8;
        GPIOA->BSRR = ((1u << 5) | (1u << 7)) << 16;
        GPIOA->MODER = (saved_modes & ~spi_modes) |
                       (1u << (5u * 2u)) | (1u << (7u * 2u));
        gpio_spi_settle();

        for (uint8_t address = 0u; address < 7u; ++address)
        {
            gpio_spi_read(address, &early[address], &late[address]);
        }

        GPIOA->MODER = saved_modes;
        SPI1->CR1 = saved_cr1;

        for (uint8_t address = 0u; address < 7u; ++address)
        {
            uart2_putc('R');
            uart2_putc((char)('0' + address));
            uart2_puts(" HW=");
            uart2_put_hex11(hardware[address]);
            uart2_puts(" GPIO_before_fall=");
            uart2_put_hex11(early[address]);
            uart2_puts(" GPIO_after_fall=");
            uart2_put_hex11(late[address]);
            uart2_puts("\r\n");
        }
    }

}

static void run_spi_comparison(void)
{
    const uint32_t saved_cr1 = SPI1->CR1;
    const uint32_t saved_ppre2 = RCC->CFGR & RCC_CFGR_PPRE2;

    uart2_puts("--- Baseline: APB2=100MHz, SPI=/256, SCK=390625Hz, PA5=low ---\r\n");
    run_spi_comparison_passes();

    // STM32F411 errata ES0287 Rev 6, 2.11.4: delayed SCK pad feedback
    // can leave the last received bit stale. At 30 pF ST specifies at most
    // 25 MHz APB for low-speed SCK GPIO. Test that workaround without changing
    // the external clock rate or the SCK slew setting: 100/256 = 25/64 MHz.
    // Only this diagnostic changes APB2; USART2 remains on 50 MHz APB1 and
    // no APB2 timers or ADC are initialized. This is not a motor-image fix.
    SPI1->CR1 &= ~SPI_CR1_SPE;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2) | RCC_CFGR_PPRE2_DIV4;
    (void)RCC->CFGR;
    __DSB();
    SPI1->CR1 = (saved_cr1 & ~(SPI_CR1_SPE | SPI_CR1_BR_0 |
                              SPI_CR1_BR_1 | SPI_CR1_BR_2)) |
                 SPI_CR1_BR_2 | SPI_CR1_BR_0;  // BR=101: /64.
    SPI1->CR1 |= SPI_CR1_SPE;

    uart2_puts("--- Workaround: APB2=25MHz, SPI=/64, SCK=390625Hz, PA5=low ---\r\n");
    run_spi_comparison_passes();

    drv_enable_low();
    SPI1->CR1 &= ~SPI_CR1_SPE;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2) | saved_ppre2;
    (void)RCC->CFGR;
    __DSB();
    SPI1->CR1 = saved_cr1;
    uart2_puts("Comparison complete; ENABLE LOW. Reset to repeat.\r\n");
}
#endif

#ifdef DRV_SPI_WRITE_TEST
static void print_csa_value(const char *label, uint16_t value)
{
    uart2_puts(label);
    uart2_put_hex11(value);
    uart2_puts("\r\n");
}

static void run_spi_write_test(void)
{
    const uint32_t saved_cr1 = SPI1->CR1;
    const uint32_t saved_ppre2 = RCC->CFGR & RCC_CFGR_PPRE2;

    // Use the read path validated on this bench, before attempting any write.
    SPI1->CR1 &= ~SPI_CR1_SPE;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2) | RCC_CFGR_PPRE2_DIV4;
    (void)RCC->CFGR;
    __DSB();
    SPI1->CR1 = (saved_cr1 & ~(SPI_CR1_SPE | SPI_CR1_BR_0 |
                              SPI_CR1_BR_1 | SPI_CR1_BR_2)) |
                 SPI_CR1_BR_2 | SPI_CR1_BR_0;
    SPI1->CR1 |= SPI_CR1_SPE;
    uart2_puts("APB2=25MHz, SPI=/64, SCK=390625Hz, PA5=low\r\n");

    const uint16_t original = drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
    const uint16_t original_again = drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
    print_csa_value("CSA original=", original);
    print_csa_value("CSA original repeat=", original_again);
    // Require the observed reset state and fault-free idle before writing.
    // 0x283 has CSA_GAIN=10 (20 V/V); only bit 6 will change, to 40 V/V.
    const uint16_t fault_before = drv8353_read_reg(DRV8353_REG_FAULT_STATUS_1);
    const uint16_t vgs_before = drv8353_read_reg(DRV8353_REG_VGS_STATUS_2);
    print_csa_value("R0 before=", fault_before);
    print_csa_value("R1 before=", vgs_before);
    const bool pin_before_ok = drv8353_fault_pin_ok();
    uart2_puts(pin_before_ok ? "nFAULT before=HIGH\r\n" : "nFAULT before=LOW\r\n");

    if ((original != 0x0283u) || (original_again != original) ||
        (fault_before != 0u) || (vgs_before != 0u) || !pin_before_ok)
    {
        uart2_puts("Write test: ABORT (baseline unexpected; no writes sent)\r\n");
    }
    else
    {
        const uint16_t test_value = original ^ (1u << 6);
        print_csa_value("CSA test expected=", test_value);
        (void)drv8353_write_reg(DRV8353_REG_CSA_CONTROL, test_value);
        delay_ms(2u);
        const uint16_t changed = drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
        const uint16_t changed_again = drv8353_read_reg(DRV8353_REG_CSA_CONTROL);

        // Always attempt restoration, including when the test readback differs.
        (void)drv8353_write_reg(DRV8353_REG_CSA_CONTROL, original);
        delay_ms(2u);
        const uint16_t restored = drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
        const uint16_t restored_again = drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
        const uint16_t fault_after = drv8353_read_reg(DRV8353_REG_FAULT_STATUS_1);
        const uint16_t vgs_after = drv8353_read_reg(DRV8353_REG_VGS_STATUS_2);
        const bool pin_after_ok = drv8353_fault_pin_ok();

        print_csa_value("CSA test readback=", changed);
        print_csa_value("CSA test repeat=", changed_again);
        print_csa_value("CSA restored=", restored);
        print_csa_value("CSA restored repeat=", restored_again);
        print_csa_value("R0 after=", fault_after);
        print_csa_value("R1 after=", vgs_after);
        uart2_puts(pin_after_ok ? "nFAULT after=HIGH\r\n" : "nFAULT after=LOW\r\n");
        const bool passed = (changed == test_value) &&
                            (changed_again == test_value) &&
                            (restored == original) && (restored_again == original) &&
                            (fault_after == 0u) && (vgs_after == 0u) && pin_after_ok;
        uart2_puts(passed ? "Write/read/restore: PASS\r\n"
                         : "Write/read/restore: FAIL\r\n");
    }

    // Sleeping also resets the configuration. These are bounded test actions,
    // not a runtime fault supervisor; shared SPI wait loops still lack timeouts.
    drv_enable_low();
    SPI1->CR1 &= ~SPI_CR1_SPE;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2) | saved_ppre2;
    (void)RCC->CFGR;
    __DSB();
    SPI1->CR1 = saved_cr1;
    uart2_puts("Write test complete; ENABLE LOW. Reset to repeat.\r\n");
}
#endif

int main(void)
{
    clock_init();
    uart2_init();
    hold_pwm_inputs_low();
    drv_bus_init_disabled();
    user_button_init();

    uart2_puts("\r\nSTM32F411RE + DRV8353RS logic-only bring-up\r\n");
    uart2_puts("PWM: not initialized; PA8/PA9/PA10 held low\r\n");
#ifdef DRV_SPI_WRITE_TEST
    uart2_puts("DRV config writes: CSA gain write/read/restore only\r\n");
    uart2_puts("Diagnostic: CSA write/read/restore v1\r\n");
#else
    uart2_puts("DRV config writes: disabled\r\n");
#endif
#ifdef DRV_SPI_COMPARE
    uart2_puts("Diagnostic: read-only hardware/GPIO SPI comparison v2 (APB2 test)\r\n");
#endif

    wait_for_user_button();

    // drv8353_init() configures SPI/control GPIO, resets through ENABLE low,
    // then asserts ENABLE and waits for the DRV to wake.
    drv8353_init();

#if defined(DRV_SPI_COMPARE) || defined(DRV_SPI_WRITE_TEST)
#ifdef DRV_SPI_WRITE_TEST
    run_spi_write_test();
#else
    run_spi_comparison();
#endif
    while (1)
    {
        __WFI();
    }
#else
    uart2_puts(drv8353_spi_self_test()
                  ? "SPI sanity check: PASS\r\n"
                  : "SPI sanity check: FAIL (SDO appears stuck high or low)\r\n");
    print_snapshot();

    while (1)
    {
        delay_ms(1000u);
        uart2_puts("--- status ---\r\n");
        print_snapshot();
    }
#endif
}
