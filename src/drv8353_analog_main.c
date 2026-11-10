// No-motor analog bring-up for the NUCLEO-F411RE / DRV8353RS-EVM.
// No TIM1/PWM, DMA or control loops are linked. Only the separate
// DRV_ANALOG_ZERO_CAL target writes the CSA gain field, before zero calibration.
// PC0/PC1/PC2 = ISENA/B/C; PA4 = VSENVM. Motor and all INHx/INLx stay absent.
#include "clock.h"
#include "drv8353.h"
#include "stm32f4xx.h"

#include <stdbool.h>
#include <stdint.h>

enum { CHANNELS = 5, EXTERNAL_CHANNELS = 4, SAMPLES = 32, REPORTS = 30,
       FACTORY_CAL_REFERENCE_MV = 3300, VREFINT_INDEX = 4 };
// STM32F411 DS10314 Table 75 / ST LL ADC: 12-bit VREFINT ADC calibration
// at VDDA=3.3 V and 30 C. This is chip-specific system memory, not 1.21 V nominal.
#define VREFINT_CAL_ADDRESS ((const uint16_t *)0x1FFF7A2Au)
static uint16_t vrefint_cal;
#ifdef DRV_ANALOG_ZERO_CAL
enum { CAL_BLOCKS = 8, CAL_SAMPLES = SAMPLES * CAL_BLOCKS };
static uint16_t expected_csa = 0x0283u;
static uint32_t zero_sums[3];
#else
static const uint16_t expected_csa = 0x0283u;
#endif
static volatile uint32_t elapsed_ms;
static volatile uint32_t enable_ms_remaining;

void SysTick_Handler(void)
{
    ++elapsed_ms;
    // Independent of the main polling loops: end the enable window even if a
    // shared SPI wait stalls. Requires the CPU/SysTick interrupt to be running.
    if ((enable_ms_remaining != 0u) && (--enable_ms_remaining == 0u))
    {
        GPIOB->BSRR = 1u << 16;
    }
}

static void time_init(void)
{
    SysTick->LOAD = SystemCoreClock / 1000u - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

static void wait_ms(uint32_t duration)
{
    const uint32_t start = elapsed_ms;
    while ((uint32_t)(elapsed_ms - start) < duration)
    {
    }
}

static void uart_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 4)) | (2u << 4);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFu << 8)) | (7u << 8);
    USART2->BRR = 0x01B2u;  // USART2 APB1 remains 50 MHz.
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void putc_uart(char c)
{
    while ((USART2->SR & USART_SR_TXE) == 0u)
    {
    }
    USART2->DR = (uint8_t)c;
}

static void puts_uart(const char *text)
{
    while (*text != '\0')
    {
        putc_uart(*text++);
    }
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
    while (count != 0u)
    {
        putc_uart(digits[--count]);
    }
}

static void put_hex(uint16_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    puts_uart("0x");
    putc_uart(digits[(value >> 8) & 7u]);
    putc_uart(digits[(value >> 4) & 15u]);
    putc_uart(digits[value & 15u]);
}

static void safe_gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                      RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;
    // ENABLE latch low before output mode, as on the validated SPI diagnostic.
    GPIOB->BSRR = 1u << 16;
    GPIOB->MODER = (GPIOB->MODER & ~3u) | 1u;
    GPIOB->OTYPER &= ~1u;
    GPIOB->PUPDR &= ~3u;
    const uint32_t pwm_mask = (1u << 8) | (1u << 9) | (1u << 10);
    GPIOA->BSRR = pwm_mask << 16;
    GPIOA->MODER = (GPIOA->MODER & ~((3u << 16) | (3u << 18) | (3u << 20))) |
                   (1u << 16) | (1u << 18) | (1u << 20);
    GPIOA->OTYPER &= ~pwm_mask;
    GPIOA->PUPDR &= ~((3u << 16) | (3u << 18) | (3u << 20));
    // Configure analog inputs before the external driver is awakened.
    GPIOC->MODER |= (3u << 0) | (3u << 2) | (3u << 4);
    GPIOC->PUPDR &= ~((3u << 0) | (3u << 2) | (3u << 4));
    GPIOA->MODER |= 3u << 8;
    GPIOA->PUPDR &= ~(3u << 8);
    // MB1136 C04: R30 pulls PC13 high; blue B1 presses it low (sheet 3).
    GPIOC->MODER &= ~(3u << 26);  // PC13 blue USER, active low.
    GPIOC->PUPDR &= ~(3u << 26);
}

static void wait_for_button_level(bool pressed)
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

static void wait_for_user(void)
{
    puts_uart("Power the EVM, then press/release blue USER to start analog readings.\r\n");
    wait_for_button_level(true);
    wait_for_button_level(false);
    puts_uart("USER press/release accepted; USER_raw=");
    put_uint((GPIOC->IDR >> 13) & 1u);
    puts_uart("\r\n");
}

static void validated_spi_clock(void)
{
    // No transfers have occurred since drv8353_init(). ES0287 2.11.4 workaround.
    SPI1->CR1 &= ~SPI_CR1_SPE;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2) | RCC_CFGR_PPRE2_DIV4;
    (void)RCC->CFGR;
    __DSB();
    SPI1->CR1 = (SPI1->CR1 & ~(SPI_CR1_BR_0 | SPI_CR1_BR_1 | SPI_CR1_BR_2)) |
                 SPI_CR1_BR_2 | SPI_CR1_BR_0;  // 25 MHz /64 = 390625 Hz.
    SPI1->CR1 |= SPI_CR1_SPE;
}

static void fault_stop(const char *location, uint32_t pins_b, uint32_t pins_c)
{
    const uint32_t event_ms = elapsed_ms;
    // Logic-only / no-motor diagnostic: capture before ENABLE low resets the
    // registers. This is not an inverter fault-shutdown implementation. The
    // independent SysTick enable deadline remains active during these reads.
    const bool awake_at_capture = enable_ms_remaining != 0u;
    const uint16_t r0 = awake_at_capture ? drv8353_read_reg(DRV8353_REG_FAULT_STATUS_1) : 0u;
    const uint16_t r1 = awake_at_capture ? drv8353_read_reg(DRV8353_REG_VGS_STATUS_2) : 0u;
    const bool fault_pin_after_reads = drv8353_fault_pin_ok();
    drv_enable_low();
    puts_uart("STOP: nFAULT LOW; location="); puts_uart(location);
    puts_uart(" t_ms="); put_uint(event_ms);
    puts_uart(" USER_at_event="); put_uint((pins_c >> 13) & 1u);
    puts_uart(" nFAULT_at_event="); put_uint((pins_b >> 1) & 1u);
    puts_uart("\r\n");
    if (awake_at_capture)
    {
        puts_uart("Fault snapshot before sleep: R0="); put_hex(r0);
        puts_uart(" R1="); put_hex(r1);
        puts_uart(" nFAULT_after_reads=");
        puts_uart(fault_pin_after_reads ? "HIGH\r\n" : "LOW\r\n");
    }
    else
    {
        puts_uart("Enable deadline already elapsed; fault registers not sampled.\r\n");
    }
}

static bool driver_idle_ok(void)
{
    const uint32_t pins_b = GPIOB->IDR;
    if ((pins_b & (1u << 1)) == 0u)
    {
        fault_stop("DRV idle check", pins_b, GPIOC->IDR);
        return false;
    }
    const uint16_t r0 = drv8353_read_reg(DRV8353_REG_FAULT_STATUS_1);
    const uint16_t r1 = drv8353_read_reg(DRV8353_REG_VGS_STATUS_2);
    const uint16_t r2 = drv8353_read_reg(DRV8353_REG_DRIVER_CONTROL);
    const uint16_t r6 = drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
    if ((r0 != 0u) || (r1 != 0u) || (r2 != 0u) || (r6 != expected_csa) ||
        !drv8353_fault_pin_ok())
    {
        drv_enable_low();
        puts_uart("STOP: unexpected DRV idle state R0="); put_hex(r0);
        puts_uart(" R1="); put_hex(r1);
        puts_uart(" R2="); put_hex(r2);
        puts_uart(" R6="); put_hex(r6);
        puts_uart("\r\n");
        return false;
    }
    return true;
}

static void adc_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;
    ADC1->CR2 = 0u;
    // APB2=25 MHz, ADC /4=6.25 MHz. Long 480-cycle acquisition on each input.
    ADC->CCR = ADC_CCR_ADCPRE_0 | ADC_CCR_TSVREFE;
    ADC1->CR1 = 0u;  // 12-bit, one channel, no interrupt.
    ADC1->SQR1 = 0u;
    ADC1->SQR2 = 0u;
    ADC1->SQR3 = 0u;
    // Channels 10/11/12 and internal VREFINT channel 17. At 6.25 MHz,
    // 480-cycle sampling is 76.8 us, above VREFINT's 10 us minimum.
    ADC1->SMPR1 = (7u << 0) | (7u << 3) | (7u << 6) | ADC_SMPR1_SMP17;
    ADC1->SMPR2 = 7u << 12;  // Channel 4.
    ADC1->SR = 0u;
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EOCS; // Single software starts, no DMA.
    wait_ms(2u);  // Exceeds ADC 3 us and VREFINT 10 us startup maxima.
}

static bool adc_read(uint8_t channel, uint16_t *value)
{
    // Called only after the preceding single conversion completed. Changing
    // SQRx during a conversion would trigger STM32F411 erratum ES0287 2.4.1.
    ADC1->SQR3 = channel;
    ADC1->SR = 0u;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    const uint32_t start = elapsed_ms;
    while ((ADC1->SR & ADC_SR_EOC) == 0u)
    {
        const uint32_t pins_b = GPIOB->IDR;
        if ((pins_b & (1u << 1)) == 0u)
        {
            fault_stop("ADC conversion", pins_b, GPIOC->IDR);
            return false;
        }
        if ((uint32_t)(elapsed_ms - start) >= 10u)
        {
            drv_enable_low();
            puts_uart("STOP: ADC timeout\r\n");
            return false;
        }
    }
    *value = (uint16_t)(ADC1->DR & 0x0FFFu);
    return true;
}

typedef struct
{
    uint32_t sum;
    uint16_t minimum;
    uint16_t maximum;
} ChannelStats;

static bool collect_stats(ChannelStats stats[CHANNELS])
{
    static const uint8_t channels[CHANNELS] = {10u, 11u, 12u, 4u, 17u};
    for (unsigned int c = 0u; c < CHANNELS; ++c)
    {
        stats[c] = (ChannelStats){0u, 4095u, 0u};
    }
    for (unsigned int n = 0u; n < SAMPLES; ++n)
    {
        for (unsigned int c = 0u; c < CHANNELS; ++c)
        {
            uint16_t discard, value;
            // Discard one conversion after changing the ADC multiplexer.
            if (!adc_read(channels[c], &discard) || !adc_read(channels[c], &value))
            {
                return false;
            }
            if ((value < 16u) || (value > 4079u))
            {
                drv_enable_low();
                puts_uart("STOP: ADC near rail, channel="); put_uint(channels[c]);
                puts_uart(" raw="); put_uint(value); puts_uart("\r\n");
                return false;
            }
            stats[c].sum += value;
            if (value < stats[c].minimum) { stats[c].minimum = value; }
            if (value > stats[c].maximum) { stats[c].maximum = value; }
        }
    }
    return true;
}

#ifdef DRV_ANALOG_ZERO_CAL
static bool configure_gain_40(void)
{
    // Called only after the reset-state idle check. Change only CSA_GAIN bit 6.
    (void)drv8353_write_reg(DRV8353_REG_CSA_CONTROL, 0x02C3u);
    wait_ms(2u);
    const uint16_t first = drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
    const uint16_t second = drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
    if ((first != 0x02C3u) || (second != first))
    {
        drv_enable_low(); // Sleep resets configuration as well as disabling gates.
        puts_uart("STOP: 40V/V gain readback mismatch R6="); put_hex(first);
        puts_uart(" repeat="); put_hex(second); puts_uart("\r\n");
        return false;
    }
    expected_csa = 0x02C3u;
    puts_uart("CSA gain set/read twice: R6=0x2C3, gain=40V/V\r\n");
    return driver_idle_ok();
}

static bool calibrate_zero(void)
{
    uint16_t minimum[3] = {4095u, 4095u, 4095u};
    uint16_t maximum[3] = {0u, 0u, 0u};
    puts_uart("Measuring zero offsets: motor disconnected, 256 samples/channel.\r\n");
    wait_ms(20u);
    for (unsigned int block = 0u; block < CAL_BLOCKS; ++block)
    {
        ChannelStats stats[CHANNELS];
        if (!driver_idle_ok() || !collect_stats(stats)) { return false; }
        for (unsigned int c = 0u; c < 3u; ++c)
        {
            zero_sums[c] += stats[c].sum;
            if (stats[c].minimum < minimum[c]) { minimum[c] = stats[c].minimum; }
            if (stats[c].maximum > maximum[c]) { maximum[c] = stats[c].maximum; }
        }
    }
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        // Broad zero-current plausibility window, not an accuracy tolerance.
        if ((zero_sums[c] < 1800u * CAL_SAMPLES) ||
            (zero_sums[c] > 2300u * CAL_SAMPLES))
        {
            drv_enable_low();
            puts_uart("STOP: zero offset far from midscale, phase=");
            putc_uart((char)('A' + c)); puts_uart("\r\n");
            return false;
        }
    }
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        const uint32_t thousandths = (zero_sums[c] * 1000u + CAL_SAMPLES / 2u) /
                                     CAL_SAMPLES;
        puts_uart("ISEN"); putc_uart((char)('A' + c)); puts_uart(" zero_raw=");
        put_uint(thousandths / 1000u); putc_uart('.');
        putc_uart((char)('0' + thousandths / 100u % 10u));
        putc_uart((char)('0' + thousandths / 10u % 10u));
        putc_uart((char)('0' + thousandths % 10u));
        puts_uart(" min="); put_uint(minimum[c]);
        puts_uart(" max="); put_uint(maximum[c]); puts_uart("\r\n");
    }
    puts_uart("Fresh zero offsets recorded in RAM; absolute current gain still unverified.\r\n");
    return true;
}
#endif

static bool report_stats(const ChannelStats stats[CHANNELS])
{
    const ChannelStats * const reference = &stats[VREFINT_INDEX];
    const uint32_t reference_mean = (reference->sum + SAMPLES / 2u) / SAMPLES;
    if (reference->sum == 0u)
    {
        drv_enable_low();
        puts_uart("STOP: zero internal-reference reading\r\n");
        return false;
    }
    // Use the unrounded sample sum. Numerator is <433 million for any valid
    // 12-bit calibration count and SAMPLES=32, so 32-bit unsigned math fits.
    const uint32_t vdda_mv = (FACTORY_CAL_REFERENCE_MV * (uint32_t)vrefint_cal *
                              SAMPLES + reference->sum / 2u) / reference->sum;
    puts_uart("VREFINT raw_mean="); put_uint(reference_mean);
    puts_uart(" min="); put_uint(reference->minimum);
    puts_uart(" max="); put_uint(reference->maximum);
    puts_uart(" factory_cal="); put_uint(vrefint_cal);
    puts_uart(" VDDA_mV_est="); put_uint(vdda_mv); puts_uart("\r\n");
    // A broad gross-error check for this nominal 3.3 V Nucleo bench setup,
    // not a precision acceptance tolerance or a substitute for a calibrated meter.
    if ((vdda_mv < 2800u) || (vdda_mv > 3600u))
    {
        drv_enable_low();
        puts_uart("STOP: internal-reference supply estimate implausible for bench\r\n");
        return false;
    }
    static const char * const names[EXTERNAL_CHANNELS] = {"ISENA", "ISENB", "ISENC", "VSENVM"};
    for (unsigned int c = 0u; c < EXTERNAL_CHANNELS; ++c)
    {
        const uint32_t mean = (stats[c].sum + SAMPLES / 2u) / SAMPLES;
        const uint32_t millivolts = (mean * vdda_mv + 2047u) / 4095u;
        puts_uart(names[c]); puts_uart(" raw_mean="); put_uint(mean);
        puts_uart(" min="); put_uint(stats[c].minimum);
        puts_uart(" max="); put_uint(stats[c].maximum);
        puts_uart(" mV_est="); put_uint(millivolts);
#ifdef DRV_ANALOG_ZERO_CAL
        if (c < 3u)
        {
            // Provisional scale only: 7 milliohm EVM shunt * configured 40 V/V
            // = 0.28 V/A. Keep fractional zero counts using 256-sample sums.
            const int32_t delta = (int32_t)(stats[c].sum * CAL_BLOCKS) -
                                  (int32_t)zero_sums[c];
            const int64_t denominator = (int64_t)CAL_SAMPLES * 4095 * 28;
            int64_t numerator = (int64_t)delta * vdda_mv * 100;
            numerator += numerator >= 0 ? denominator / 2 : -denominator / 2;
            int32_t milliamps = (int32_t)(numerator / denominator);
            puts_uart(" I_mA_nominal=");
            if (milliamps < 0) { putc_uart('-'); milliamps = -milliamps; }
            put_uint((uint32_t)milliamps);
        }
#endif
        if (c == 3u)
        {
            // EVM divider 383k / 9.76k; use raw counts to avoid mV rounding.
            const uint32_t bus_mv = (uint32_t)(((uint64_t)mean * vdda_mv *
                                              392760u) / (4095ull * 9760u));
            puts_uart(" VBUS_mV_est="); put_uint(bus_mv);
        }
        puts_uart("\r\n");
    }
    return true;
}

typedef enum { STOP_NONE, STOP_TIME, STOP_BUTTON, STOP_FAULT } StopReason;

static StopReason poll_stop(void)
{
    static bool user_pending;
    static uint32_t user_since;
    if (enable_ms_remaining == 0u)
    {
        puts_uart("30-second enable window ended.\r\n");
        return STOP_TIME;
    }
    const uint32_t pins_b = GPIOB->IDR;
    const uint32_t pins_c = GPIOC->IDR;
    if ((pins_b & (1u << 1)) == 0u)
    {
        fault_stop("between reports", pins_b, pins_c);
        return STOP_FAULT;
    }
    if ((pins_c & (1u << 13)) == 0u)
    {
        if (!user_pending)
        {
            user_pending = true;
            user_since = elapsed_ms;
        }
        if ((uint32_t)(elapsed_ms - user_since) >= 20u)
        {
            drv_enable_low();
            puts_uart("STOP: USER LOW for 20ms; t_ms="); put_uint(elapsed_ms);
            puts_uart(" USER_at_event=0 nFAULT_at_event=1\r\n");
            return STOP_BUTTON;
        }
    }
    else if (user_pending)
    {
        user_pending = false;
        // Preserve evidence of a short pulse without treating it as a stop.
        puts_uart("USER LOW pulse ignored; duration_ms="); put_uint(elapsed_ms - user_since);
        puts_uart("\r\n");
    }
    return STOP_NONE;
}

int main(void)
{
    clock_init();
    safe_gpio_init();
    time_init();
    uart_init();
#ifdef DRV_ANALOG_ZERO_CAL
    puts_uart("\r\nDiagnostic: DRV8353RS no-PWM current-zero v1 (40V/V)\r\n");
#else
    puts_uart("\r\nDiagnostic: DRV8353RS no-PWM analog v4 (factory VREFINT)\r\n");
#endif
    puts_uart("PWM not initialized; motor/INHx/INLx must be disconnected.\r\n");
#ifdef DRV_ANALOG_ZERO_CAL
    puts_uart("DRV config writes: CSA gain 40V/V only; fresh zero offsets each reset.\r\n");
#else
    puts_uart("DRV config writes disabled; default CSA gain 20 V/V.\r\n");
#endif
    puts_uart("PC0/PC1/PC2=ISENA/B/C; PA4=VSENVM.\r\n");
    vrefint_cal = *VREFINT_CAL_ADDRESS;
    puts_uart("Factory VREFINT_CAL raw="); put_uint(vrefint_cal); puts_uart("\r\n");
    if ((vrefint_cal == 0u) || (vrefint_cal >= 4095u))
    {
        puts_uart("STOP: invalid factory reference calibration; ENABLE stays LOW.\r\n");
        SysTick->CTRL = 0u;
        while (1) { __NOP(); }
    }
    puts_uart("Voltage estimates use factory VREFINT calibration; compare VBUS with supply display.\r\n");
    puts_uart("This checks scaling plausibility, not absolute phase-current accuracy.\r\n");
    puts_uart("30 seconds maximum; press blue USER again to stop.\r\n");
    puts_uart("USER released=HIGH, pressed=LOW; arm debounce=25ms, stop debounce=20ms.\r\n");
    wait_for_user();
    drv8353_init();
    enable_ms_remaining = 30000u;
    validated_spi_clock();
    bool completed = false;
    if (driver_idle_ok())
    {
        puts_uart("DRV idle OK: R0/R1/R2=0x000 R6=0x283 nFAULT=HIGH\r\n");
#ifdef DRV_ANALOG_ZERO_CAL
        if (!configure_gain_40()) { goto stopped; }
#endif
        puts_uart("APB2=25MHz SPI=/64 ADC=/4; ENABLE HIGH\r\n");
        adc_init();
#ifdef DRV_ANALOG_ZERO_CAL
        if (!calibrate_zero()) { goto stopped; }
#endif
        const uint32_t start = elapsed_ms;
        for (unsigned int report = 0u; report < REPORTS; ++report)
        {
            if (!driver_idle_ok()) { break; }
            ChannelStats stats[CHANNELS];
            if (!collect_stats(stats)) { break; }
            puts_uart("--- analog report "); put_uint(report + 1u); puts_uart(" ---\r\n");
            if (!report_stats(stats)) { break; }
            const uint32_t next = (report + 1u) * 1000u;
            while ((uint32_t)(elapsed_ms - start) < next)
            {
                const StopReason reason = poll_stop();
                if (reason != STOP_NONE)
                {
                    completed = (reason == STOP_TIME) && (report + 1u == REPORTS);
                    goto stopped;
                }
            }
            completed = (report + 1u == REPORTS);
        }
    }
stopped:
    drv_enable_low();
    enable_ms_remaining = 0u;
    ADC1->CR2 = 0u;
    puts_uart(completed ? "Acquisition complete; review VREFINT and VBUS estimates.\r\n"
                        : "Acquisition stopped early.\r\n");
    puts_uart("Analog test complete; ENABLE LOW. Reset to repeat.\r\n");
    // Stay awake to ease debugger reconnection. An earlier sleeping diagnostic
    // needed under-reset recovery; the exact connection-failure cause is unknown.
    SysTick->CTRL = 0u;
    while (1) { __NOP(); }
}
