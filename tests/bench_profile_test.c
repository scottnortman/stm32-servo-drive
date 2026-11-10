// Integration contract: upstream AKM defaults must not leak into the GEMS bench.
#include "config.h"
#include "board_f411.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#ifdef REPO_MOTOR_BENCH
#include "repo_motor_profile.h"
#endif
int main(void)
{
    assert(fabsf(DT_CURRENT - 50e-6f) < 1e-10f);
    assert(PIN_PWM_A == 8 && PIN_PWM_B == 9 && PIN_PWM_C == 10);
#ifdef REPO_MOTOR_BENCH
    assert(V_BUS == 12.0f && SHUNT_GAIN == 20.0f);
    assert(REPO_ADC_SAMPLE_CODE == 1u && REPO_ADC_TRIGGER_CCR == 2489u);
    assert(REPO_FILTER_TRIP_A == 7.5f && REPO_RAW_TRIP_A == 8.0f);
    assert(fabsf(AMPS_PER_COUNT - 3.3f/(4096.0f*0.007f*20.0f)) < 1e-8f);
    puts("PASS: 12V gain20 GEMS bench sampling/current-limit contract");
#else
    assert(V_BUS == 24.0f && SHUNT_GAIN == 40.0f && MOTOR_POLE_PAIRS == 3u);
    assert(CURRENT_LOOP_KP == 5.28f && CURRENT_LOOP_KI == 8696.0f);
    puts("PASS: upstream 24V AKM defaults remain isolated from bench settings");
#endif
}
