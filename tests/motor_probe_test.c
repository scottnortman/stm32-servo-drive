#include "motor_probe.h"
#include "motor_probe_pwm.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void check_regulation(uint8_t source, uint8_t sink, int32_t sign, int32_t time_constant)
{
    MotorProbe probe;
    assert(motor_probe_reset(&probe, source, sink));
    int32_t plant_q8 = 0;
    for (unsigned int n = 0; n < 5000; ++n)
    {
        // Test plant includes a dead band and selectable response delay. It is
        // not a model or a source of R/L parameters for Scott's actual motor.
        uint16_t compare[3];
        assert(motor_probe_pwm_compares(source, sink, probe.duty, compare));
        const int32_t differential = compare[source] - compare[sink];
        const int32_t requested = differential > 12 ? (differential - 12) * 20 : 0;
        plant_q8 += (requested * 256 - plant_q8) / time_constant;
        const int32_t value = sign * (plant_q8 / 256);
        int32_t currents[3] = {0,0,0};
        currents[source] = -value; currents[sink] = value;
        assert(motor_probe_update(&probe, currents) == MOTOR_PROBE_OK);
        assert(probe.duty <= MOTOR_PROBE_MAX_CCR);
    }
    assert(motor_probe_target_seen(&probe));
    const int32_t result = probe.filtered_q8[sink] / 256;
    assert(result * sign >= MOTOR_PROBE_TARGET_MA - 30 &&
           result * sign <= MOTOR_PROBE_TARGET_MA + 30);
}

int main(void)
{
    assert(motor_probe_current_ma(2050,2050u*256u,3315u)==0);
    const int32_t positive = motor_probe_current_ma(2085,2050u*256u,3315u);
    const int32_t negative = motor_probe_current_ma(2015,2050u*256u,3315u);
    assert(positive >= 100 && positive <= 102 && negative == -positive);
    assert(motor_probe_current_ma(4095,0,3600) > 12000);
    assert(motor_probe_current_ma(0,4095u*256u,3600) < -12000);
    MotorProbe probe;
    assert(!motor_probe_reset(&probe,0,0));
    assert(motor_probe_update(&probe,(int32_t[3]){0,0,0})==MOTOR_PROBE_CONFIG);
    assert(probe.duty==0);
    assert(!motor_probe_reset(&probe,3,1));
    assert(motor_probe_update(&probe,(int32_t[3]){0,0,0})==MOTOR_PROBE_CONFIG);
    assert(motor_probe_reset(&probe,0,1));
    assert(motor_probe_update(&probe,(int32_t[3]){INT_MIN,0,0})==MOTOR_PROBE_RAW_CURRENT);
    assert(probe.duty==0);
    assert(motor_probe_reset(&probe,0,1));
    MotorProbeResult result=MOTOR_PROBE_OK;
    for(unsigned int n=0;n<100 && result==MOTOR_PROBE_OK;++n)
        result=motor_probe_update(&probe,(int32_t[3]){-600,600,0});
    assert(result==MOTOR_PROBE_FILTER_CURRENT && probe.duty==0);
    assert(motor_probe_reset(&probe,0,1));
    for(unsigned int n=0;n<2000 && result!=MOTOR_PROBE_NO_RESPONSE;++n)
        result=motor_probe_update(&probe,(int32_t[3]){0,0,0});
    assert(result==MOTOR_PROBE_NO_RESPONSE && probe.duty==0 && !motor_probe_target_seen(&probe));
    assert(motor_probe_reset(&probe,0,1));
    result=MOTOR_PROBE_OK;
    for(unsigned int n=0;n<1000 && result==MOTOR_PROBE_OK;++n)
        result=motor_probe_update(&probe,(int32_t[3]){100,100,0});
    assert(result==MOTOR_PROBE_MAPPING && probe.duty==0);
    assert(motor_probe_reset(&probe,0,1));
    result=MOTOR_PROBE_OK;
    for(unsigned int n=0;n<1000 && result==MOTOR_PROBE_OK;++n)
        result=motor_probe_update(&probe,(int32_t[3]){-100,100,100});
    assert(result==MOTOR_PROBE_MAPPING && probe.duty==0);
    for(uint8_t source=0;source<3;++source)for(uint8_t sink=0;sink<3;++sink)
        if(source!=sink)for(int32_t sign=-1;sign<=1;sign+=2)for(int32_t tau=4;tau<=40;tau*=10)
            check_regulation(source,sink,sign,tau);
    puts("PASS: conversion, sign, limits, no-response, mapping, and 24 delayed/dead-band regulation cases");
    return 0;
}
