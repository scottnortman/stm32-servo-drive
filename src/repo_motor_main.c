// Bench adapter. Motor math, PWM and ADC acquisition are the existing repo modules.
#include "stm32f4xx.h"
#include "clock.h"
#include "drv8353.h"
#include "pwm.h"
#include "loops.h"
#include "control.h"
#include "current_feedback.h"
#include "config.h"
#include "repo_motor_profile.h"
#ifdef REPO_CURRENT_TUNING
#include "repo_tune_link.h"
#endif
#ifdef REPO_MCUVIEWER
#include "repo_tune_mcuviewer.h"
#endif
#ifdef REPO_RECIPROCAL
#include "repo_reciprocate.h"
#endif
#ifdef REPO_COUNT_POLES
#include "repo_pole_count.h"
#endif
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

enum { INLS=(1u<<6)|(1u<<7)|(1u<<9), ALIGN_MS=1000u,
       POLE_PAIRS_GEMS=4u, NOMINAL_RPM=10u };
enum { USER_STOP=1u, FAULT_STOP=2u, RAW_STOP=3u, FILTER_STOP=4u,
       ADC_STOP=5u, BUS_STOP=6u, CONFIG_STOP=7u, SELF_STOP=8u,
       TIMING_STOP=10u };
static volatile uint32_t millis,reason,start_ms,user_ms,last_adc_ms,last_adc_count;
static volatile bool running;
volatile uint32_t g_repo_self_test,g_repo_control_ticks,g_repo_irq_max_cycles;
volatile uint32_t g_repo_timer_events,g_repo_fault_irq_seen;
volatile uint32_t g_repo_bus_mv;
volatile float g_repo_theta,g_repo_voltage,g_repo_phase_amps[3];
#ifdef REPO_CURRENT_5000MA
volatile uint32_t g_repo_csa_control;
#endif
static PIState amplitude_limit;
#ifdef REPO_RECIPROCAL
#ifndef REPO_CURRENT_TUNING
static RepoReciprocate reciprocal;
#endif
volatile uint32_t g_repo_half_moves;
volatile int32_t g_repo_command_mech_mdeg;
#endif
#ifdef REPO_COUNT_POLES
volatile uint32_t g_repo_electrical_turns;
#endif

static void stop_drive(uint32_t why)
{
    const uint32_t mask=__get_PRIMASK(); __disable_irq();
    drv_enable_low(); GPIOC->BSRR=(uint32_t)INLS<<16; pwm_disable();
    running=false;
#ifdef REPO_CURRENT_TUNING
    repo_tune_disarm();
#endif
    if (reason==0u) { reason=why; }
    __set_PRIMASK(mask);
}

static void uart_char(char c)
{
#if defined(REPO_CURRENT_TUNING) && !defined(REPO_MCUVIEWER)
    (void)c; //BinaryDMA telemetry owns USART2 in this target.
#else
    while (!(USART2->SR & USART_SR_TXE)) {} USART2->DR=(uint8_t)c;
#endif
}
static void uart_text(const char *s) { while (*s) { uart_char(*s++); } }
static void uart_uint(uint32_t n)
{
    char b[10]; unsigned int i=0u;
    do { b[i++]=(char)('0'+n%10u); n/=10u; } while (n!=0u);
    while (i!=0u) { uart_char(b[--i]); }
}
static void uart_int(int32_t n)
{ if (n<0) { uart_char('-'); uart_uint((uint32_t)(-n)); } else { uart_uint((uint32_t)n); } }
static void wait_ms(uint32_t delay)
{ const uint32_t since=millis; while ((uint32_t)(millis-since)<delay) { __WFI(); } }
static void button(bool pressed)
{
    uint32_t since=millis;
    while ((uint32_t)(millis-since)<25u)
    {
#ifdef REPO_CURRENT_TUNING
        repo_tune_poll(millis,false,reason,g_repo_irq_max_cycles);
        if(repo_tune_take_stop()) { reason=9u; }
#endif
        if (((GPIOC->IDR & (1u<<13))==0u)!=pressed) { since=millis; }
    }
}

void EXTI1_IRQHandler(void)
{ if (EXTI->PR & 2u) { ++g_repo_fault_irq_seen; stop_drive(FAULT_STOP); EXTI->PR=2u; } }

// Vector adapter adds only the board's raw-current shutdown. The original
// current_feedback.c ADC_IRQHandler still owns injected acquisition/filtering.
void ADC_IRQHandler(void);
void repo_ADC_IRQHandler(void)
{
    if (ADC1->SR & ADC_SR_AWD)
    {
        if (running) { stop_drive(RAW_STOP); }
        ADC1->SR=~ADC_SR_AWD;
    }
    ADC_IRQHandler();
}

void SysTick_Handler(void)
{
    ++millis;
    if (!running) { return; }
#ifdef REPO_CURRENT_TUNING
    if(!repo_tune_link_alive(millis)) {stop_drive(9u);return;}
#endif
    if (!drv8353_fault_pin_ok()) { stop_drive(FAULT_STOP); return; }
    if ((GPIOC->IDR & (1u<<13))==0u)
    { if (++user_ms>=20u) { stop_drive(USER_STOP); return; } }
    else { user_ms=0u; }
    const uint32_t samples=current_feedback_sample_count();
    if (samples!=last_adc_count) { last_adc_count=samples; last_adc_ms=millis; }
    else if ((uint32_t)(millis-last_adc_ms)>=2u) { stop_drive(ADC_STOP); }
}

void TIM1_UP_TIM10_IRQHandler(void)
{
    TIM1->SR=~TIM_SR_UIF;
    ++g_repo_timer_events;
    if (!running) {
#ifdef REPO_MCUVIEWER
        repo_tune_recorder_tick();
#endif
        return;
    }
    const uint32_t cycle=DWT->CYCCNT;
    if (!current_feedback_sample_valid()) { stop_drive(ADC_STOP); return; }
    float amps[3];
    current_feedback_get_phase_amps(&amps[0],&amps[1],&amps[2]);
#ifdef REPO_CURRENT_TUNING
    // Motion profiles tighten the 6.25 A-capable tuning target's phase trips.
    // The ADC watchdog threshold is selected at physical arming.
#ifdef REPO_TUNING_24V
    const float phase_trip=1.0f;
#else
    const float phase_trip=repo_tune_d_reciprocal_selected() ?
        (repo_tune_motion_amplitude()==6.0f ? 7.5f :
         repo_tune_motion_amplitude()==5.0f ? 6.5f : 3.75f) : REPO_FILTER_TRIP_A;
#endif
#else
    const float phase_trip=REPO_FILTER_TRIP_A;
#endif
#ifndef REPO_CURRENT_TUNING
    float sum_squares=0.0f;
#endif
    for (unsigned int c=0u;c<3u;++c)
    {
        g_repo_phase_amps[c]=amps[c];
        if (!(fabsf(amps[c])<=phase_trip)) { stop_drive(FILTER_STOP); return; }
        if (current_adc_raw[c]<16u || current_adc_raw[c]>4079u)
        { stop_drive(ADC_STOP); return; }
#ifndef REPO_CURRENT_TUNING
        sum_squares+=amps[c]*amps[c];
#endif
    }
#ifdef REPO_CURRENT_TUNING
    float id,iq,vd,vq;
    const float field_theta=repo_tune_field_angle();
    current_feedback_get_dq(field_theta,&id,&iq);
    const bool active=repo_tune_control_tick(g_repo_timer_events,id,iq,amps,&vd,&vq);
    if(running && reason==0u) {
        if(active) pwm_apply_dq(vd,vq,field_theta);
        else {GPIOC->BSRR=(uint32_t)INLS<<16;pwm_disable();pwm_apply_dq(0,0,0);}
        g_repo_theta=field_theta;g_repo_voltage=sqrtf(vd*vd+vq*vq);++g_repo_control_ticks;
    }
#else
    const float magnitude=sqrtf(sum_squares*(2.0f/3.0f));
    const uint32_t age=millis-start_ms;
    const float current_reference=age<500u ? REPO_PHASE_TARGET_A*(float)age/500.0f : REPO_PHASE_TARGET_A;
    // Existing PI implements a slow amplitude/current limiter; the existing
    // open_loop_step and pwm_apply_dq implement the rotating sine voltage.
    const float voltage=pi_step(&amplitude_limit,current_reference-magnitude,DT_CURRENT);
#ifndef REPO_RECIPROCAL
    float delta=0.0f;
    if (age>=ALIGN_MS)
    {
        const uint32_t ramp=age-ALIGN_MS;
        const float factor=ramp<1000u ? (float)ramp/1000.0f : 1.0f;
#ifdef REPO_COUNT_POLES
        delta=FOC_TWO_PI*0.1f*DT_CURRENT*factor;
#else
        delta=FOC_TWO_PI*(float)POLE_PAIRS_GEMS*((float)NOMINAL_RPM/60.0f)*DT_CURRENT*factor;
#endif
    }
#endif
    float theta=g_repo_theta;
    if (running && reason==0u)
    {
        // Fault/ADC IRQs stay enabled through the existing math. A preempting
        // stop removes ENABLE/INLs/MOE; later CCR writes cannot re-enable drive.
#ifdef REPO_RECIPROCAL
        if (age>=ALIGN_MS) { repo_reciprocate_step(&reciprocal,&theta,voltage); }
        else { open_loop_step(&theta,voltage,0.0f); }
        g_repo_half_moves=reciprocal.half_moves;
        g_repo_command_mech_mdeg=(int32_t)(reciprocal.mechanical_position*(180000.0f/M_PI));
#elif defined(REPO_COUNT_POLES)
        uint32_t turns=g_repo_electrical_turns;
        repo_pole_count_step(&theta,&turns,voltage,delta);
        g_repo_electrical_turns=turns;
#else
        open_loop_step(&theta,voltage,delta);
#endif
        g_repo_theta=theta; g_repo_voltage=voltage; ++g_repo_control_ticks;
    }
#endif
#ifdef REPO_MCUVIEWER
    repo_tune_recorder_tick();
#endif
    const uint32_t duration=DWT->CYCCNT-cycle;
    if (duration>g_repo_irq_max_cycles) { g_repo_irq_max_cycles=duration; }
#ifdef REPO_CURRENT_TUNING
#ifdef REPO_TUNING_24V
    if(duration>4500u)
#else
    if(repo_tune_d_reciprocal_selected() && duration>4500u)
#endif
    { stop_drive(TIMING_STOP); }
#endif
}

static bool regular_read(uint8_t channel,uint16_t *value)
{
    (void)ADC1->DR; ADC1->SQR1=0u; ADC1->SQR3=channel;
    ADC1->CR2|=ADC_CR2_SWSTART;
    const uint32_t since=millis;
    while (!(ADC1->SR & ADC_SR_EOC))
    { if (reason!=0u || (uint32_t)(millis-since)>=5u) { stop_drive(BUS_STOP); return false; } }
    *value=(uint16_t)ADC1->DR; return true;
}

static bool check_bus(void)
{
    uint32_t ref=0u,bus=0u;
    for (unsigned int n=0u;n<8u;++n)
    {
        uint16_t r,b;
        if (!regular_read(17u,&r) || !regular_read(4u,&b)) { return false; }
        ref+=r; bus+=b;
    }
    const uint16_t cal=*(const uint16_t *)0x1FFF7A2Au;
    const uint32_t vdda=ref!=0u ? (uint32_t)cal*3300u*8u/ref : 0u;
    g_repo_bus_mv=(uint32_t)((uint64_t)bus*vdda*392760u/(8ull*4095u*9760u));
    const uint32_t bus_nominal_mv=(uint32_t)(V_BUS*1000.0f);
    if (cal<1000u || cal>2000u || vdda<3000u || vdda>3600u ||
        g_repo_bus_mv<bus_nominal_mv-2000u ||
        g_repo_bus_mv>bus_nominal_mv+2000u)
    { stop_drive(BUS_STOP); return false; }
    return true;
}

static void report(void)
{
    uart_text(running ? "RUN" : "STOP"); uart_text(" reason="); uart_uint(reason);
    uart_text(" control_ticks="); uart_uint(g_repo_control_ticks);
    uart_text(" ADC_samples="); uart_uint(current_feedback_sample_count());
    uart_text(" phase_mA=");
    for (unsigned int c=0u;c<3u;++c)
    { if (c!=0u) { uart_char('/'); } uart_int((int32_t)(g_repo_phase_amps[c]*1000.0f)); }
    uart_text(" voltage_mV="); uart_uint((uint32_t)(g_repo_voltage*1000.0f));
    uart_text(" field_electrical_mdeg="); uart_uint((uint32_t)(g_repo_theta*(180000.0f/M_PI)));
#ifdef REPO_RECIPROCAL
    uart_text(" half_moves="); uart_uint(g_repo_half_moves);
    uart_text(" command_mech_mdeg="); uart_int(g_repo_command_mech_mdeg);
    uart_text(" command_direction="); uart_text((g_repo_half_moves & 1u)!=0u ? "BACK" : "FORWARD");
    uart_text(" VBUS_preflight_mV="); uart_uint(g_repo_bus_mv);
#elif defined(REPO_COUNT_POLES)
    uart_text(" electrical_turns="); uart_uint(g_repo_electrical_turns);
    uart_text(" VBUS_preflight_mV="); uart_uint(g_repo_bus_mv);
#else
    uart_text(" VBUS_mV="); uart_uint(g_repo_bus_mv);
#endif
    uart_text(" max_IRQ_CPUticks="); uart_uint(g_repo_irq_max_cycles); uart_text("\r\n");
}

int main(void)
{
    // Board adapter: existing INL-enable harness, USER PC13 and UART2.
    RCC->AHB1ENR|=RCC_AHB1ENR_GPIOAEN|RCC_AHB1ENR_GPIOBEN|RCC_AHB1ENR_GPIOCEN;
    GPIOB->BSRR=1u<<16; GPIOB->MODER=(GPIOB->MODER & ~15u)|1u;
    GPIOC->BSRR=(uint32_t)INLS<<16;
    const uint32_t inl_modes=(3u<<12)|(3u<<14)|(3u<<18);
    GPIOC->MODER=(GPIOC->MODER & ~inl_modes)|(1u<<12)|(1u<<14)|(1u<<18);
    GPIOC->MODER&=~(3u<<26); GPIOC->PUPDR=(GPIOC->PUPDR & ~(3u<<26))|(1u<<26);
    // Board-only VSENVM input: PA4/CN7-32 <- EVM J1-4. Current ADC module owns PC0/1/2.
    GPIOA->MODER|=3u<<8;
    GPIOA->PUPDR&=~(3u<<8);
    SCB->CPACR|=(3u<<20)|(3u<<22); __DSB(); __ISB();
    clock_init();
    RCC->APB1ENR|=RCC_APB1ENR_USART2EN;
    GPIOA->MODER=(GPIOA->MODER & ~((3u<<4)|(3u<<6)))|(2u<<4)|(2u<<6);
    GPIOA->AFR[0]=(GPIOA->AFR[0] & ~((15u<<8)|(15u<<12)))|(7u<<8)|(7u<<12);
    USART2->BRR=50000000u/115200u; USART2->CR1=USART_CR1_UE|USART_CR1_TE;
#ifdef REPO_CURRENT_TUNING
    repo_tune_init();
#endif
    SysTick_Config(SystemCoreClock/1000u); NVIC_SetPriority(SysTick_IRQn,3u);
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT=0u; DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
#ifdef REPO_MCUVIEWER
    const bool recorder_ok=repo_tune_recorder_self_test();
#endif
    RCC->APB2ENR|=RCC_APB2ENR_SYSCFGEN;
    SYSCFG->EXTICR[0]=(SYSCFG->EXTICR[0] & ~(15u<<4))|(1u<<4);
    EXTI->FTSR|=2u; EXTI->RTSR&=~2u; EXTI->PR=2u;
    NVIC_SetPriority(EXTI1_IRQn,0u); NVIC_EnableIRQ(EXTI1_IRQn);

    // The real repo modules own TIM1 configuration, ADC timing and acquisition.
    pwm_init(); current_feedback_init();
#ifdef REPO_CURRENT_TUNING
    // pwm_set_duty maps logicalB/C to physicalC/B. Match current transforms
    // to that existing mapping; physical plots are mapped back in telemetry.
    ADC1->JSQR=(2u<<ADC_JSQR_JL_Pos)|(10u<<ADC_JSQR_JSQ2_Pos)|
                (12u<<ADC_JSQR_JSQ3_Pos)|(11u<<ADC_JSQR_JSQ4_Pos);
#endif
#ifdef REPO_CURRENT_6250MA
    // Board sampling point: leave the full15cycle sequence before the next
    // switching edge at the4V ceiling. The repo still owns trigger/acquisition.
    TIM1->CCR4=REPO_ADC_TRIGGER_CCR;
#endif
#ifdef REPO_DOUBLE_CURRENT
    // Board sample-time setting only; retain the original ADC ISR/trigger path.
    // The larger voltage ceiling needs the acquisition to finish before the
    // earliest PWM edge:24.49us+8.16us <35.21us at1.1V phase amplitude.
    ADC1->SMPR1=(ADC1->SMPR1 & ~0x1FFu)|REPO_ADC_SAMPLE_CODE|
                (REPO_ADC_SAMPLE_CODE<<3)|(REPO_ADC_SAMPLE_CODE<<6);
#endif
    NVIC_SetPriority(TIM1_UP_TIM10_IRQn,2u); NVIC_SetPriority(ADC_IRQn,1u);
    ADC->CCR|=ADC_CCR_TSVREFE;
    ADC1->SMPR1|=7u<<21; ADC1->SMPR2|=7u<<12;
    float theta=0.0f; open_loop_step(&theta,0.0f,0.0f);
#ifdef REPO_RECIPROCAL
    // Exercise the command adapter and actual repo math on the MCU at zero
    // voltage before any enable. Leave the real trajectory state untouched.
    RepoReciprocate trial; repo_reciprocate_reset(&trial);
    trial.sample=REPO_RECIP_SAMPLES_PER_LEG/2u;
    repo_reciprocate_step(&trial,&theta,0.0f);
    const bool forward_math_ok=isfinite(theta) && isfinite(trial.mechanical_position);
    trial.reverse=true; trial.sample=REPO_RECIP_SAMPLES_PER_LEG-1u;
    repo_reciprocate_step(&trial,&theta,0.0f);
    const bool reciprocal_math_ok=forward_math_ok && theta==0.0f && trial.mechanical_position==0.0f;
#endif
    wait_ms(20u);
    // Prove the board's shutdown vector with zero output and ENABLE already LOW.
    const uint32_t boot_mask=__get_PRIMASK(); __disable_irq();
    running=true; EXTI->PR=2u; EXTI->IMR|=2u; EXTI->SWIER|=2u;
    __set_PRIMASK(boot_mask);
    wait_ms(1u); EXTI->IMR&=~2u; running=false;
    const bool fault_irq_ok=reason==FAULT_STOP && g_repo_fault_irq_seen==1u;
    reason=0u;
    if (!fault_irq_ok || current_feedback_sample_count()<10u || g_repo_timer_events<10u ||
        TIM1->CCR1!=1249u || TIM1->CCR2!=1249u || TIM1->CCR3!=1249u ||
        (TIM1->BDTR & TIM_BDTR_MOE)!=0u || (GPIOB->ODR & 1u)!=0u || (GPIOC->ODR & INLS)!=0u ||
        (GPIOA->MODER & (3u<<8))!=(3u<<8) || (GPIOA->PUPDR & (3u<<8))!=0u
#ifdef REPO_DOUBLE_CURRENT
        || (ADC1->SMPR1 & 0x1FFu)!=(REPO_ADC_SAMPLE_CODE|(REPO_ADC_SAMPLE_CODE<<3)|(REPO_ADC_SAMPLE_CODE<<6))
#endif
#ifdef REPO_CURRENT_6250MA
        || TIM1->CCR4!=REPO_ADC_TRIGGER_CCR
#endif
#ifdef REPO_CURRENT_TUNING
        || ADC1->JSQR!=((2u<<ADC_JSQR_JL_Pos)|(10u<<ADC_JSQR_JSQ2_Pos)|
                       (12u<<ADC_JSQR_JSQ3_Pos)|(11u<<ADC_JSQR_JSQ4_Pos))
#endif
#ifdef REPO_MCUVIEWER
        || !recorder_ok
#endif
#ifdef REPO_RECIPROCAL
        || !reciprocal_math_ok
#endif
       )
    { g_repo_self_test=2u; stop_drive(SELF_STOP); }
    else { g_repo_self_test=1u; }
#ifdef REPO_MCUVIEWER
    uart_text("\r\nREPO CURRENT TUNING v1: MCUViewer FREE,20 live/5 Recorder variables.\r\n");
#elif defined(REPO_RECIPROCAL)
#ifdef REPO_CURRENT_5000MA
#ifdef REPO_CURRENT_6250MA
    uart_text("\r\nREPO ONE-TURN RECIPROCAL v6 (6250mA,5s per leg): original repo motor path.\r\n");
#elif defined(REPO_RECIP_DOUBLE_SPEED)
    uart_text("\r\nREPO ONE-TURN RECIPROCAL v5 (5000mA,2x speed): original repo motor path.\r\n");
#else
    uart_text("\r\nREPO ONE-TURN RECIPROCAL v4 (5000mA): original repo motor path.\r\n");
#endif
#elif defined(REPO_CURRENT_2500MA)
    uart_text("\r\nREPO ONE-TURN RECIPROCAL v2 (2500mA): original repo motor path.\r\n");
#else
    uart_text("\r\nREPO ONE-TURN RECIPROCAL v1 (2000mA): original repo motor path.\r\n");
#endif
#elif defined(REPO_COUNT_POLES)
#ifdef REPO_DOUBLE_CURRENT
    uart_text("\r\nREPO POLE COUNT v2 (1400mA):10 electrical turns,original repo motor path.\r\n");
#else
    uart_text("\r\nREPO POLE COUNT v1:10 electrical turns,original repo motor path.\r\n");
#endif
#else
    uart_text("\r\nREPO MOTOR v2: original loops.c -> pwm.c, original current_feedback.c ADC.\r\n");
#endif
    uart_text("Boot zero-output PWM/ADC/software-EXTI check="); uart_uint(g_repo_self_test); uart_text(" (1=PASS). ENABLE LOW.\r\n");
    uart_text("Software EXTI test does not prove the physical nFAULT wire.\r\n");
#ifdef REPO_MCUVIEWER
    uart_text("Original repo signed d/q PI,PWM,ADC. Fixed electrical frame; no rotor feedback.\r\n");
    uart_text("Default D-axis0.5A step,1s test,0.75V ceiling. Gains are starting values.\r\n");
    uart_text("USER arms at ZERO output. Apply settings then increment START in MCUViewer.\r\n");
#ifdef REPO_TUNING_24V
    uart_text("24V stationary tuning only:0.5A reference,0.1V voltage step,2V vector ceiling,2kHz tones.No motion.\r\n");
    uart_text("Bounded tests auto-stop. USER/stop/fault disarms; re-arm with USER.\r\n");
#else
    uart_text("Bounded tests auto-stop; 6A motion repeats until USER/stop/fault. Re-arm with USER.\r\n");
#endif
#elif defined(REPO_RECIPROCAL)
#ifdef REPO_RECIP_QUAD_SPEED
    uart_text("1s alignment,thenonecommandedrevolutionFORWARDandBACK;5sperleg,cosineaccel/decel.\r\n");
#elif defined(REPO_RECIP_DOUBLE_SPEED)
    uart_text("1s alignment,thenonecommandedrevolutionFORWARDandBACK;10sperleg,cosineaccel/decel.\r\n");
#else
    uart_text("1s alignment,thenonecommandedrevolutionFORWARDandBACK;20sperleg,cosineaccel/decel.\r\n");
#endif
    uart_text("4polepairs assumed; no encoder. Actual shafttravel/smoothness unmeasured.\r\n");
#ifdef REPO_CURRENT_6250MA
    uart_text("Nominal6250mA,max4000mV phase. Gain20 with matched ADC scaling. Repeats until USER/fault.\r\n");
#elif defined(REPO_CURRENT_5000MA)
    uart_text("Nominal5000mA,max3200mV phase. Gain20 with matched ADC scaling. Repeats until USER/fault.\r\n");
#elif defined(REPO_CURRENT_2500MA)
    uart_text("Nominal2500mA,max1600mV phase. Repeats until USER/fault. Keep12V supplyfixed.\r\n");
#else
    uart_text("Nominal2000mA,max1300mV phase. Repeats until USER/fault. Keep12V supplyfixed.\r\n");
#endif
#elif defined(REPO_COUNT_POLES)
    uart_text("1s alignment,1s speed ramp,then0.1Hz electrical(~100s). No pole-count assumption.\r\n");
#ifdef REPO_DOUBLE_CURRENT
    uart_text("After10 electrical turns,hold finalfield until USER. Nominal1400mA,max1100mV phase.\r\n");
#else
    uart_text("After10 electrical turns,hold finalfield until USER. Nominal700mA,max550mV phase.\r\n");
#endif
    uart_text("Keep supply12V fixed. Bus measured before drive; original current ADC runs alone during drive.\r\n");
#else
    uart_text("Slow10rpm FIELD; nominal700mA amplitude limit; max550mV phase voltage.\r\n");
#endif
#ifdef REPO_MCUVIEWER
#ifdef REPO_TUNING_24V
    uart_text("1A filtered/1.5A sampled raw trips; USER20ms,nFAULT,ADC guards retained.\r\n");
#else
    uart_text("7.5A filtered/8A sampled raw trips; USER20ms,nFAULT,ADC guards retained.\r\n");
#endif
#elif defined(REPO_RECIPROCAL)
#ifdef REPO_CURRENT_6250MA
    uart_text("7.5A filtered/8A sampled raw trips. USER hold stops;nFAULT immediate;ADC15cycles.\r\n");
#elif defined(REPO_CURRENT_5000MA)
    uart_text("6.5A filtered/8A sampled raw trips. USER hold stops;nFAULT immediate;ADC28cycles.\r\n");
#elif defined(REPO_CURRENT_2500MA)
    uart_text("3.3A filtered/4.5A sampled raw trips. USER hold stops;nFAULT immediate;ADC56cycles.\r\n");
#else
    uart_text("2.8A filtered/4.5A sampled raw trips. USER hold stops;nFAULT immediate;ADC56cycles.\r\n");
#endif
#elif defined(REPO_DOUBLE_CURRENT)
    uart_text("2A filtered/4A sampled raw trips. USER hold stops; nFAULT immediate. ADC56cycles.\r\n");
#else
    uart_text("1A filtered/2A sampled raw trips. USER hold stops; nFAULT immediate. No timed stop.\r\n");
#endif
#ifdef REPO_MCUVIEWER
#ifdef REPO_TUNING_24V
    uart_text("Secure unloaded motor.24V fixed/0.25A input limit for INITIAL0.5A test.Halls disconnected.\r\n");
#else
    uart_text("Secure unloaded motor.12V fixed/0.25A input limit for INITIAL0.5A test.Halls disconnected.\r\n");
#endif
#elif defined(REPO_RECIPROCAL)
#ifdef REPO_CURRENT_6250MA
    uart_text("Secure unloaded motor.Halls disconnected.12V/3.5A supply limit,USER start.\r\n");
#elif defined(REPO_CURRENT_5000MA)
    uart_text("Secure unloaded motor.Halls disconnected.12V/2.5A supply limit,USER start.\r\n");
#elif defined(REPO_CURRENT_2500MA)
    uart_text("Secure unloaded motor.Halls disconnected.12V/0.75A supply limit,USER start.\r\n");
#else
    uart_text("Secure unloaded motor.Halls disconnected.12V/0.5A supply limit,USER start.\r\n");
#endif
    repo_reciprocate_reset(&reciprocal);
#else
    uart_text("No rotor feedback. Secure unloaded motor. Halls disconnected.12V/0.25A,USER start.\r\n");
#endif
    if (reason!=0u) { while (1) { __WFI(); } }
#ifdef REPO_CURRENT_TUNING
arm_again:
#endif
    button(false); button(true); button(false);
#ifdef REPO_CURRENT_TUNING
    reason=0u;
#endif
#if defined(REPO_COUNT_POLES) || defined(REPO_RECIPROCAL)
    // Board preflight only: no injected/regular ADC contention while reading
    // VREF/bus. All drive outputs are still disabled. The existing calibration
    // routine subsequently restores the original injected sampling path.
    const uint32_t saved_edges=ADC1->CR2 & ADC_CR2_JEXTEN;
    const uint32_t saved_jeoc=ADC1->CR1 & ADC_CR1_JEOCIE;
    ADC1->CR2&=~ADC_CR2_JEXTEN; ADC1->CR1&=~ADC_CR1_JEOCIE;
    wait_ms(1u);
    const bool bus_ok=check_bus();
    ADC1->CR1|=saved_jeoc; ADC1->CR2|=saved_edges;
    if (!bus_ok) {
        report();
#ifdef REPO_CURRENT_TUNING
        goto arm_again;
#else
        while (1) { __WFI(); }
#endif
    }
#else
    if (!check_bus()) { report(); while (1) { __WFI(); } }
#endif
    drv8353_init();
    if (!drv8353_configure() || !drv8353_fault_pin_ok()) { stop_drive(CONFIG_STOP); }
#ifdef REPO_CURRENT_5000MA
    // Board range selection only. Original driver config is verified first;
    // all INLs and PWM remain disabled through this change and fresh zeros.
    if (reason==0u)
    {
        // Original configure already verifies R6. An additional first read
        // after switching from R2 returned a stale low bit on this board.
        // Verify the NEW gain after writing R6 instead (both reads required).
        if ((GPIOC->ODR & INLS)!=0u || (TIM1->BDTR & TIM_BDTR_MOE)!=0u)
        { stop_drive(CONFIG_STOP); }
        else
        {
            drv8353_write_reg(DRV8353_REG_CSA_CONTROL,0x0283u);
            wait_ms(1u);
            const uint16_t first=drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
            g_repo_csa_control=drv8353_read_reg(DRV8353_REG_CSA_CONTROL);
            if (first!=0x0283u || g_repo_csa_control!=0x0283u || !drv8353_fault_pin_ok())
            { stop_drive(CONFIG_STOP); }
            else { uart_text("CSA gain20 verified twice: R6=0x283; fresh zeros follow.\r\n"); }
        }
    }
#endif
    if (reason==0u)
    {
        current_feedback_calibrate();
        for (unsigned int c=0u;c<3u;++c)
        { if (!(adc_offset[c]>=1800.0f && adc_offset[c]<=2300.0f)) { stop_drive(ADC_STOP); } }
    }
    if (reason==0u)
    {
        wait_ms(10u);
        if (!current_feedback_sample_valid() || current_feedback_sample_count()<16u)
        { stop_drive(ADC_STOP); }
    }
    if (reason==0u)
    {
        float low=adc_offset[0],high=adc_offset[0];
        for (unsigned int c=1u;c<3u;++c)
        { if (adc_offset[c]<low) { low=adc_offset[c]; } if (adc_offset[c]>high) { high=adc_offset[c]; } }
#ifdef REPO_CURRENT_TUNING
#ifdef REPO_TUNING_24V
        const float raw_trip_a=1.5f;
#else
        const float raw_trip_a=repo_tune_d_reciprocal_selected() ?
        (repo_tune_motion_amplitude()==6.0f ? 8.0f :
         repo_tune_motion_amplitude()==5.0f ? 7.0f : 4.0f) : REPO_RAW_TRIP_A;
#endif
#else
        const float raw_trip_a=REPO_RAW_TRIP_A;
#endif
        const uint32_t trip=(uint32_t)(raw_trip_a/AMPS_PER_COUNT);
        ADC1->LTR=(uint32_t)low-trip; ADC1->HTR=(uint32_t)high+trip;
        pi_init(&amplitude_limit,0.0f,4.0f,0.0f,REPO_PHASE_VMAX_V);
#ifdef REPO_MCUVIEWER
        uart_text("ARMED: output OFF; waiting for fresh MCUViewer START sequence.\r\n");
#elif defined(REPO_RECIPROCAL)
        uart_text("START RECIPROCAL:1commandedrevforward,1back,repeat.USER stops.\r\n");
#elif defined(REPO_COUNT_POLES)
        uart_text("START COUNT: observe start mark after alignment.10 electricalturns then finalfield hold.\r\n");
#else
        uart_text("START: existing repo sine/open-loop path. Continuous until USER/fault.\r\n");
#endif
        // All SPI is complete, INLs LOW throughout configuration/calibration.
        wait_ms(1u);
        const uint32_t mask=__get_PRIMASK(); __disable_irq();
        EXTI->PR=2u;  // Clear before the level check; preserve any later falling edge.
        if (reason==0u && drv8353_fault_pin_ok())
        {
            ADC1->SR=~ADC_SR_AWD;
            ADC1->CR1|=ADC_CR1_JAWDEN|ADC_CR1_AWDIE;
            last_adc_count=current_feedback_sample_count();
            last_adc_ms=start_ms=millis;
            running=true; EXTI->IMR|=2u;
#ifndef REPO_CURRENT_TUNING
            pwm_enable(); GPIOC->BSRR=INLS;
#endif
        }
        else { stop_drive(FAULT_STOP); }
        __set_PRIMASK(mask);
    }
#if defined(REPO_COUNT_POLES) || defined(REPO_RECIPROCAL)
#ifndef REPO_CURRENT_TUNING
    uint32_t last_report=millis;
#endif
#ifdef REPO_COUNT_POLES
    bool count_announced=false;
#endif
#else
    uint32_t last_bus=millis,last_report=millis;
#endif
    while (running)
    {
        const uint32_t now=millis;
#ifdef REPO_CURRENT_TUNING
        repo_tune_poll(now,true,reason,g_repo_irq_max_cycles);
        if(repo_tune_take_stop()) {stop_drive(9u);break;}
        if(repo_tune_take_start()) {
            const uint32_t mask=__get_PRIMASK();__disable_irq();
            if(running && reason==0u && drv8353_fault_pin_ok()) {
                pwm_apply_dq(0,0,0);pwm_enable();GPIOC->BSRR=INLS;
            } else stop_drive(FAULT_STOP);
            __set_PRIMASK(mask);
        }
#else
#ifdef REPO_COUNT_POLES
        if (!count_announced && g_repo_electrical_turns>=REPO_POLE_TEST_TURNS)
        {
            uart_text("COUNT COMPLETE:10 commanded electricalturns;finalfield held. Shaftfollowing unmeasured.USER stops.\r\n");
            count_announced=true;
        }
#else
#ifndef REPO_RECIPROCAL
        if ((uint32_t)(now-last_bus)>=100u) { if (!check_bus()) { break; } last_bus=now; }
#endif
#endif
        if ((uint32_t)(now-last_report)>=1000u) { report(); last_report=now; }
#endif
        __WFI();
    }
    report(); uart_text("ENABLE LOW; reset to repeat. reason1=USER,2=nFAULT,3=raw,4=filtered,5=ADC,6=bus,7=config,8=selftest.\r\n");
#ifdef REPO_CURRENT_TUNING
    // A deliberate physicalUSER press is needed again after a disarm/fault.
    button(false);
    goto arm_again;
#endif
    while (1) { __WFI(); }
}
