// DRV8353RS-EVM scope diagnostics and a separately selected bounded motor probe.
// DRV_GATE_3PWM keeps PA8/9/10 on INHA/B/C and controls INL enables separately.
// The older default low-side-only diagnostic is retained but skipped on this bench.
// DRV_GATE_MOTOR_TEST alone enables stationary current regulation. No Hall,
// encoder, FOC, automatic restart or normal motor configuration is linked.
#include "clock.h"
#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef DRV_GATE_MOTOR_TEST
#include "motor_probe.h"
#include "motor_probe_trace.h"
#include "motor_probe_regular.h"
#include "motor_probe_pwm.h"
#ifdef DRV_GATE_SPIN_TEST
#include "motor_probe_sequence.h"
#endif
#ifdef DRV_GATE_CONTINUOUS
#include "motor_continuous.h"
#endif
#endif

#if defined(DRV_GATE_ADC_CHECK) && !defined(DRV_GATE_3PWM)
#error "DRV_GATE_ADC_CHECK requires the three-PWM wiring and INL controls"
#endif
#if defined(DRV_GATE_SYNC) && !defined(DRV_GATE_ADC_CHECK)
#error "DRV_GATE_SYNC requires DRV_GATE_ADC_CHECK and its fresh zero capture"
#endif
#if defined(DRV_GATE_MOTOR_TEST) && !defined(DRV_GATE_SYNC)
#error "Motor probe requires synchronous acquisition and freshness guards"
#endif
#if defined(DRV_GATE_SPIN_TEST) && !defined(DRV_GATE_MOTOR_TEST)
#error "Stepped spin requires the protected motor-current probe"
#endif
#if (defined(DRV_GATE_CRAWL_250MA) || defined(DRV_GATE_CRAWL_350MA) || defined(DRV_GATE_TORQUE_700MA)) && !defined(DRV_GATE_SPIN_TEST)
#error "Current trials require the guarded stepped-crawl sequence"
#endif
#if defined(DRV_GATE_HIZ_SPI) && !defined(DRV_GATE_CRAWL_250MA) && !defined(DRV_GATE_CRAWL_350MA) && !defined(DRV_GATE_TORQUE_700MA)
#error "Hi-Z SPI startup requires an isolated current trial"
#endif
#if defined(DRV_GATE_CONTINUOUS) && (!defined(DRV_GATE_HIZ_SPI) || (!defined(DRV_GATE_CRAWL_350MA) && !defined(DRV_GATE_TORQUE_700MA)))
#error "Continuous operation requires an isolated Hi-Z-SPI profile"
#endif
#if defined(DRV_GATE_SMOOTH) && !defined(DRV_GATE_CONTINUOUS)
#error "Smooth current blend requires the continuous profile"
#endif
#if defined(DRV_GATE_TORQUE_700MA) && !defined(DRV_GATE_SMOOTH)
#error "700mA torque trial requires the smooth current controller"
#endif

#ifdef DRV_GATE_3PWM
enum { CTRL_RUN = 0x4A0u, CTRL_COAST = 0x4A4u,
       INL_PINS = (1u << 6) | (1u << 7) | (1u << 9),
       INL_MODES = (3u << 12) | (3u << 14) | (3u << 18),
       INL_GPIO = (1u << 12) | (1u << 14) | (1u << 18) };
#else
enum { CTRL_RUN = 0x480u, CTRL_COAST = 0x484u };
#endif
enum { PWM_PINS = 0x700u, PWM_MODES = 0x3F0000u,
       PWM_GPIO = 0x150000u, PWM_AF = 0x2A0000u };
#ifdef DRV_GATE_MOTOR_TEST
#ifdef DRV_GATE_CONTINUOUS
enum { WINDOW_MS = 0u };  // User-requested continuous operation; faults/USER still stop.
#elif defined(DRV_GATE_SPIN_TEST)
enum { WINDOW_MS = 12000u };
#else
enum { WINDOW_MS = 2000u };
#endif
enum { CSA_RUN = 0x2C3u,
       GATE_HS_RUN = 0x333u, GATE_LS_RUN = 0x333u,
       MOTOR_OCP_RUN = 0x115u };  //150/300mA,100ns deadtime; retain latched0.2V OCP.
#ifdef DRV_GATE_TORQUE_700MA
_Static_assert(MOTOR_PROBE_MAX_CCR * 100u <= 1250u * 8u, "Torque trial differential ceiling <=8percent");
#elif defined(DRV_GATE_CRAWL_350MA)
_Static_assert(MOTOR_PROBE_MAX_CCR * 10000u <= 1250u * 448u, "Trial differential ceiling <=4.48percent");
#elif defined(DRV_GATE_CRAWL_250MA)
_Static_assert(MOTOR_PROBE_MAX_CCR * 1000u <= 1250u * 32u, "Trial differential ceiling <=3.2percent");
#else
_Static_assert(MOTOR_PROBE_MAX_CCR * 100u <= 1250u * 2u, "Motor drive ceiling must stay<=2percent");
#endif
#else
enum { WINDOW_MS = 300000u, CSA_RUN = 0x283u };
#endif
typedef enum { STOP_NONE, STOP_USER, STOP_FAULT, STOP_TIME, STOP_SPI,
               STOP_REGISTER, STOP_BUS, STOP_SELF_TEST, STOP_CURRENT_ADC,
               STOP_SYNC_ADC, STOP_MOTOR_PROBE, STOP_COMPLETE } StopReason;
static volatile uint32_t elapsed_ms, remaining_ms, user_low_ms, stop_ms, stop_pins;
static volatile bool active, fault_monitor;
static volatile StopReason stop_reason;
volatile uint32_t g_gate_shutdown_self_test;  //0=not run,1=PASS,2=FAIL; SWD observable.
#ifdef DRV_GATE_ADC_CHECK
enum { CURRENT_ZERO_SAMPLES = 256u, CURRENT_REPORT_SAMPLES = 128u };
static uint32_t current_zero_sum[3], current_vdda_mv;
typedef struct
{
    uint32_t sum;
    uint16_t minimum, maximum;
} CurrentStats;
#endif
#ifdef DRV_GATE_SYNC
enum { SYNC_TRIGGER_COUNT = 850u, SYNC_ADC_SAMPLE_CYCLES = 56u,
       SYNC_SEQUENCE_TIMER_TICKS = 3u * (SYNC_ADC_SAMPLE_CYCLES + 12u) * 4u,
       SYNC_PERIOD_MIN = 4500u, SYNC_PERIOD_MAX = 5500u,
       SYNC_MAX_UNREPORTED = 60000u };
// Timer50MHz, ADC12.5MHz. INHC is LOW from tick750 to tick1750.
// Trigger at17us, nominal end33.32us, next INHC edge35us. Reserve1us.
_Static_assert(SYNC_TRIGGER_COUNT >= 750u + 100u, "Need2us settling before trigger");
_Static_assert(SYNC_TRIGGER_COUNT + SYNC_SEQUENCE_TIMER_TICKS + 50u < 2500u - 750u,
               "Injected sequence must fit the all-low-side-ON window");
_Static_assert(2500u - 100u + (144u + 12u) * 4u + 250u < 2500u + SYNC_TRIGGER_COUNT,
               "Regular bus/reference conversion needs5us before next injection");
_Static_assert(ADC_SR_JEOC == (1u << 2), "Wrong F411 JEOC bit");
static volatile bool sync_armed;
static volatile uint32_t sync_last_ms, sync_last_cycle, sync_active_bank;
typedef struct
{
    CurrentStats stats[3];
    uint32_t count, period_min, period_max, cnt_min, cnt_max;
} SyncBank;
static volatile SyncBank sync_banks[2];
static volatile uint16_t sync_last_raw[3];
volatile uint32_t g_sync_adc_total, g_sync_failure, g_sync_last_cnt, g_sync_last_period;
// Failure1=config,2=completion phase,3=period,4=stale,5=report backlog,6=no samples.
#endif
#ifdef DRV_GATE_MOTOR_TEST
_Static_assert(CURRENT_ZERO_SAMPLES == 256u, "Motor current conversion expects256zero samples");
static MotorProbe motor_probe_state;
static volatile bool motor_on, motor_pair_done;
static volatile uint32_t motor_pair_start_ms;
static volatile int32_t motor_last_ma[3];
volatile uint32_t g_motor_failure;
#ifdef DRV_GATE_SPIN_TEST
enum { MOTOR_TRACE_BANKS = MOTOR_SEQUENCE_TRACES };
#ifdef DRV_GATE_CONTINUOUS
static MotorContinuous motor_continuous_state;
static volatile uint32_t motor_continuous_progress_ms;
#else
static volatile uint32_t motor_pair_end_ms;
typedef struct
{
    uint32_t valid, start_ms, end_ms, targets, peak, differential;
    int32_t filtered_ma[3];
} MotorSpinSummary;
static MotorSpinSummary motor_spin_summaries[MOTOR_SEQUENCE_STEPS];
static uint32_t motor_spin_holds, motor_reverse_passed, motor_field_start_ms, motor_field_end_ms;
#endif
#else
enum { MOTOR_TRACE_BANKS = 3 };
#endif
static MotorProbeTrace motor_traces[MOTOR_TRACE_BANKS];
static uint32_t motor_step;
static volatile MotorProbeRegular motor_regular;
static volatile uint32_t bus_failure, bus_channel, bus_reads, bus_ref_sum, bus_raw_sum, bus_vdda_mv;
static volatile uint32_t motor_irq_cycles_max, motor_irq_end_count, motor_irq_end_down;
static volatile bool motor_spi_active;
static volatile uint32_t motor_spi_last_reg;
typedef struct
{
    uint32_t valid, adc_sr, adc_cr1, adc_cr2, lower, upper, count, down;
    uint32_t time_ms, samples, command_ccr[3], source, sink, motor_was_on;
    uint32_t spi_active, spi_busy, spi_cs_low, spi_last_reg;
    uint16_t raw[3];
} MotorStopSnapshot;
static volatile MotorStopSnapshot motor_stop_snapshot;
#endif

static void stop_all(StopReason reason)
{
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    GPIOB->BSRR = 1u << 16;  // First action: remove DRV ENABLE, no SPI dependency.
#ifdef DRV_GATE_3PWM
    GPIOC->BSRR = (uint32_t)INL_PINS << 16;  //3PWM Hi-Z, no SPI dependency.
#endif
#ifdef DRV_GATE_MOTOR_TEST
    if (active && stop_reason == STOP_NONE && motor_stop_snapshot.valid == 0u)
    {
        // Driver ENABLE and INLs are already LOW. Freeze timer/next trigger
        // before logging; an in-flight ADC sequence may still finish.
        TIM1->CR1 &= ~TIM_CR1_CEN;
        ADC1->CR2 &= ~ADC_CR2_JEXTEN;
        motor_stop_snapshot.adc_sr = ADC1->SR;
        motor_stop_snapshot.adc_cr1 = ADC1->CR1;
        motor_stop_snapshot.adc_cr2 = ADC1->CR2;
        motor_stop_snapshot.raw[0] = (uint16_t)ADC1->JDR1;
        motor_stop_snapshot.raw[1] = (uint16_t)ADC1->JDR2;
        motor_stop_snapshot.raw[2] = (uint16_t)ADC1->JDR3;
        motor_stop_snapshot.lower = ADC1->LTR;
        motor_stop_snapshot.upper = ADC1->HTR;
        motor_stop_snapshot.count = TIM1->CNT;
        motor_stop_snapshot.down = (TIM1->CR1 & TIM_CR1_DIR) != 0u;
        motor_stop_snapshot.command_ccr[0] = TIM1->CCR1;
        motor_stop_snapshot.command_ccr[1] = TIM1->CCR2;
        motor_stop_snapshot.command_ccr[2] = TIM1->CCR3;
        motor_stop_snapshot.time_ms = elapsed_ms;
        motor_stop_snapshot.samples = motor_probe_state.samples;
        motor_stop_snapshot.source = motor_probe_state.source;
        motor_stop_snapshot.sink = motor_probe_state.sink;
        motor_stop_snapshot.motor_was_on = motor_on;
        motor_stop_snapshot.spi_active = motor_spi_active;
        motor_stop_snapshot.spi_busy = (SPI1->SR & SPI_SR_BSY) != 0u;
        motor_stop_snapshot.spi_cs_low = (GPIOC->IDR & (1u << 8)) == 0u;
        motor_stop_snapshot.spi_last_reg = motor_spi_last_reg;
        motor_stop_snapshot.valid = 1u;
    }
#endif
#ifdef DRV_GATE_SYNC
    sync_armed = false;
    if ((RCC->APB2ENR & RCC_APB2ENR_ADC1EN) != 0u)
    {
        ADC1->CR2 &= ~ADC_CR2_JEXTEN;
        ADC1->CR1 &= ~ADC_CR1_JEOCIE;
#ifdef DRV_GATE_MOTOR_TEST
        ADC1->CR1 &= ~(ADC_CR1_AWDIE | ADC_CR1_JAWDEN);
        motor_on = false;
#endif
    }
    NVIC_DisableIRQ(ADC_IRQn);
    NVIC_ClearPendingIRQ(ADC_IRQn);
    GPIOC->BSRR = 1u << 20;  // Completion IRQ marker PC4 LOW.
    GPIOA->BSRR = 1u << 27;  // Hardware trigger marker PA11 LOW.
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 22)) | (1u << 22);
#endif
    TIM1->BDTR &= ~TIM_BDTR_MOE;
    TIM1->CCER = 0u;
    TIM1->CR1 &= ~TIM_CR1_CEN;
    GPIOA->BSRR = PWM_PINS << 16;
    GPIOA->MODER = (GPIOA->MODER & ~PWM_MODES) | PWM_GPIO;
    EXTI->IMR &= ~2u;
    active = false;
    fault_monitor = false;
    if (stop_reason == STOP_NONE)
    {
        stop_pins = GPIOB->IDR;
        stop_ms = elapsed_ms;
        stop_reason = reason;
    }
    __set_PRIMASK(mask);
}

void EXTI1_IRQHandler(void)
{
    if ((EXTI->PR & 2u) != 0u)
    {
        if (active) { stop_all(STOP_FAULT); }
        EXTI->PR = 2u;
    }
}

#ifdef DRV_GATE_SYNC
#ifdef DRV_GATE_MOTOR_TEST
#ifndef DRV_GATE_CONTINUOUS
static void motor_end_pair(void)
{
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    GPIOC->BSRR = (uint32_t)INL_PINS << 16;  // Hi-Z first, independent of preload/SPI/UART.
#ifdef DRV_GATE_HIZ_SPI
    // Drive is already removed. Current watchdog is armed again BEFORE any
    // bridge enable, after configuration traffic has finished and settled.
    ADC1->CR1 &= ~(ADC_CR1_AWDIE | ADC_CR1_JAWDEN);
    ADC1->SR = ~ADC_SR_AWD;
#endif
    motor_on = false;
    TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = 0u;
#ifdef DRV_GATE_SPIN_TEST
    motor_pair_end_ms = elapsed_ms;
#endif
    motor_pair_done = true;
    __set_PRIMASK(mask);
}
#endif

static bool motor_sample(const uint16_t raw[3])
{
    int32_t current[3];
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        current[c] = motor_probe_current_ma(raw[c], current_zero_sum[c], current_vdda_mv);
        motor_last_ma[c] = current[c];
    }
    if (!motor_on)
    {
#ifdef DRV_GATE_HIZ_SPI
        if ((GPIOC->IDR & INL_PINS)!=0u)
        { g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false; }
#endif
        return active;
    }
#ifdef DRV_GATE_HIZ_SPI
    if ((ADC1->CR1 & (ADC_CR1_JAWDEN | ADC_CR1_AWDIE)) != (ADC_CR1_JAWDEN | ADC_CR1_AWDIE))
    { g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false; }
#endif
    const uint16_t previous[3] = {(uint16_t)TIM1->CCR1, (uint16_t)TIM1->CCR2,
                                  (uint16_t)TIM1->CCR3};
#ifndef DRV_GATE_SMOOTH
    const uint8_t source=motor_probe_state.source,sink=motor_probe_state.sink;
#endif
#ifdef DRV_GATE_SMOOTH
    if (!motor_smooth_pwm_valid(previous) || (GPIOC->IDR & INL_PINS)!=INL_PINS)
#else
    if (!motor_probe_pwm_valid(source,sink,previous) || (GPIOC->IDR & INL_PINS)!=INL_PINS)
#endif
    {
        g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false;
    }
#ifdef DRV_GATE_SMOOTH
    uint16_t minimum=previous[0],maximum=previous[0];
    for (unsigned int c=1u;c<3u;++c)
    { if (previous[c]<minimum) { minimum=previous[c]; } if (previous[c]>maximum) { maximum=previous[c]; } }
    const uint16_t command_ccr=maximum-minimum;
#else
    const uint16_t command_ccr = previous[source] - previous[sink];
#endif
#ifdef DRV_GATE_CONTINUOUS
    const uint32_t advances_before=motor_continuous_state.advances;
    const MotorProbeResult result=motor_continuous_update(&motor_continuous_state,&motor_probe_state,current);
#else
    const MotorProbeResult result = motor_probe_update(&motor_probe_state, current);
#endif
    if (result != MOTOR_PROBE_OK)
    {
        // Remove drive before copying a diagnostic frame or doing UART work.
        g_motor_failure = (uint32_t)result;
        stop_all(STOP_MOTOR_PROBE);
    }
    motor_probe_trace_record(&motor_traces[motor_step], &motor_probe_state,
                             command_ccr, raw, current, result != MOTOR_PROBE_OK);
    if (result != MOTOR_PROBE_OK)
    {
        return false;
    }
#ifdef DRV_GATE_CONTINUOUS
    if (motor_continuous_state.advances!=advances_before)
    {
        motor_continuous_progress_ms=elapsed_ms;
        motor_step=3u+motor_continuous_state.sector;
        motor_probe_trace_reset(&motor_traces[motor_step]);
    }
#endif
    uint16_t compare[3];
#ifdef DRV_GATE_SMOOTH
    if (!motor_smooth_compares(&motor_continuous_state,compare))
#elif defined(DRV_GATE_CONTINUOUS)
    if (!motor_probe_pwm_compares(motor_probe_state.source,motor_probe_state.sink,motor_probe_state.duty,compare))
#else
    if (!motor_probe_pwm_compares(source,sink,motor_probe_state.duty,compare))
#endif
    {
        g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false;
    }
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (active && motor_on && stop_reason == STOP_NONE)
    {
        // Same centered phase-voltage convention as pwm.c. All gates have
        // long pulses; bounded DIFFERENCES set the stationary winding voltage.
        TIM1->CCR1 = compare[0]; TIM1->CCR2 = compare[1]; TIM1->CCR3 = compare[2];
    }
    __set_PRIMASK(mask);
    return active;
}

static bool motor_service_regular(void)
{
    const uint32_t status = ADC1->SR;
    const bool eoc = (status & ADC_SR_EOC) != 0u;
    const uint16_t value = eoc ? (uint16_t)ADC1->DR : 0u;
    const MotorRegularAction action = motor_probe_regular_step(&motor_regular, eoc,
                                                  (status & ADC_SR_OVR) != 0u, value);
    if (action == MOTOR_REG_OVERRUN || action == MOTOR_REG_MISSING_RESULT ||
        action == MOTOR_REG_UNEXPECTED_RESULT)
    {
        bus_failure = action == MOTOR_REG_OVERRUN ? 5u :
                      action == MOTOR_REG_MISSING_RESULT ? 6u : 8u;
        stop_all(STOP_BUS); return false;
    }
    if (action == MOTOR_REG_START)
    {
        // Launch just after injected completion, BEFORE the current controller.
        // No foreground phase-window polling or ADC mux changes during injection.
        const uint32_t mask = __get_PRIMASK();
        __disable_irq();
        if (!active || !sync_armed) { __set_PRIMASK(mask); return false; }
        const uint32_t count = TIM1->CNT;
        if ((TIM1->CR1 & TIM_CR1_DIR) == 0u || count < 500u || count > 1000u)
        {
            bus_failure = 7u; stop_all(STOP_BUS);
            __set_PRIMASK(mask); return false;
        }
        ADC1->SQR3 = motor_regular.channel;
        ADC1->CR2 |= ADC_CR2_SWSTART;
        __set_PRIMASK(mask);
    }
    return active;
}
#endif

void ADC_IRQHandler(void)
{
#ifdef DRV_GATE_MOTOR_TEST
    if ((ADC1->SR & ADC_SR_AWD) != 0u)
    {
#ifdef DRV_GATE_HIZ_SPI
        if (active && sync_armed && motor_on)
#else
        if (active && sync_armed)
#endif
        {
            // Preserve AWD for the stop snapshot. It is cleared only AFTER
            // ENABLE/INLs are LOW and the snapshot has captured ADC status.
            g_motor_failure = MOTOR_PROBE_RAW_CURRENT;
            stop_all(STOP_MOTOR_PROBE);
            ADC1->SR = ~ADC_SR_AWD;
            return;
        }
        ADC1->SR = ~ADC_SR_AWD;
    }
#endif
    if ((ADC1->SR & ADC_SR_JEOC) == 0u) { return; }
    const uint32_t cycle = DWT->CYCCNT;
    const uint32_t count = TIM1->CNT;
    const bool down = (TIM1->CR1 & TIM_CR1_DIR) != 0u;
    const uint16_t raw[3] = {(uint16_t)ADC1->JDR1, (uint16_t)ADC1->JDR2,
                             (uint16_t)ADC1->JDR3};
    // Write0 clears JEOC/JSTRT; write1 preserves regular EOC/OVR flags.
    ADC1->SR = ~(ADC_SR_JEOC | ADC_SR_JSTRT);
    if (!sync_armed || !active) { return; }
    GPIOC->BSRR = 1u << 4;  // IRQ entry after sequence completion, NOT sampling aperture.
    volatile SyncBank * const bank = &sync_banks[sync_active_bank];
    g_sync_last_cnt = count;
    for (unsigned int c = 0u; c < 3u; ++c) { sync_last_raw[c] = raw[c]; }
    if (!down || count <= 775u || count >= 1000u)
    {
        g_sync_failure = 2u; stop_all(STOP_SYNC_ADC); return;
    }
    if (g_sync_adc_total != 0u)
    {
        const uint32_t period = cycle - sync_last_cycle;  // Unsigned DWT wrap is intentional.
        g_sync_last_period = period;
        if (period < SYNC_PERIOD_MIN || period > SYNC_PERIOD_MAX)
        {
            g_sync_failure = 3u; stop_all(STOP_SYNC_ADC); return;
        }
        if (period < bank->period_min) { bank->period_min = period; }
        if (period > bank->period_max) { bank->period_max = period; }
    }
    if (bank->count >= SYNC_MAX_UNREPORTED)
    {
        g_sync_failure = 5u; stop_all(STOP_SYNC_ADC); return;
    }
#ifdef DRV_GATE_MOTOR_TEST
    if (!motor_service_regular()) { return; }
#endif
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        if (raw[c] < 16u || raw[c] > 4079u) { stop_all(STOP_CURRENT_ADC); return; }
        bank->stats[c].sum += raw[c];
        if (raw[c] < bank->stats[c].minimum) { bank->stats[c].minimum = raw[c]; }
        if (raw[c] > bank->stats[c].maximum) { bank->stats[c].maximum = raw[c]; }
    }
#ifdef DRV_GATE_MOTOR_TEST
    if (!motor_sample(raw)) { return; }
#endif
    if (count < bank->cnt_min) { bank->cnt_min = count; }
    if (count > bank->cnt_max) { bank->cnt_max = count; }
    ++bank->count; ++g_sync_adc_total;
    sync_last_cycle = cycle; sync_last_ms = elapsed_ms;
#ifdef DRV_GATE_MOTOR_TEST
    const uint32_t duration = DWT->CYCCNT - cycle;
    if (duration > motor_irq_cycles_max) { motor_irq_cycles_max = duration; }
    motor_irq_end_count = TIM1->CNT;
    motor_irq_end_down = (TIM1->CR1 & TIM_CR1_DIR) != 0u;
#endif
    GPIOC->BSRR = 1u << 20;
}
#endif

void SysTick_Handler(void)
{
    ++elapsed_ms;
    if (!active) { return; }
    if (fault_monitor && (GPIOB->IDR & 2u) == 0u)
    {
        stop_all(STOP_FAULT);  // Level check supplements the falling-edge IRQ.
        return;
    }
    if ((GPIOC->IDR & (1u << 13)) == 0u)
    {
        if (++user_low_ms >= 20u) { stop_all(STOP_USER); return; }
    }
    else { user_low_ms = 0u; }
#ifdef DRV_GATE_SYNC
    if (sync_armed && (uint32_t)(elapsed_ms - sync_last_ms) >= 2u)
    {
        g_sync_failure = 4u; stop_all(STOP_SYNC_ADC); return;
    }
#endif
#ifdef DRV_GATE_MOTOR_TEST
#ifdef DRV_GATE_CONTINUOUS
    if (motor_on && (uint32_t)(elapsed_ms-motor_continuous_progress_ms)>=750u)
    { g_sync_failure=8u; stop_all(STOP_SYNC_ADC); }
#else
    if (motor_on && (uint32_t)(elapsed_ms - motor_pair_start_ms) >= MOTOR_PROBE_PULSE_MS)
    {
        motor_end_pair();
    }
#endif
#endif
    if (remaining_ms != 0u && --remaining_ms == 0u) { stop_all(STOP_TIME); }
}

static void wait_ms(uint32_t duration)
{
    const uint32_t start = elapsed_ms;
    while ((uint32_t)(elapsed_ms - start) < duration) {}
}

static void safe_gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;
    GPIOB->BSRR = 1u << 16;
    GPIOB->MODER = (GPIOB->MODER & ~0xFu) | 1u;  // PB0 output, PB1 nFAULT input.
    GPIOB->OTYPER &= ~1u;
    GPIOB->PUPDR &= ~0xFu;
    GPIOA->BSRR = PWM_PINS << 16;
    GPIOA->MODER = (GPIOA->MODER & ~PWM_MODES) | PWM_GPIO;
    GPIOA->OTYPER &= ~PWM_PINS;
    GPIOA->PUPDR &= ~PWM_MODES;
#ifdef DRV_GATE_SYNC
    GPIOA->BSRR = 1u << 27;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 22)) | (1u << 22);
    GPIOA->OTYPER &= ~(1u << 11);
    GPIOA->PUPDR &= ~(3u << 22);
    GPIOC->BSRR = 1u << 20;
    GPIOC->MODER = (GPIOC->MODER & ~(3u << 8)) | (1u << 8);
    GPIOC->OTYPER &= ~(1u << 4);
    GPIOC->PUPDR &= ~(3u << 8);
#endif
    GPIOC->BSRR = 1u << 8;
    GPIOC->MODER = (GPIOC->MODER & ~(3u << 16)) | (1u << 16);
    GPIOC->OTYPER &= ~(1u << 8);
    GPIOC->PUPDR &= ~((3u << 16) | (3u << 26));
    GPIOC->MODER &= ~(3u << 26);  // USER active LOW, board external pull-up.
#ifdef DRV_GATE_3PWM
    GPIOC->BSRR = (uint32_t)INL_PINS << 16;
    GPIOC->MODER = (GPIOC->MODER & ~(uint32_t)INL_MODES) | INL_GPIO;
    GPIOC->OTYPER &= ~(uint32_t)INL_PINS;
    GPIOC->PUPDR &= ~(uint32_t)INL_MODES;
#endif
    GPIOC->MODER |= 0x3Fu;  // Existing PC0/1/2 analog wires.
    GPIOC->PUPDR &= ~0x3Fu;
    GPIOA->MODER |= 3u << 8;  // PA4 bus divider.
    GPIOA->PUPDR &= ~(3u << 8);
}

static void uart_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 4)) | (2u << 4);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFu << 8)) | (7u << 8);
    USART2->BRR = 0x01B2u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void putc_uart(char c)
{
    while ((USART2->SR & USART_SR_TXE) == 0u) {}
    USART2->DR = (uint8_t)c;
}
static void puts_uart(const char *s) { while (*s != '\0') { putc_uart(*s++); } }
static void put_uint(uint32_t value)
{
    char digits[10]; unsigned int count = 0u;
    do { digits[count++] = (char)('0' + value % 10u); value /= 10u; } while (value != 0u);
    while (count != 0u) { putc_uart(digits[--count]); }
}
static void put_hex(uint16_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    puts_uart("0x");
    putc_uart(digits[(value >> 8) & 15u]);
    putc_uart(digits[(value >> 4) & 15u]);
    putc_uart(digits[value & 15u]);
}
static void wait_button_level(bool pressed)
{
    uint32_t since = elapsed_ms;
    while ((uint32_t)(elapsed_ms - since) < 25u)
    {
        if (((GPIOC->IDR & (1u << 13)) == 0u) != pressed) { since = elapsed_ms; }
    }
}

static void timer_init(void)
{
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2) | RCC_CFGR_PPRE2_DIV4;
    RCC->DCKCFGR &= ~RCC_DCKCFGR_TIMPRE;
    __DSB();
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;
    RCC->APB2RSTR |= RCC_APB2RSTR_TIM1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM1RST;
    TIM1->CR1 = TIM_CR1_CMS_0 | TIM_CR1_ARPE;
    TIM1->PSC = 0u;
    TIM1->ARR = 1250u;  // TIM1=50MHz, center aligned20kHz.
    TIM1->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE |
                   TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE;
    TIM1->CCMR2 = TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE;
#ifdef DRV_GATE_SYNC
    TIM1->CCMR2 |= TIM_CCMR2_OC4M_0 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2 |
                    TIM_CCMR2_OC4PE;  // PWM2: OC4REF rises on the UP count.
    TIM1->CCR4 = SYNC_TRIGGER_COUNT;
    TIM1->CR2 = (TIM1->CR2 & ~TIM_CR2_MMS) | (7u << 4);  // OC4REF -> TRGO.
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~(0xFu << 12)) | (1u << 12);  // PA11 AF1.
    GPIOA->OSPEEDR |= 2u << 22;
    GPIOC->OSPEEDR |= 2u << 8;
#endif
    TIM1->BDTR = TIM_BDTR_OSSI | TIM_BDTR_OSSR;
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~0xFFFu) | 0x111u;
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~PWM_MODES) | PWM_GPIO;
    stop_all(STOP_NONE);
}

static void timer_start(void)
{
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    // A latched stop must never be undone after a fault interrupts the caller.
    if (active && stop_reason == STOP_NONE)
    {
        TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E;
        GPIOA->MODER = (GPIOA->MODER & ~PWM_MODES) | PWM_AF;
#ifdef DRV_GATE_SYNC
        TIM1->CCER |= TIM_CCER_CC4E;
        GPIOA->MODER = (GPIOA->MODER & ~(3u << 22)) | (2u << 22);
#endif
        TIM1->CR1 |= TIM_CR1_CEN;
        TIM1->BDTR |= TIM_BDTR_MOE;
    }
    __set_PRIMASK(mask);
}

static void exti_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~0xF0u) | SYSCFG_EXTICR1_EXTI1_PB;
    EXTI->IMR &= ~2u;
    EXTI->RTSR &= ~2u;
    EXTI->FTSR |= 2u;
    EXTI->PR = 2u;
    NVIC_SetPriority(EXTI1_IRQn, 0u);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

static bool shutdown_self_test(void)
{
    // Real TIM1 output enables and AF pins, but CCR=0 and ENABLE=LOW: no pulses.
    TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = 0u;
    TIM1->EGR = TIM_EGR_UG;
    stop_reason = STOP_NONE;
    remaining_ms = 1000u;
    active = true;
    timer_start();
#ifdef DRV_GATE_3PWM
    // Exercise INL high->low shutdown too, with DRV ENABLE still LOW throughout.
    GPIOC->BSRR = INL_PINS;
    wait_ms(1u);
    if ((GPIOC->IDR & INL_PINS) != INL_PINS)
    {
        stop_all(STOP_SELF_TEST); return false;
    }
#endif
    EXTI->PR = 2u;
    EXTI->IMR |= 2u;
    EXTI->SWIER = 2u;  // Exercise the actual EXTI1 vector, not a direct call.
    __DSB(); __ISB();
    const uint32_t since = elapsed_ms;
    while (active && (uint32_t)(elapsed_ms - since) < 5u) {}
    const bool passed = stop_reason == STOP_FAULT && !active &&
#ifdef DRV_GATE_SYNC
        (GPIOA->IDR & (1u << 11)) == 0u && (GPIOC->IDR & (1u << 4)) == 0u &&
#endif
#ifdef DRV_GATE_3PWM
        (GPIOC->IDR & INL_PINS) == 0u && (GPIOC->MODER & INL_MODES) == INL_GPIO &&
#endif
        (GPIOB->IDR & 1u) == 0u && (GPIOA->IDR & PWM_PINS) == 0u &&
        (GPIOA->MODER & PWM_MODES) == PWM_GPIO && TIM1->CCER == 0u &&
        (TIM1->BDTR & TIM_BDTR_MOE) == 0u && (TIM1->CR1 & TIM_CR1_CEN) == 0u;
    if (!passed) { stop_all(STOP_SELF_TEST); return false; }
#ifdef DRV_GATE_MOTOR_TEST
    TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = 0u;
#else
    TIM1->CCR1 = 250u; TIM1->CCR2 = 500u; TIM1->CCR3 = 750u;
#endif
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0u;
    stop_reason = STOP_NONE;
    remaining_ms = 0u;
    return true;
}

static void spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;
    GPIOA->MODER = (GPIOA->MODER & ~0xFC00u) | 0xA800u;  // PA5/6/7 AF5.
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~0xFFF00000u) | 0x55500000u;
    GPIOA->OSPEEDR &= ~0xFC00u;
    GPIOA->PUPDR &= ~0xFC00u;  // EVM supplies SDO pull-up; no joined rails.
    GPIOA->OTYPER &= ~((1u << 5) | (1u << 7));
    SPI1->CR1 = 0u; SPI1->CR2 = 0u;
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_CPHA |
                 SPI_CR1_DFF | SPI_CR1_BR_2 | SPI_CR1_BR_0 | SPI_CR1_SPE;
}

static bool spi_wait(uint32_t mask, bool set)
{
    const uint32_t since = elapsed_ms;
    while (((SPI1->SR & mask) != 0u) != set)
    {
        if (!active || (uint32_t)(elapsed_ms - since) >= 2u) { return false; }
    }
    return active;
}

static bool transfer(uint16_t tx, uint16_t *rx)
{
    if (!active) { return false; }
#ifdef DRV_GATE_MOTOR_TEST
    // Enforce the established motor path's quiet SPI policy. No health-poll
    // clocks may be emitted while the bounded current pulse is armed.
#ifdef DRV_GATE_HIZ_SPI
    if (motor_on || (GPIOC->IDR & INL_PINS)!=0u)
#else
    if (motor_on)
#endif
    {
        g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false;
    }
    motor_spi_last_reg = (tx >> 11) & 0xFu;
    motor_spi_active = true;
#endif
    if ((SPI1->SR & SPI_SR_RXNE) != 0u)
    {
        const volatile uint16_t stale = *(volatile uint16_t *)&SPI1->DR;
        (void)stale;
    }
    GPIOC->BSRR = 1u << 24;
    for (volatile unsigned int n = 0u; n < 50u; ++n) {}
    if (!spi_wait(SPI_SR_TXE, true)) { goto failed; }
    *(volatile uint16_t *)&SPI1->DR = tx;
    if (!spi_wait(SPI_SR_RXNE, true)) { goto failed; }
    *rx = *(volatile uint16_t *)&SPI1->DR & 0x7FFu;
    if (!spi_wait(SPI_SR_BSY, false)) { goto failed; }
    GPIOC->BSRR = 1u << 8;
    for (volatile unsigned int n = 0u; n < 100u; ++n) {}
#ifdef DRV_GATE_MOTOR_TEST
    motor_spi_active = false;
#endif
    return active;
failed:
    GPIOC->BSRR = 1u << 8;
    stop_all(STOP_SPI);
#ifdef DRV_GATE_MOTOR_TEST
    motor_spi_active = false;
#endif
    return false;
}

static bool read_reg(uint8_t address, uint16_t *value)
{
    return transfer((uint16_t)(0x8000u | ((uint16_t)address << 11)), value);
}
static bool expect_reg(uint8_t address, uint16_t expected)
{
    uint16_t first = 0u, second = 0u;
    if (!read_reg(address, &first) || !read_reg(address, &second)) { return false; }
    if (first != expected || second != expected)
    {
        stop_all(STOP_REGISTER);
        puts_uart("Register mismatch R"); put_uint(address);
        puts_uart(" expected="); put_hex(expected);
        puts_uart(" first="); put_hex(first); puts_uart(" second="); put_hex(second);
        puts_uart("\r\n"); return false;
    }
    return true;
}
static bool set_reg(uint8_t address, uint16_t value)
{
    uint16_t ignored = 0u;
    return transfer((uint16_t)(((uint16_t)address << 11) | value), &ignored) &&
           expect_reg(address, value);
}

static void adc_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;
#ifdef DRV_GATE_SYNC
    ADC->CCR = ADC_CCR_TSVREFE;  // APB2=25MHz /2=12.5MHz.
#else
    ADC->CCR = ADC_CCR_ADCPRE_0 | ADC_CCR_TSVREFE;  //25MHz/4=6.25MHz.
#endif
    ADC1->CR1 = 0u;
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EOCS;
    ADC1->SMPR1 = 7u << 21;  // VREFINT ch17,480cycles.
#ifdef DRV_GATE_ADC_CHECK
    ADC1->SMPR1 |= 7u | (7u << 3) | (7u << 6);  // PC0/1/2 ch10/11/12,480cycles.
#endif
    ADC1->SMPR2 = 7u << 12;  // PA4 ch4,480cycles.
    ADC1->SQR1 = 0u;
#ifdef DRV_GATE_SYNC
    //144 cycles=11.52us: VREFINT >=10us. Regular conversions fit between injections.
    ADC1->SMPR1 = (6u << 21) | 3u | (3u << 3) | (3u << 6);  // Current=56cycles.
    ADC1->SMPR2 = 6u << 12;  // PA4,144cycles.
    ADC1->CR1 = ADC_CR1_SCAN;
    ADC1->JSQR = (2u << 20) | (10u << 5) | (11u << 10) | (12u << 15);
    ADC1->CR2 |= ADC_CR2_JEXTSEL_0;  // TIM1_TRGO selected, trigger edge still disabled.
    ADC1->SR = 0u;
#endif
}
static bool adc_read(uint8_t channel, uint16_t *value)
{
#ifdef DRV_GATE_SYNC
#ifdef DRV_GATE_MOTOR_TEST
    if (sync_armed)
    {
        // Single foreground producer: channel is published before REQUESTED.
        // IRQ sees IDLE or a complete request, so no IRQ mask is needed here.
        const bool queued = active && motor_probe_regular_request(&motor_regular, channel);
        if (active) { bus_channel = channel; }
        if (!active) { return false; }
        if (!queued) { bus_failure = 4u; stop_all(STOP_BUS); return false; }
        const uint32_t since = elapsed_ms;
        while (active)
        {
            if (motor_probe_regular_take(&motor_regular, value)) { return active; }
            if (active && (uint32_t)(elapsed_ms - since) >= 2u)
            {
                bus_failure = 2u; stop_all(STOP_BUS); return false;
            }
        }
        return false;
    }
#else
    if (sync_armed)
    {
        // Regular bus/VREF conversions start after injected completion, in the
        // descending CNT600..100 window (38..48us). Their12.48us conversion
        // ends before the next injected trigger at67us, even at the late edge.
        const uint32_t since = elapsed_ms;
        while (active)
        {
            const uint32_t count = TIM1->CNT;
            if ((TIM1->CR1 & TIM_CR1_DIR) != 0u && count >= 100u && count <= 600u) { break; }
            if ((uint32_t)(elapsed_ms - since) >= 2u)
            {
                stop_all(STOP_BUS); return false;
            }
        }
    }
#endif
    if (!active) { return false; }
#endif
    ADC1->SQR3 = channel;
#ifdef DRV_GATE_MOTOR_TEST
    bus_channel = channel;
#endif
#ifndef DRV_GATE_SYNC
    ADC1->SR = 0u;
#endif
    // In sync mode the preceding DR read already cleared regular EOC. Never
    // clear the whole SR here: that could lose a concurrent injected JEOC.
    ADC1->CR2 |= ADC_CR2_SWSTART;
    const uint32_t since = elapsed_ms;
    while ((ADC1->SR & ADC_SR_EOC) == 0u)
    {
        if (!active || (uint32_t)(elapsed_ms - since) >= 2u)
        {
#ifdef DRV_GATE_MOTOR_TEST
            if (active) { bus_failure = 2u; }
#endif
#ifdef DRV_GATE_ADC_CHECK
            stop_all(channel >= 10u && channel <= 12u ? STOP_CURRENT_ADC : STOP_BUS);
#else
            stop_all(STOP_BUS);
#endif
            return false;
        }
    }
    *value = (uint16_t)ADC1->DR;
    return active;
}
static bool check_bus(uint32_t *bus_mv)
{
    uint32_t ref_sum = 0u, bus_sum = 0u;
#ifdef DRV_GATE_MOTOR_TEST
    bus_reads = bus_ref_sum = bus_raw_sum = 0u;
#endif
    for (unsigned int n = 0u; n < 16u; ++n)
    {
        uint16_t ref = 0u, bus = 0u;
        if (!adc_read(17u, &ref) || !adc_read(4u, &bus)) { return false; }
        ref_sum += ref; bus_sum += bus;
#ifdef DRV_GATE_MOTOR_TEST
        bus_reads = n + 1u; bus_ref_sum = ref_sum; bus_raw_sum = bus_sum;
#endif
    }
    const uint16_t cal = *(const uint16_t *)0x1FFF7A2Au;
    const uint32_t vdda = ref_sum != 0u ? (uint32_t)cal * 3300u * 16u / ref_sum : 0u;
#ifdef DRV_GATE_MOTOR_TEST
    bus_vdda_mv = vdda;
#endif
    *bus_mv = (uint32_t)((uint64_t)bus_sum * vdda * 392760u / (16ull * 4095u * 9760u));
    if (cal < 1000u || cal > 2000u || vdda < 3000u || vdda > 3600u ||
        *bus_mv < 10000u || *bus_mv > 14000u)
    {
#ifdef DRV_GATE_MOTOR_TEST
        bus_failure = 3u;
#endif
        stop_all(STOP_BUS); return false;
    }
#ifdef DRV_GATE_ADC_CHECK
    current_vdda_mv = vdda;
#endif
    return true;
}

#ifdef DRV_GATE_ADC_CHECK
static void put_thousandths(uint32_t value)
{
    put_uint(value / 1000u); putc_uart('.');
    putc_uart((char)('0' + value / 100u % 10u));
    putc_uart((char)('0' + value / 10u % 10u));
    putc_uart((char)('0' + value % 10u));
}

static bool collect_current(CurrentStats stats[3], uint32_t samples)
{
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        stats[c] = (CurrentStats){0u, 4095u, 0u};
    }
    for (uint32_t n = 0u; n < samples; ++n)
    {
        for (unsigned int c = 0u; c < 3u; ++c)
        {
            uint16_t discard = 0u, value = 0u;
            // Asynchronous observation, not a PWM-synchronous current-loop sample.
            // Discard after the mux change, then retain the second long acquisition.
            if (!adc_read((uint8_t)(10u + c), &discard) ||
                !adc_read((uint8_t)(10u + c), &value)) { return false; }
            if (value < 16u || value > 4079u)
            {
                // Gross wiring / rail check only. This is NOT an ampere trip limit.
                stop_all(STOP_CURRENT_ADC);
                puts_uart("Current ADC near rail: phase="); putc_uart((char)('A' + c));
                puts_uart(" raw="); put_uint(value); puts_uart("\r\n");
                return false;
            }
            stats[c].sum += value;
            if (value < stats[c].minimum) { stats[c].minimum = value; }
            if (value > stats[c].maximum) { stats[c].maximum = value; }
        }
    }
    return active;
}

static bool print_current(const char *label, const CurrentStats stats[3],
                          uint32_t samples, bool show_delta)
{
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        if (!active) { return false; }
        puts_uart(label); puts_uart(" ISEN"); putc_uart((char)('A' + c));
        puts_uart(" n="); put_uint(samples);
#ifdef DRV_GATE_SYNC
        puts_uart(" mean_raw=");
        put_thousandths((uint32_t)(((uint64_t)stats[c].sum * 1000u + samples / 2u) / samples));
#else
        puts_uart(" mean_raw="); put_thousandths((stats[c].sum * 1000u + samples / 2u) / samples);
#endif
        puts_uart(" min="); put_uint(stats[c].minimum);
        puts_uart(" max="); put_uint(stats[c].maximum);
        puts_uart(" mean_mV=");
        put_uint((uint32_t)(((uint64_t)stats[c].sum * current_vdda_mv +
                            (uint64_t)samples * 4095u / 2u) / ((uint64_t)samples * 4095u)));
        if (show_delta)
        {
            // Form the difference before rounding either mean. Keep 64-bit math.
            const int64_t difference = (int64_t)stats[c].sum * CURRENT_ZERO_SAMPLES -
                                       (int64_t)current_zero_sum[c] * samples;
            const int64_t divisor = (int64_t)samples * CURRENT_ZERO_SAMPLES;
            const int32_t delta = (int32_t)(difference * 1000 / divisor);
            puts_uart(" delta_raw="); putc_uart(delta < 0 ? '-' : '+');
            put_thousandths(delta < 0 ? 0u - (uint32_t)delta : (uint32_t)delta);
        }
        puts_uart("\r\n");
    }
    return active;
}

static bool capture_current_zero(uint32_t *bus_mv)
{
    // Driver awake at the verified CSA gain, COAST set, INLs LOW, PWM off.
    // Never reuse a zero from the older gain40 analog diagnostic.
    wait_ms(20u);
    CurrentStats stats[3];
    if (!active || !check_bus(bus_mv) || !collect_current(stats, CURRENT_ZERO_SAMPLES))
    {
        return false;
    }
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        if (stats[c].sum < 1800u * CURRENT_ZERO_SAMPLES ||
            stats[c].sum > 2300u * CURRENT_ZERO_SAMPLES)
        {
            stop_all(STOP_CURRENT_ADC);
            puts_uart("Current zero far from midscale: phase=");
            putc_uart((char)('A' + c)); puts_uart("\r\n"); return false;
        }
        current_zero_sum[c] = stats[c].sum;
    }
    if (!expect_reg(0u, 0u) || !expect_reg(1u, 0u) ||
        !expect_reg(2u, CTRL_COAST) || !expect_reg(6u, CSA_RUN)) { return false; }
    return print_current("ZERO_COAST", stats, CURRENT_ZERO_SAMPLES, false);
}

#ifndef DRV_GATE_SYNC
static bool report_current(void)
{
    CurrentStats stats[3];
    const uint32_t sample_start = elapsed_ms;
    if (!collect_current(stats, CURRENT_REPORT_SAMPLES)) { return false; }
    const uint32_t sample_end = elapsed_ms;
    if (!active) { return false; }
    puts_uart("ADC_BLOCK t_start_ms="); put_uint(sample_start);
    puts_uart(" t_end_ms="); put_uint(sample_end);
    puts_uart(" VDDA_mV="); put_uint(current_vdda_mv);
    puts_uart(" gain=20 R6=0x283 (asynchronous, no ampere calibration)\r\n");
    return print_current("SWITCHING", stats, CURRENT_REPORT_SAMPLES, true);
}
#else
static void clear_sync_bank(uint32_t index)
{
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        sync_banks[index].stats[c] = (CurrentStats){0u, 4095u, 0u};
    }
    sync_banks[index].count = 0u;
    sync_banks[index].period_min = UINT32_MAX; sync_banks[index].period_max = 0u;
    sync_banks[index].cnt_min = UINT32_MAX; sync_banks[index].cnt_max = 0u;
}

static bool arm_sync_adc(void)
{
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (!active || stop_reason != STOP_NONE) { __set_PRIMASK(mask); return false; }
    if (SystemCoreClock != 100000000u || (RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV4 ||
        (ADC->CCR & ADC_CCR_ADCPRE) != 0u || (ADC1->SMPR1 & 0x1FFu) != 0xDBu ||
        (ADC1->CR1 & ADC_CR1_RES) != 0u || TIM1->ARR != 1250u ||
#ifdef DRV_GATE_MOTOR_TEST
        TIM1->CCR1 != 0u || TIM1->CCR2 != 0u || TIM1->CCR3 != 0u ||
#else
        TIM1->CCR1 != 250u || TIM1->CCR2 != 500u || TIM1->CCR3 != 750u ||
#endif
        TIM1->CCR4 != SYNC_TRIGGER_COUNT || (TIM1->CR2 & TIM_CR2_MMS) != (7u << 4))
    {
        g_sync_failure = 1u; stop_all(STOP_SYNC_ADC); __set_PRIMASK(mask); return false;
    }
    clear_sync_bank(0u); clear_sync_bank(1u); sync_active_bank = 0u;
    g_sync_adc_total = 0u; g_sync_failure = 0u;
    sync_last_cycle = 0u; sync_last_ms = elapsed_ms;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    ADC1->SR = ~(ADC_SR_JEOC | ADC_SR_JSTRT);
#ifdef DRV_GATE_MOTOR_TEST
    uint32_t zero_min = 4095u, zero_max = 0u;
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        const uint32_t zero = current_zero_sum[c] / CURRENT_ZERO_SAMPLES;
        if (zero < zero_min) { zero_min = zero; }
        if (zero > zero_max) { zero_max = zero; }
    }
    const uint32_t trip = (uint32_t)((uint64_t)MOTOR_PROBE_RAW_TRIP_MA * 4095u * 40u * 7u /
                                    ((uint64_t)current_vdda_mv * 1000u));
    ADC1->LTR = zero_min > trip ? zero_min - trip : 0u;
    ADC1->HTR = zero_max + trip < 4095u ? zero_max + trip : 4095u;
    ADC1->SR = ~ADC_SR_AWD;
#ifdef DRV_GATE_HIZ_SPI
    ADC1->CR1 &= ~(ADC_CR1_JAWDEN | ADC_CR1_AWDIE);  // Hi-Z configuration/preflight.
#else
    ADC1->CR1 |= ADC_CR1_JAWDEN | ADC_CR1_AWDIE;  // Injected channels only.
#endif
#endif
    NVIC_SetPriority(ADC_IRQn, 1u);  // Below physical fault IRQ, above SysTick.
    NVIC_ClearPendingIRQ(ADC_IRQn);
    ADC1->CR1 |= ADC_CR1_JEOCIE;
    sync_armed = true;
    NVIC_EnableIRQ(ADC_IRQn);
    ADC1->CR2 |= ADC_CR2_JEXTEN_0;  // Rising OC4REF only: one sequence per PWM period.
    __set_PRIMASK(mask);
    return active;
}

#ifndef DRV_GATE_CONTINUOUS
static bool report_sync_current(void)
{
    CurrentStats snapshot[3];
    // Only the bank-index swap masks interrupts. Bulk copying and printing
    // cannot delay the completion IRQ into the next switching edge.
    const uint32_t next = sync_active_bank ^ 1u;
    clear_sync_bank(next);
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    const uint32_t previous = sync_active_bank, total = g_sync_adc_total;
    sync_active_bank = next;
    __set_PRIMASK(mask);
    const volatile SyncBank * const finished = &sync_banks[previous];
    const uint32_t samples = finished->count;
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        snapshot[c] = finished->stats[c];
    }
    if (!active) { return false; }
    if (samples == 0u)
    {
        g_sync_failure = 6u; stop_all(STOP_SYNC_ADC); return false;
    }
    puts_uart("SYNC_BLOCK n="); put_uint(samples); puts_uart(" total="); put_uint(total);
    puts_uart(" period_CPUticks_min="); put_uint(finished->period_min);
    puts_uart(" max="); put_uint(finished->period_max);
    puts_uart(" completion_down_CNT_min="); put_uint(finished->cnt_min);
    puts_uart(" max="); put_uint(finished->cnt_max);
    puts_uart(" VDDA_mV="); put_uint(current_vdda_mv);
#ifdef DRV_GATE_MOTOR_TEST
    puts_uart(" R6=0x2C3 gain=40\r\n");
#else
    puts_uart(" R6=0x283 gain=20\r\n");
#endif
    return print_current("SYNC", snapshot, samples, true);
}
#endif
#endif
#endif

static bool prepare_driver(uint32_t *bus_mv)
{
#ifdef DRV_GATE_MOTOR_TEST
    motor_stop_snapshot.valid = 0u;
    g_motor_failure = 0u;
    motor_step = 0u;
    motor_regular = (MotorProbeRegular){0};
    bus_failure = bus_channel = bus_reads = bus_ref_sum = bus_raw_sum = bus_vdda_mv = 0u;
    motor_irq_cycles_max = motor_irq_end_count = motor_irq_end_down = 0u;
    motor_spi_active = false; motor_spi_last_reg = 0u;
    for (unsigned int step = 0u; step < MOTOR_TRACE_BANKS; ++step)
    {
        motor_probe_trace_reset(&motor_traces[step]);
    }
#endif
    stop_reason = STOP_NONE;
    user_low_ms = 0u; remaining_ms = 5000u; active = true;
    if (!check_bus(bus_mv)) { return false; }  // No ENABLE before valid12V bus.
    // A USER/deadline stop interrupting the bus check must never be undone.
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (active && stop_reason == STOP_NONE) { GPIOB->BSRR = 1u; }
    __set_PRIMASK(mask);
    wait_ms(10u);  // All six inputs LOW, fault IRQ not armed during regulator wake.
    if (!active) { return false; }
    mask = __get_PRIMASK();
    __disable_irq();
    EXTI->PR = 2u;
    fault_monitor = true;
    EXTI->IMR |= 2u;
    if ((GPIOB->IDR & 2u) == 0u) { stop_all(STOP_FAULT); }
    __set_PRIMASK(mask);
    if (!active) { return false; }
    if (!expect_reg(0u, 0u) || !expect_reg(1u, 0u) || !expect_reg(2u, 0u) ||
        !expect_reg(3u, 0x3FFu) || !expect_reg(6u, 0x283u)) { return false; }
    // No automatic retries, GDF/GDUV/sense protections remain enabled.
#ifdef DRV_GATE_MOTOR_TEST
    if (!set_reg(2u, CTRL_COAST) ||
        !set_reg(3u, GATE_HS_RUN) || !set_reg(4u, GATE_LS_RUN) ||
        !set_reg(5u, MOTOR_OCP_RUN) || !set_reg(6u, CSA_RUN)) { return false; }
#else
    if (!set_reg(2u, CTRL_COAST) || !set_reg(3u, 0x300u) ||
        !set_reg(4u, 0x300u) ||
        !set_reg(5u, 0x315u)) { return false; }
#endif
    // R2=selected PWM mode, linked OCP shutdown+OTW report+COAST.
    // Motor uses the repo's150/300mA gates and100ns deadtime; TDRIVE4us.
    // Retain probe's latched OCP,2us deglitch,VDS0.2V. Older scopes unchanged.
    puts_uart("Config read twice: R2="); put_hex(CTRL_COAST);
#ifdef DRV_GATE_MOTOR_TEST
    puts_uart(" R3=0x333 R4=0x333 R5=0x115 R6=0x2C3\r\n");
#else
    puts_uart(" R3=0x300 R4=0x300 R5=0x315 R6=0x283\r\n");
#endif
    if (!expect_reg(0u, 0u) || !expect_reg(1u, 0u) || !active) { return false; }
#ifdef DRV_GATE_ADC_CHECK
    if (!capture_current_zero(bus_mv)) { return false; }
#endif
    remaining_ms = WINDOW_MS;
    timer_start();  // Start logic PWM with COAST still set.
#ifdef DRV_GATE_MOTOR_TEST
    // All phase compares are ZERO and all INLs remain LOW. Prove the ADC
    // timing/freshness path before allowing any differential motor voltage.
    if (!arm_sync_adc()) { return false; }
    wait_ms(10u);
    if (!active) { return false; }
    const uint32_t preflight_sequences = g_sync_adc_total;
    if (preflight_sequences < 160u || preflight_sequences > 240u)
    {
        g_sync_failure = 6u; stop_all(STOP_SYNC_ADC); return false;
    }
    puts_uart("PREFLIGHT PASS: zero drive, fresh ADC sequences="); put_uint(preflight_sequences);
    puts_uart("\r\n");
    if (!check_bus(bus_mv)) { return false; }
    puts_uart("BUS PREFLIGHT PASS: IRQ-queued VREF/bus reads at ZERO drive.\r\n");
    return active;
#else
#ifdef DRV_GATE_3PWM
    mask = __get_PRIMASK();
    __disable_irq();
    if (active && stop_reason == STOP_NONE) { GPIOC->BSRR = INL_PINS; }
    __set_PRIMASK(mask);
    wait_ms(1u);  // Allow pin/pad settling while COAST remains set.
    if (!active) { return false; }
    if ((GPIOC->IDR & INL_PINS) != INL_PINS)
    {
        stop_all(STOP_REGISTER);
        puts_uart("INL enable GPIO level check failed\r\n"); return false;
    }
#endif
    if (!set_reg(2u, CTRL_RUN)) { return false; }
#ifdef DRV_GATE_SYNC
    if (!arm_sync_adc()) { return false; }
#endif
    return active;
#endif
}

#ifdef DRV_GATE_MOTOR_TEST
static void put_signed_ma(int32_t value)
{
    if (value < 0) { putc_uart('-'); put_uint(0u - (uint32_t)value); }
    else { put_uint((uint32_t)value); }
}

#ifdef DRV_GATE_SPIN_TEST
static void report_motor_spin(void)
{
#ifdef DRV_GATE_CONTINUOUS
    puts_uart("CONTINUOUS_RESULT aligned="); put_uint(motor_continuous_state.aligned);
    puts_uart(" sector="); put_uint(motor_continuous_state.sector);
    puts_uart(" field_advances=");
#ifdef DRV_GATE_SMOOTH
    put_uint(motor_continuous_state.advances==0u ? 0u : motor_continuous_state.advances-1u);
#else
    put_uint(motor_continuous_state.advances);
#endif
    puts_uart(" run_ms="); put_uint(motor_pair_start_ms==0u ? 0u : stop_ms-motor_pair_start_ms);
    puts_uart(" shaft_tracking=UNMEASURED\r\n");
#else
    for (uint32_t index=0u; index<MOTOR_SEQUENCE_STEPS; ++index)
    {
        const MotorSpinSummary * const summary=&motor_spin_summaries[index];
        if (summary->valid==0u) { continue; }
        MotorProbeStep step;
        if (!motor_probe_sequence_step(index,&step)) { continue; }
        puts_uart("STEP DONE index="); put_uint(index);
        puts_uart(step.rotating ? " stage=CRAWL pair=" : " stage=REVERSE pair=");
        putc_uart((char)('A'+step.source)); puts_uart("->"); putc_uart((char)('A'+step.sink));
        puts_uart(" nominal_filtered_mA=");
        for (unsigned int c=0u;c<3u;++c)
        {
            if (c!=0u) { putc_uart('/'); }
            put_signed_ma(summary->filtered_ma[c]);
        }
        puts_uart(" diff_CCR="); put_uint(summary->differential);
        puts_uart(" peak_raw_mA="); put_uint(summary->peak);
        puts_uart(" target_samples="); put_uint(summary->targets);
        puts_uart(" pulse_ms="); put_uint(summary->end_ms-summary->start_ms);
        puts_uart("\r\n");
    }
    puts_uart("SPIN_RESULT reverse_passed="); put_uint(motor_reverse_passed);
    puts_uart(" field_holds_completed="); put_uint(motor_spin_holds);
    puts_uart(" field_advances="); put_uint(motor_spin_holds==0u ? 0u : motor_spin_holds-1u);
    puts_uart(" field_elapsed_ms=");
    put_uint(motor_spin_holds>1u ? motor_field_end_ms-motor_field_start_ms : 0u);
    puts_uart(" pole_pairs=4 shaft_tracking=UNMEASURED\r\n");
#endif
}
#endif

static void report_motor_feedback(void)
{
    // Called once, after stop_all has disabled ENABLE, INLs, PWM and ADC IRQ.
#ifdef DRV_GATE_TORQUE_700MA
    puts_uart("FEEDBACK BEGIN target_mA=700 VDDA_mV="); put_uint(current_vdda_mv);
#elif defined(DRV_GATE_CRAWL_350MA)
    puts_uart("FEEDBACK BEGIN target_mA=350 VDDA_mV="); put_uint(current_vdda_mv);
#elif defined(DRV_GATE_CRAWL_250MA)
    puts_uart("FEEDBACK BEGIN target_mA=250 VDDA_mV="); put_uint(current_vdda_mv);
#else
    puts_uart("FEEDBACK BEGIN target_mA=100 VDDA_mV="); put_uint(current_vdda_mv);
#endif
#ifdef DRV_GATE_SMOOTH
    puts_uart(" sample_period_us=50 history_interval_us=10000\r\n");
#else
    puts_uart(" sample_period_us=50 history_interval_us=1000\r\n");
#endif
    puts_uart("BUS_CHECK failure="); put_uint(bus_failure);
    puts_uart(" channel="); put_uint(bus_channel);
    puts_uart(" queue_state="); put_uint((uint32_t)motor_regular.state);
    puts_uart(" ref_bus_pairs="); put_uint(bus_reads);
    puts_uart(" ref_sum="); put_uint(bus_ref_sum);
    puts_uart(" bus_sum="); put_uint(bus_raw_sum);
    puts_uart(" VDDA_last_mV="); put_uint(bus_vdda_mv);
    puts_uart("\r\nIRQ_WORK max_CPUticks="); put_uint(motor_irq_cycles_max);
    puts_uart(" last_end_CNT="); put_uint(motor_irq_end_count);
    puts_uart(" last_end_down="); put_uint(motor_irq_end_down);
    puts_uart("\r\n");
    if (motor_stop_snapshot.valid != 0u)
    {
        puts_uart("ADC_STOP after_disable_may_partial=1 SR="); put_hex((uint16_t)motor_stop_snapshot.adc_sr);
        puts_uart(" AWD="); put_uint((motor_stop_snapshot.adc_sr & ADC_SR_AWD) != 0u);
        puts_uart(" JEOC="); put_uint((motor_stop_snapshot.adc_sr & ADC_SR_JEOC) != 0u);
        puts_uart(" LTR="); put_uint(motor_stop_snapshot.lower);
        puts_uart(" HTR="); put_uint(motor_stop_snapshot.upper);
        puts_uart(" raw=");
        for (unsigned int c = 0u; c < 3u; ++c)
        {
            if (c != 0u) { putc_uart('/'); }
            put_uint(motor_stop_snapshot.raw[c]);
        }
        puts_uart(" nominal_mA=");
        for (unsigned int c = 0u; c < 3u; ++c)
        {
            if (c != 0u) { putc_uart('/'); }
            if (current_vdda_mv == 0u) { puts_uart("NA"); }
            else { put_signed_ma(motor_probe_current_ma(motor_stop_snapshot.raw[c],
                                                       current_zero_sum[c], current_vdda_mv)); }
        }
        puts_uart(" CNT="); put_uint(motor_stop_snapshot.count);
        puts_uart(" down="); put_uint(motor_stop_snapshot.down);
        puts_uart(" CCR=");
        for (unsigned int c = 0u; c < 3u; ++c)
        {
            if (c != 0u) { putc_uart('/'); }
            put_uint(motor_stop_snapshot.command_ccr[c]);
        }
        puts_uart(" samples="); put_uint(motor_stop_snapshot.samples);
        puts_uart(" pair="); putc_uart((char)('A' + motor_stop_snapshot.source));
        puts_uart("->"); putc_uart((char)('A' + motor_stop_snapshot.sink));
        puts_uart(" pulse_was_on="); put_uint(motor_stop_snapshot.motor_was_on);
        puts_uart(" t_ms="); put_uint(motor_stop_snapshot.time_ms);
        puts_uart(" spi_active="); put_uint(motor_stop_snapshot.spi_active);
        puts_uart(" spi_busy="); put_uint(motor_stop_snapshot.spi_busy);
        puts_uart(" spi_cs_low="); put_uint(motor_stop_snapshot.spi_cs_low);
        puts_uart(" spi_last_reg="); put_uint(motor_stop_snapshot.spi_last_reg);
        puts_uart("\r\n");
    }
#ifdef DRV_GATE_SPIN_TEST
    report_motor_spin();
    char source[MOTOR_TRACE_BANKS],sink[MOTOR_TRACE_BANKS];
    for (uint32_t bank=0u;bank<MOTOR_TRACE_BANKS;++bank)
    {
        MotorProbeStep step;
        (void)motor_probe_sequence_step(bank,&step);
        source[bank]=(char)('A'+step.source); sink[bank]=(char)('A'+step.sink);
    }
#else
    static const char source[3] = {'A','B','C'}, sink[3] = {'B','C','A'};
#endif
    for (unsigned int step = 0u; step < MOTOR_TRACE_BANKS; ++step)
    {
        const MotorProbeTrace * const trace = &motor_traces[step];
        if (trace->samples == 0u) { continue; }
        puts_uart("FEEDBACK_PAIR "); putc_uart(source[step]); puts_uart("->"); putc_uart(sink[step]);
        puts_uart(" completed_samples="); put_uint(trace->samples);
        puts_uart(" raw_min=");
        for (unsigned int c = 0u; c < 3u; ++c)
        {
            if (c != 0u) { putc_uart('/'); }
            put_uint(trace->raw_min[c]);
        }
        puts_uart(" raw_max=");
        for (unsigned int c = 0u; c < 3u; ++c)
        {
            if (c != 0u) { putc_uart('/'); }
            put_uint(trace->raw_max[c]);
        }
        puts_uart("\r\n");
        for (uint32_t index = 0u; index < trace->count; ++index)
        {
#ifdef DRV_GATE_SPIN_TEST
            // Full reverse traces, nine tail frames per final crawl sector.
            if (step>=3u && index%4u!=0u && index+1u<trace->count) { continue; }
#endif
            const MotorProbeFrame * const frame = motor_probe_trace_frame(trace, index);
            puts_uart("FB "); putc_uart(source[step]); puts_uart("->"); putc_uart(sink[step]);
            puts_uart(" t_us="); put_uint(frame->sample * 50u);
            puts_uart(" diff_CCR="); put_uint(frame->command_ccr);
#ifdef DRV_GATE_SMOOTH
            puts_uart(" reference_mA=");
            for (unsigned int c=0u;c<3u;++c)
            { if (c!=0u) { putc_uart('/'); } put_signed_ma(frame->reference_ma[c]); }
#endif
            puts_uart(" raw=");
            for (unsigned int c = 0u; c < 3u; ++c)
            {
                if (c != 0u) { putc_uart('/'); }
                put_uint(frame->raw[c]);
            }
            puts_uart(" nominal_mA=");
            for (unsigned int c = 0u; c < 3u; ++c)
            {
                if (c != 0u) { putc_uart('/'); }
                put_signed_ma(frame->nominal_ma[c]);
            }
            puts_uart(" filtered_mA=");
            for (unsigned int c = 0u; c < 3u; ++c)
            {
                if (c != 0u) { putc_uart('/'); }
                put_signed_ma(frame->filtered_ma[c]);
            }
            puts_uart("\r\n");
        }
    }
    puts_uart("FEEDBACK END\r\n");
}

static bool motor_registers_ok(void)
{
    return expect_reg(0u,0u) && expect_reg(1u,0u) && expect_reg(2u,CTRL_RUN) &&
           expect_reg(3u,GATE_HS_RUN) && expect_reg(4u,GATE_LS_RUN) &&
           expect_reg(5u,MOTOR_OCP_RUN) && expect_reg(6u,CSA_RUN);
}

#ifdef DRV_GATE_HIZ_SPI
static bool motor_arm_after_quiet_spi(void)
{
    // Already Hi-Z. Drain SPI-induced settling before arming at4..8us of a
    // PWM cycle, all INHs HIGH and >=9us before the next injected trigger.
    wait_ms(1u);
    const uint32_t since=elapsed_ms;
    while (active)
    {
        const uint32_t mask=__get_PRIMASK(); __disable_irq();
        const uint32_t count=TIM1->CNT;
        if (active && stop_reason==STOP_NONE && (TIM1->CR1 & TIM_CR1_DIR)==0u &&
            count>=200u && count<=400u)
        {
            if ((GPIOC->IDR & INL_PINS)!=0u || motor_spi_active ||
                (SPI1->SR & SPI_SR_BSY)!=0u || (GPIOC->IDR & (1u<<8))==0u ||
                (GPIOA->IDR & PWM_PINS)!=PWM_PINS || !sync_armed)
            {
                g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE);
                __set_PRIMASK(mask); return false;
            }
            if ((GPIOB->IDR & 2u)==0u)
            { stop_all(STOP_FAULT); __set_PRIMASK(mask); return false; }
            ADC1->SR = ~ADC_SR_AWD;
            ADC1->CR1 |= ADC_CR1_JAWDEN | ADC_CR1_AWDIE;
            GPIOC->BSRR=INL_PINS;  // Watchdog ON before any bridge enable.
#ifdef DRV_GATE_CONTINUOUS
            motor_continuous_progress_ms=elapsed_ms;
#endif
            motor_pair_start_ms=elapsed_ms; motor_pair_done=false; motor_on=true;
            __set_PRIMASK(mask); return true;
        }
        __set_PRIMASK(mask);
        if (active && (uint32_t)(elapsed_ms-since)>=2u)
        { g_sync_failure=7u; stop_all(STOP_SYNC_ADC); return false; }
    }
    return false;
}
#endif

#ifdef DRV_GATE_SPIN_TEST
static void motor_spin_discard_sync_block(void)
{
    // Keep the unreported-bank guard bounded without serial pauses in the crawl.
    const uint32_t next=sync_active_bank^1u;
    clear_sync_bank(next);
    const uint32_t mask=__get_PRIMASK(); __disable_irq();
    sync_active_bank=next;
    __set_PRIMASK(mask);
}

#ifdef DRV_GATE_CONTINUOUS
static bool run_motor_probe(uint32_t *bus_mv)
{
    if (!motor_continuous_reset(&motor_continuous_state,&motor_probe_state))
    { g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false; }
    motor_step=3u;
    motor_probe_trace_reset(&motor_traces[motor_step]);
    const uint32_t mask=__get_PRIMASK(); __disable_irq();
    if (active && stop_reason==STOP_NONE)
    { TIM1->CCR1=TIM1->CCR2=TIM1->CCR3=MOTOR_PWM_CENTER; }
    __set_PRIMASK(mask);
    wait_ms(1u);
    if (!active || !set_reg(2u,CTRL_RUN) || !motor_registers_ok()) { return false; }
#ifdef DRV_GATE_SMOOTH
#ifdef DRV_GATE_TORQUE_700MA
    puts_uart("RUN TORQUE:700mA,sharedrepoPI,slowcontinuousfullturnfield~10rpm. USER or fault stops.\r\n");
#else
    puts_uart("RUN SMOOTH:350mA,signed3phasecurrentblend,continuousfullturnfield~10rpm. USER or fault stops.\r\n");
#endif
#else
    puts_uart("RUN CONTINUOUS:350mA,briefalignmentthenroughly10rpmfield. USER or fault stops.\r\n");
#endif
    if (!motor_arm_after_quiet_spi()) { return false; }
    uint32_t last_bus=elapsed_ms,last_stats=elapsed_ms;
    while (active)
    {
        const uint32_t now=elapsed_ms;
        if ((uint32_t)(now-last_bus)>=25u)
        { if (!check_bus(bus_mv)) { return false; } last_bus=now; }
        if ((uint32_t)(now-last_stats)>=1000u)
        { motor_spin_discard_sync_block(); last_stats=now; }
        __WFI();
    }
    return false;  // Expected normal exit is USER stop, not a scheduled completion.
}
#else
static bool run_motor_probe(uint32_t *bus_mv)
{
    puts_uart("REVERSE CHECK: B->A, C->B, A->C; crawl only after all three pass.\r\n");
    for (uint32_t index=0u;index<MOTOR_SEQUENCE_STEPS && active;++index)
    {
        MotorProbeStep step;
        if (!motor_probe_sequence_step(index,&step))
        { g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false; }
        if (step.rotating && motor_reverse_passed!=MOTOR_SEQUENCE_REVERSE)
        { g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false; }
        motor_end_pair();
        if (!active || !set_reg(2u,CTRL_COAST)) { return false; }
        wait_ms(step.coast_ms);
        if (!active || !check_bus(bus_mv)) { return false; }
        if (!motor_probe_reset(&motor_probe_state,step.source,step.sink))
        { g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false; }
        motor_step=step.trace_bank;
        motor_probe_trace_reset(&motor_traces[motor_step]);
        if (!step.rotating)
        {
            puts_uart("REVERSE START "); putc_uart((char)('A'+step.source));
            puts_uart("->"); putc_uart((char)('A'+step.sink));
#ifdef DRV_GATE_CRAWL_350MA
            puts_uart(" target_mA=350\r\n");
#elif defined(DRV_GATE_CRAWL_250MA)
            puts_uart(" target_mA=250\r\n");
#else
            puts_uart(" target_mA=100\r\n");
#endif
        }
        else if (index==MOTOR_SEQUENCE_REVERSE)
        {
#ifdef DRV_GATE_CRAWL_350MA
            puts_uart("REVERSE PASS. CRAWL START: align then24 field advances,350mA, roughly10rpm field.\r\n");
#elif defined(DRV_GATE_CRAWL_250MA)
            puts_uart("REVERSE PASS. CRAWL START: align then24 field advances,250mA, roughly10rpm field.\r\n");
#else
            puts_uart("REVERSE PASS. CRAWL START: align then24 field advances,100mA, roughly10rpm field.\r\n");
#endif
        }
        uint32_t mask=__get_PRIMASK(); __disable_irq();
        if (active && stop_reason==STOP_NONE)
        {
            TIM1->CCR1=TIM1->CCR2=TIM1->CCR3=MOTOR_PWM_CENTER;
#ifndef DRV_GATE_HIZ_SPI
            GPIOC->BSRR=INL_PINS;
#endif
        }
        __set_PRIMASK(mask);
        wait_ms(1u);
        if (!active) { return false; }
#ifdef DRV_GATE_HIZ_SPI
        if ((GPIOC->IDR & INL_PINS)!=0u || TIM1->CCR1!=MOTOR_PWM_CENTER ||
#else
        if ((GPIOC->IDR & INL_PINS)!=INL_PINS || TIM1->CCR1!=MOTOR_PWM_CENTER ||
#endif
            TIM1->CCR2!=MOTOR_PWM_CENTER || TIM1->CCR3!=MOTOR_PWM_CENTER)
        { g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false; }
        if (!set_reg(2u,CTRL_RUN) || !motor_registers_ok()) { return false; }
#ifdef DRV_GATE_HIZ_SPI
        if (!motor_arm_after_quiet_spi()) { return false; }
#else
        mask=__get_PRIMASK(); __disable_irq();
        if (active && stop_reason==STOP_NONE)
        {
            motor_pair_start_ms=elapsed_ms; motor_pair_done=false; motor_on=true;
        }
        __set_PRIMASK(mask);
#endif
        uint32_t last_bus=elapsed_ms;
        while (active && !motor_pair_done)
        {
            const uint32_t now=elapsed_ms;
            if ((uint32_t)(now-last_bus)>=25u)
            { if (!check_bus(bus_mv)) { return false; } last_bus=now; }
        }
        if (!active || !motor_registers_ok() || !set_reg(2u,CTRL_COAST)) { return false; }
        if (!motor_probe_target_seen(&motor_probe_state))
        { g_motor_failure=MOTOR_PROBE_TARGET_MISSED; stop_all(STOP_MOTOR_PROBE); return false; }
        MotorSpinSummary * const summary=&motor_spin_summaries[index];
        summary->start_ms=motor_pair_start_ms; summary->end_ms=motor_pair_end_ms;
        summary->targets=motor_probe_state.target_samples;
        summary->peak=motor_probe_state.peak_raw_ma;
        summary->differential=(motor_probe_state.duty/2u)*2u;
        for (unsigned int c=0u;c<3u;++c) { summary->filtered_ma[c]=motor_probe_state.filtered_q8[c]/256; }
        summary->valid=1u;
        if (step.rotating)
        {
            if (motor_spin_holds==0u) { motor_field_start_ms=motor_pair_end_ms; }
            ++motor_spin_holds; motor_field_end_ms=motor_pair_end_ms;
            motor_spin_discard_sync_block();
        }
        else
        {
            ++motor_reverse_passed;
            puts_uart("REVERSE DONE "); putc_uart((char)('A'+step.source));
            puts_uart("->"); putc_uart((char)('A'+step.sink)); puts_uart(" target recognized\r\n");
            if (!report_sync_current()) { return false; }
        }
    }
    if (!active) { return false; }
    stop_all(STOP_COMPLETE); return true;
}
#endif
#else
static bool run_motor_probe(uint32_t *bus_mv)
{
    static const uint8_t source[3] = {0u,1u,2u}, sink[3] = {1u,2u,0u};
    for (unsigned int step=0u; step<3u && active; ++step)
    {
        // Every transition starts Hi-Z/COAST, with compare preloads at ZERO.
        motor_end_pair();
        if (!active || !set_reg(2u,CTRL_COAST)) { return false; }
        wait_ms(100u);
        if (!active || !check_bus(bus_mv)) { return false; }
        if (!motor_probe_reset(&motor_probe_state,source[step],sink[step]))
        {
            g_motor_failure=MOTOR_PROBE_CONFIG; stop_all(STOP_MOTOR_PROBE); return false;
        }
        motor_step = step;
        puts_uart("PAIR START "); putc_uart((char)('A'+source[step])); puts_uart("->");
        putc_uart((char)('A'+sink[step]));
        puts_uart(" target_mA=100 max_diff_CCR=24 center_CCR=625 duration_ms=250\r\n");
        uint32_t mask=__get_PRIMASK();
        __disable_irq();
        if (active && stop_reason==STOP_NONE)
        {
            TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = MOTOR_PWM_CENTER;
            GPIOC->BSRR=INL_PINS;
        }
        __set_PRIMASK(mask);
        wait_ms(1u);  // Still COAST and ZERO differential drive.
        if (!active) { return false; }
        if ((GPIOC->IDR & INL_PINS)!=INL_PINS || TIM1->CCR1!=MOTOR_PWM_CENTER ||
            TIM1->CCR2!=MOTOR_PWM_CENTER || TIM1->CCR3!=MOTOR_PWM_CENTER)
        {
            stop_all(STOP_REGISTER); return false;
        }
        if (!set_reg(2u,CTRL_RUN) || !motor_registers_ok()) { return false; }
        mask=__get_PRIMASK(); __disable_irq();
        if (active && stop_reason==STOP_NONE)
        {
            motor_pair_start_ms=elapsed_ms; motor_pair_done=false; motor_on=true;
        }
        __set_PRIMASK(mask);
        uint32_t last_bus=elapsed_ms;
        while (active && !motor_pair_done)
        {
            const uint32_t now=elapsed_ms;
            if ((uint32_t)(now-last_bus)>=25u)
            {
                if (!check_bus(bus_mv)) { return false; }
                last_bus=now;
            }
        }
        // SysTick already forced INLs LOW. UART cannot extend the pulse.
        if (!active || !motor_registers_ok() || !set_reg(2u,CTRL_COAST)) { return false; }
        puts_uart("PAIR DONE "); putc_uart((char)('A'+source[step])); puts_uart("->");
        putc_uart((char)('A'+sink[step])); puts_uart(" nominal_filtered_mA=");
        for (unsigned int c=0u;c<3u;++c)
        {
            if (c!=0u) { putc_uart('/'); }
            put_signed_ma(motor_probe_state.filtered_q8[c]/256);
        }
        puts_uart(" last_diff_CCR="); put_uint((motor_probe_state.duty/2u)*2u);
        puts_uart(" peak_raw_mA="); put_uint(motor_probe_state.peak_raw_ma);
        puts_uart(" target_samples="); put_uint(motor_probe_state.target_samples);
        puts_uart("\r\n");
        if (!motor_probe_target_seen(&motor_probe_state))
        {
            g_motor_failure=MOTOR_PROBE_TARGET_MISSED; stop_all(STOP_MOTOR_PROBE); return false;
        }
        if (!report_sync_current()) { return false; }
    }
    if (!active) { return false; }
    stop_all(STOP_COMPLETE);
    return true;
}
#endif
#endif

int main(void)
{
    safe_gpio_init();
#ifdef DRV_GATE_TORQUE_700MA
    // Same FPU access setup as the working main.c, before shared float PI use.
    SCB->CPACR |= ((3UL << (10u * 2u)) | (3UL << (11u * 2u)));
    __DSB(); __ISB();
#endif
    clock_init(); uart_init(); timer_init(); exti_init(); spi_init();
    SysTick->LOAD = SystemCoreClock / 1000u - 1u;
    SysTick->VAL = 0u;
    NVIC_SetPriority(SysTick_IRQn, 2u);
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    adc_init(); wait_ms(2u);
#ifdef DRV_GATE_TORQUE_700MA
    // Exercise the actual shared PI on the MCU while ENABLE/INLs remain LOW.
    PIState pi_check;
    pi_init(&pi_check,0.25f,4.0f,-2.0f,2.0f);
    if (pi_step(&pi_check,2.0f,0.125f)!=1.5f)
    {
        stop_all(STOP_SELF_TEST);
        puts_uart("Shared repo PI/FPU self-test FAILED; ENABLE LOW.\r\n");
        while (1) { __WFI(); }
    }
    puts_uart("Shared repo PI/FPU self-test: PASS (driver disabled).\r\n");
#endif
#ifdef DRV_GATE_MOTOR_TEST
#ifdef DRV_GATE_SPIN_TEST
#ifdef DRV_GATE_CONTINUOUS
#ifdef DRV_GATE_SMOOTH
#ifdef DRV_GATE_TORQUE_700MA
    puts_uart("\r\nDiagnostic: DRV8353RS SLOW TORQUE v3 (700mA, shared repo PI, Hi-Z SPI)\r\n");
#else
    puts_uart("\r\nDiagnostic: DRV8353RS SMOOTH continuous rotation v2 (350mA, Hi-Z SPI)\r\n");
#endif
#else
    puts_uart("\r\nDiagnostic: DRV8353RS CONTINUOUS slow rotation v1 (350mA, Hi-Z SPI)\r\n");
#endif
#ifdef DRV_GATE_TORQUE_700MA
    puts_uart("Target700mA,max8% differential. Same slow10rpm FIELD; no timed motion stop.\r\n");
#else
    puts_uart("Target350mA,max4.48% differential. Continuous until USER or protection stop.\r\n");
#endif
#ifdef DRV_GATE_SMOOTH
    puts_uart("Initial0.5s alignment,then gradual3phasecurrent blends;fullFIELD turn every6s(~10rpm).\r\n");
    puts_uart("Signed per-phase integral control;common-mode rejection;sum-current consistency guard150mA.\r\n");
#else
    puts_uart("Initial0.5s alignment,then60deg electrical advances every250ms,roughly10rpm FIELD.\r\n");
#endif
#ifdef DRV_GATE_SMOOTH
    puts_uart("References update every50us; no60deg jumps,coast gaps,or finite run limit.\r\n");
#else
    puts_uart("No programmed coast gaps,no current ramp reset at commutation,no finite run limit.\r\n");
#endif
#elif defined(DRV_GATE_CRAWL_350MA)
    puts_uart("\r\nDiagnostic: DRV8353RS reverse-and-stepped-crawl v4 (350mA, Hi-Z SPI)\r\n");
    puts_uart("Reverse3 pairs, then align+24 field advances. Target350mA, max4.48% differential.\r\n");
#elif defined(DRV_GATE_CRAWL_250MA)
#ifdef DRV_GATE_HIZ_SPI
    puts_uart("\r\nDiagnostic: DRV8353RS reverse-and-stepped-crawl v3 (250mA, Hi-Z SPI)\r\n");
#else
    puts_uart("\r\nDiagnostic: DRV8353RS reverse-and-stepped-crawl v2 (250mA)\r\n");
#endif
    puts_uart("Reverse3 pairs, then align+24 field advances. Target250mA, max3.2% differential.\r\n");
#else
    puts_uart("\r\nDiagnostic: DRV8353RS reverse-and-stepped-crawl v1\r\n");
    puts_uart("Reverse3 pairs, then align+24 field advances. Target100mA, max1.92% differential.\r\n");
#endif
#ifdef DRV_GATE_CONTINUOUS
    puts_uart("Actual shaft speed/following unmeasured. Secure unloaded motor; keep hands clear.\r\n");
#else
    puts_uart("First rotation attempt: roughlyone shaft turn in6..7sec IF rotor follows; no shaft feedback.\r\n");
    puts_uart("Expect a stepped crawl, not smooth speed regulation. Do not restrain or hand-start shaft.\r\n");
#endif
#else
    puts_uart("\r\nDiagnostic: DRV8353RS bounded MOTOR current probe v5\r\n");
    puts_uart("Three250ms stationary phase-pair pulses. Target100mA, max1.92% differential PWM.\r\n");
#endif
#ifdef DRV_GATE_TORQUE_700MA
    puts_uart("Fresh gain40 zeros, nominal7mOhm scaling. Filtered1000mA/raw2000mA sampled trips.\r\n");
#else
    puts_uart("Fresh gain40 zeros, nominal7mOhm scaling. Filtered500mA/raw1000mA sampled trips.\r\n");
#endif
#ifdef DRV_GATE_SPIN_TEST
#ifdef DRV_GATE_CONTINUOUS
#ifdef DRV_GATE_TORQUE_700MA
    puts_uart("Repo-centered PWM:leg46..54%. Bridge remains enabled across field advances.\r\n");
#else
    puts_uart("Repo-centered PWM:leg47.76..52.24%. Bridge remains enabled across field advances.\r\n");
#endif
#elif defined(DRV_GATE_CRAWL_350MA)
    puts_uart("Repo-centered PWM: leg47.76..52.24%;250ms per step, brief COAST gaps, auto-stop.\r\n");
#elif defined(DRV_GATE_CRAWL_250MA)
    puts_uart("Repo-centered PWM: leg48.4..51.6%;250ms per step, brief COAST gaps, auto-stop.\r\n");
#else
    puts_uart("Repo-centered PWM: leg49.04..50.96%;250ms per step, brief COAST gaps, auto-stop.\r\n");
#endif
#else
    puts_uart("Repo-centered PWM: leg duty49.04..50.96%, all INLs enabled during pulse. No rotation command.\r\n");
#endif
#ifdef DRV_GATE_TORQUE_700MA
    puts_uart("Gate source150mA/sink300mA,100ns deadtime; current trips1000mA filtered/2000mA sampled raw.\r\n");
#else
    puts_uart("Gate source150mA/sink300mA,100ns deadtime; sampled current trips unchanged.\r\n");
#endif
    puts_uart("Motor may twitch. Secure unloaded motor. Hall leads stay disconnected.\r\n");
    puts_uart("Same INH/INL wiring. Current-loop preflight runs at ZERO drive before any pulse.\r\n");
    puts_uart("Feedback history and ADC stop snapshot print AFTER shutdown; no scope required for this run.\r\n");
    puts_uart("Bus ADC requests launch from the injected IRQ; BUS_CHECK reports exact failure class.\r\n");
#ifdef DRV_GATE_CONTINUOUS
    puts_uart("SPI verified before starting; NO SPI or UART traffic during continuous operation.\r\n");
#else
    puts_uart("SPI register checks before/after each pulse; NO SPI clocks during current pulses.\r\n");
#endif
#ifdef DRV_GATE_HIZ_SPI
    puts_uart("ALL SPI requires INLs LOW.1ms quiet settling; raw watchdog armed BEFORE INLs HIGH.\r\n");
#endif
#else
#ifdef DRV_GATE_3PWM
#ifdef DRV_GATE_ADC_CHECK
#ifdef DRV_GATE_SYNC
    puts_uart("\r\nDiagnostic: DRV8353RS THREE-PWM synchronous current ADC v1 (NO MOTOR)\r\n");
    puts_uart("20k sequences/s: TIM1_CH4 rising at up-CNT850; ADC12.5MHz,56cycle acquisitions.\r\n");
    puts_uart("Scope PA11 CN10-14=hardware trigger; PC4 CN10-34=completion IRQ pulse.\r\n");
    puts_uart("Three sequential samples, nominal16.32us total. Markers are NOT simultaneous apertures.\r\n");
#else
    puts_uart("\r\nDiagnostic: DRV8353RS THREE-PWM current ADC v1 (NO MOTOR)\r\n");
    puts_uart("PC0/1/2 ISENA/B/C: gain20, fresh COAST zeros, then 128 samples/channel/report.\r\n");
    puts_uart("Raw/mV observation only. Not PWM-synchronous sampling or calibrated current protection.\r\n");
#endif
#else
    puts_uart("\r\nDiagnostic: DRV8353RS THREE-PWM bridge scope v1 (NO MOTOR)\r\n");
#endif
    puts_uart("Keep PA8/9/10 on INHA/B/C J2-2/6/10. NO MOTOR/HALL leads or INL high ties.\r\n");
    puts_uart("ADD PC6 CN10-4->J2-4 INLA; PC7 CN10-19->J2-8 INLB; PC9 CN10-1->J2-12 INLC.\r\n");
    puts_uart("3PWM20kHz: INHA/B/C20/40/60%; low gates approximately80/60/40%.\r\n");
    puts_uart("INL enables stay LOW through wake/config; gate source50mA/sink100mA.\r\n");
#else
    puts_uart("\r\nDiagnostic: DRV8353RS LOW-SIDE-ONLY gate scope v1\r\n");
    puts_uart("NO MOTOR/HALL leads. NO INL high ties. Disconnect ALL INHA/B/C wires.\r\n");
    puts_uart("PA8 CN10-23->J2-4 INLA; PA9 CN10-21->J2-8 INLB; PA10 CN10-33->J2-12 INLC.\r\n");
    puts_uart("6PWM20kHz: GLA/GLB/GLC20/40/60%; source50mA/sink100mA.\r\n");
#endif
#endif
    if (!shutdown_self_test())
    {
        g_gate_shutdown_self_test = 2u;
        stop_all(STOP_SELF_TEST);
        puts_uart("STOP: EXTI shutdown self-test FAILED; ENABLE LOW.\r\n");
        while (1) { __WFI(); }
    }
    g_gate_shutdown_self_test = 1u;
    puts_uart("EXTI shutdown self-test: PASS (software trigger; physical fault wiring not proven).\r\n");
    puts_uart("ENABLE LOW. Power EVM12V/current limit0.25A, then press/release USER.\r\n");
#ifdef DRV_GATE_MOTOR_TEST
#ifdef DRV_GATE_CONTINUOUS
    puts_uart("NO timed motion stop. USER hold20ms stops; nFAULT/current/ADC/bus guards remain active.\r\n");
#elif defined(DRV_GATE_SPIN_TEST)
    puts_uart("12-second total maximum;250ms pulse cutoff; USER stop20ms; nFAULT EXTI stop.\r\n");
#else
    puts_uart("2-second total maximum;250ms pulse cutoff; USER stop20ms; nFAULT EXTI stop.\r\n");
#endif
#else
    puts_uart("5-minute maximum; USER stop20ms; nFAULT falling edge stops immediately via EXTI.\r\n");
#endif
    wait_button_level(false); wait_button_level(true); wait_button_level(false);
    uint32_t bus_mv = 0u;
    if (prepare_driver(&bus_mv))
    {
#ifdef DRV_GATE_MOTOR_TEST
        (void)run_motor_probe(&bus_mv);
#else
        puts_uart("GATES ACTIVE: R2="); put_hex(CTRL_RUN);
#ifdef DRV_GATE_3PWM
        puts_uart("; BOTH halves switching. Scope low gates or motor outputs vs GND.\r\n");
#else
        puts_uart("; scope GLA/GLB/GLC vs GND; high-side inputs absent.\r\n");
#endif
        uint32_t last_check = elapsed_ms, last_report = elapsed_ms;
        while (active)
        {
            const uint32_t now = elapsed_ms;
            if ((uint32_t)(now - last_check) >= 100u)
            {
                if (!expect_reg(0u, 0u) || !expect_reg(1u, 0u) ||
                    !expect_reg(2u, CTRL_RUN) || !expect_reg(3u, 0x300u) ||
                    !expect_reg(4u, 0x300u) || !expect_reg(5u, 0x315u) ||
                    !expect_reg(6u, 0x283u)) { break; }
                last_check = now;
            }
            if ((uint32_t)(now - last_report) >= 1000u)
            {
                if (!check_bus(&bus_mv)) { break; }
#ifdef DRV_GATE_ADC_CHECK
#ifdef DRV_GATE_SYNC
                if (!report_sync_current()) { break; }
#else
                if (!report_current()) { break; }
#endif
#endif
                puts_uart("ACTIVE VBUS_mV="); put_uint(bus_mv);
                puts_uart(" nFAULT="); put_uint((GPIOB->IDR >> 1) & 1u);
                puts_uart(" remaining_s="); put_uint(remaining_ms / 1000u);
                puts_uart(" R0/R1=0 R2="); put_hex(CTRL_RUN);
                puts_uart("\r\n"); last_report = now;
            }
        }
#endif
    }
    if (active) { stop_all(STOP_REGISTER); }
#ifdef DRV_GATE_MOTOR_TEST
    puts_uart("MOTOR_STOP failure="); put_uint(g_motor_failure);
    puts_uart(" last_nominal_mA="); put_signed_ma(motor_last_ma[0]); putc_uart('/');
    put_signed_ma(motor_last_ma[1]); putc_uart('/'); put_signed_ma(motor_last_ma[2]);
    puts_uart("\r\n");
#endif
#ifdef DRV_GATE_SYNC
    puts_uart("SYNC_STOP total="); put_uint(g_sync_adc_total);
    puts_uart(" failure="); put_uint(g_sync_failure);
    puts_uart(" last_CNT="); put_uint(g_sync_last_cnt);
    puts_uart(" last_period_CPUticks="); put_uint(g_sync_last_period);
    puts_uart(" last_raw="); put_uint(sync_last_raw[0]); putc_uart('/');
    put_uint(sync_last_raw[1]); putc_uart('/'); put_uint(sync_last_raw[2]);
    puts_uart("\r\n");
#endif
    puts_uart("STOP reason="); put_uint((uint32_t)stop_reason);
#ifdef DRV_GATE_ADC_CHECK
#ifdef DRV_GATE_MOTOR_TEST
    puts_uart(" (1=USER,2=nFAULT,3=deadline,4=SPI,5=register,6=bus,7=self-test,8=ADC,9=sync,10=motor,11=complete) t_ms=");
#else
#ifdef DRV_GATE_SYNC
    puts_uart(" (1=USER,2=nFAULT,3=deadline,4=SPI,5=register,6=bus,7=self-test,8=current-ADC,9=sync-ADC) t_ms=");
#else
    puts_uart(" (1=USER,2=nFAULT,3=deadline,4=SPI,5=register,6=bus,7=self-test,8=current-ADC) t_ms=");
#endif
#endif
#else
    puts_uart(" (1=USER,2=nFAULT,3=deadline,4=SPI,5=register,6=bus,7=self-test) t_ms=");
#endif
    put_uint(stop_ms); puts_uart(" nFAULT_at_stop="); put_uint((stop_pins >> 1) & 1u);
    puts_uart(" VBUS_mV_last="); put_uint(bus_mv);
    puts_uart("\r\nENABLE LOW; PWM GPIO LOW. Fault registers reset on sleep; reset to repeat.\r\n");
#ifdef DRV_GATE_MOTOR_TEST
    report_motor_feedback();
#endif
    while (1) { __WFI(); }
}
