#include "motor_continuous.h"
#include "motor_probe_pwm.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void run_continuously(int32_t delay, int32_t sign)
{
    MotorContinuous state;
    MotorProbe probe;
    assert(motor_continuous_reset(&state,&probe));
    int32_t plant[3]={0};
    uint32_t changes=0u;
    for (uint32_t n=0u;n<400000u;++n)
    {
        uint16_t compare[3];
        assert(motor_probe_pwm_compares(probe.source,probe.sink,probe.duty,compare));
        const int32_t differential=compare[probe.source]-compare[probe.sink];
        const int32_t magnitude=differential>12 ? (differential-12)*20 : 0;
        int32_t desired[3]={0},current[3];
        desired[probe.source]=sign*magnitude;
        desired[probe.sink]=-sign*magnitude;
        for (unsigned int c=0u;c<3u;++c)
        {
            plant[c]+=(desired[c]*256-plant[c])/delay;
            current[c]=plant[c]/256;
        }
        const uint32_t previous=state.advances;
        assert(motor_continuous_update(&state,&probe,current)==MOTOR_PROBE_OK);
        assert(probe.duty<=MOTOR_PROBE_MAX_CCR);
        assert(motor_probe_pwm_compares(probe.source,probe.sink,probe.duty,compare));
        assert(motor_probe_pwm_valid(probe.source,probe.sink,compare));
        if (state.advances!=previous)
        {
            ++changes;
            assert(probe.duty>0u && probe.integrator_q16>0);
            assert(probe.samples==MOTOR_PROBE_RAMP_SAMPLES);
            assert(probe.mapping_grace_samples==MOTOR_CONTINUOUS_MAPPING_GRACE);
            assert(probe.commutation_settle_samples==MOTOR_CONTINUOUS_SETTLE_SAMPLES);
            assert(probe.filtered_q8[0]!=0 || probe.filtered_q8[1]!=0 || probe.filtered_q8[2]!=0);
        }
    }
    //20 seconds, well beyond every old finite sequence, with no terminal state.
    assert(state.aligned && state.advances==79u && changes==79u);
}

int main(void)
{
    assert(MOTOR_PROBE_TARGET_MA==350 && MOTOR_PROBE_FILTER_TRIP_MA==500 && MOTOR_PROBE_RAW_TRIP_MA==1000);
    run_continuously(4,1); run_continuously(40,1);
    run_continuously(4,-1); run_continuously(40,-1);
    MotorContinuous state;
    MotorProbe probe;
    assert(motor_continuous_reset(&state,&probe));
    state.aligned=true; state.sector_samples=MOTOR_CONTINUOUS_SECTOR_SAMPLES-1u;
    assert(motor_continuous_update(&state,&probe,(int32_t[3]){1200,0,0})==MOTOR_PROBE_RAW_CURRENT);
    assert(state.advances==0u && probe.duty==0u);
    assert(motor_continuous_reset(&state,&probe));
    probe.filtered_q8[0]=600*256; probe.filtered_q8[1]=-600*256;
    probe.mapping_grace_samples=600u; probe.commutation_settle_samples=80u;
    assert(motor_continuous_update(&state,&probe,(int32_t[3]){600,-600,0})==MOTOR_PROBE_FILTER_CURRENT);
    assert(motor_continuous_reset(&state,&probe));
    state.aligned=true; state.sector_samples=MOTOR_CONTINUOUS_SECTOR_SAMPLES-1u;
    probe.samples=1000u;
    assert(motor_continuous_update(&state,&probe,(int32_t[3]){0,0,0})==MOTOR_PROBE_TARGET_MISSED);
    assert(motor_continuous_reset(&state,&probe));
    state.sector=6u;
    assert(motor_continuous_update(&state,&probe,(int32_t[3]){0,0,0})==MOTOR_PROBE_CONFIG);
    assert(motor_continuous_reset(&state,&probe));
    probe.samples=1000u; probe.mapping_grace_samples=2u;
    probe.filtered_q8[0]=350*256; probe.filtered_q8[1]=350*256;
    assert(motor_probe_update(&probe,(int32_t[3]){350,350,0})==MOTOR_PROBE_OK);
    assert(motor_probe_update(&probe,(int32_t[3]){350,350,0})==MOTOR_PROBE_MAPPING);
    puts("PASS: continuous >finite-run duration, no zero/coast transitions, retained filters, all PWM bounds and live protections");
    return 0;
}
