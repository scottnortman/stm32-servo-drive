#include "motor_continuous.h"
#include "motor_probe_pwm.h"
#include <assert.h>
#include <stdio.h>

static int32_t absolute(int32_t x) { return x<0 ? -x : x; }

static void run_blended(int32_t delay,int32_t gain,int32_t dead)
{
    MotorContinuous state; MotorProbe probe;
    assert(motor_continuous_reset(&state,&probe));
    int32_t plant[3]={0},old_ref[3]={0};
    uint16_t old_compare[3]={625,625,625};
    uint32_t seen=0u;
    for (uint32_t n=0u;n<400000u;++n)
    {
        int32_t voltage[3],sum=0,current[3];
        for (unsigned int c=0u;c<3u;++c)
        {
            int32_t u=(int32_t)state.compare[c]-MOTOR_PWM_CENTER;
            voltage[c]=u>dead ? u-dead : (u<-dead ? u+dead : 0); sum+=voltage[c];
        }
        for (unsigned int c=0u;c<3u;++c)
        {
            const int32_t target=(voltage[c]*3-sum)*gain*256/3;
            plant[c]+=(target-plant[c])/delay;
            current[c]=plant[c]/256;
        }
        const MotorProbeResult result=motor_continuous_update(&state,&probe,current);
        if (result!=MOTOR_PROBE_OK)
        { printf("FAIL delay=%ld gain=%ld dead=%ld sample=%lu result=%u I=%ld/%ld/%ld ref=%ld/%ld/%ld duty=%u\n",
                 (long)delay,(long)gain,(long)dead,(unsigned long)n,(unsigned int)result,
                 (long)current[0],(long)current[1],(long)current[2],
                 (long)probe.reference_ma[0],(long)probe.reference_ma[1],(long)probe.reference_ma[2],probe.duty); }
        assert(result==MOTOR_PROBE_OK);
        assert(motor_smooth_pwm_valid(state.compare));
        assert(probe.reference_ma[0]+probe.reference_ma[1]+probe.reference_ma[2]==0);
        for (unsigned int c=0u;c<3u;++c)
        {
            assert(absolute(probe.reference_ma[c])<=MOTOR_PROBE_TARGET_MA);
            assert(absolute(probe.reference_ma[c]-old_ref[c])<=(MOTOR_PROBE_TARGET_MA+MOTOR_PROBE_RAMP_SAMPLES-1)/MOTOR_PROBE_RAMP_SAMPLES);
            assert(absolute((int32_t)state.compare[c]-old_compare[c])<=1);
            if (n>2000u && absolute(probe.reference_ma[c]-current[c])>=105)
            { fprintf(stderr,"TRACK n=%lu delay=%ld gain=%ld dead=%ld phase=%u ref=%ld measured=%ld pwm=%u/%u/%u\n",(unsigned long)n,(long)delay,(long)gain,(long)dead,c,(long)probe.reference_ma[c],(long)current[c],state.compare[0],state.compare[1],state.compare[2]); assert(0); }
            old_ref[c]=probe.reference_ma[c]; old_compare[c]=state.compare[c];
        }
        seen|=1u<<state.sector;
    }
    assert(seen==63u && state.aligned);
    assert(state.advances==79u);
}

int main(void)
{
#ifdef DRV_GATE_TORQUE_700MA
    assert(MOTOR_PROBE_TARGET_MA==700 && MOTOR_PROBE_FILTER_TRIP_MA==1000 && MOTOR_PROBE_RAW_TRIP_MA==2000);
#else
    assert(MOTOR_PROBE_TARGET_MA==350 && MOTOR_PROBE_FILTER_TRIP_MA==500 && MOTOR_PROBE_RAW_TRIP_MA==1000);
#endif
    run_blended(4,20,8); run_blended(40,20,8);
    run_blended(4,35,4); run_blended(40,35,4);
    MotorContinuous state; MotorProbe probe;
    assert(motor_continuous_reset(&state,&probe));
    assert(motor_continuous_update(&state,&probe,(int32_t[3]){0,0,-MOTOR_PROBE_RAW_TRIP_MA-1})==MOTOR_PROBE_RAW_CURRENT);
    assert(motor_continuous_reset(&state,&probe));
    probe.filtered_q8[0]=(MOTOR_PROBE_FILTER_TRIP_MA+100)*256;
    assert(motor_continuous_update(&state,&probe,(int32_t[3]){MOTOR_PROBE_FILTER_TRIP_MA+100,0,0})==MOTOR_PROBE_FILTER_CURRENT);
    assert(motor_continuous_reset(&state,&probe));
    probe.samples=601u; probe.filtered_q8[0]=probe.filtered_q8[1]=probe.filtered_q8[2]=100*256;
    assert(motor_continuous_update(&state,&probe,(int32_t[3]){100,100,100})==MOTOR_PROBE_MAPPING);
    assert(motor_continuous_reset(&state,&probe));
    MotorProbeResult result=MOTOR_PROBE_OK;
    for (unsigned int n=0u;n<10000u && result==MOTOR_PROBE_OK;++n)
    { result=motor_continuous_update(&state,&probe,(int32_t[3]){0,0,0}); }
    assert(result==MOTOR_PROBE_NO_RESPONSE || result==MOTOR_PROBE_TARGET_MISSED);
    const uint16_t bad[3]={MOTOR_PWM_CENTER-MOTOR_PROBE_MAX_CCR/2u-1u,625,MOTOR_PWM_CENTER+MOTOR_PROBE_MAX_CCR/2u+1u}; assert(!motor_smooth_pwm_valid(bad));
    assert(motor_continuous_reset(&state,&probe)); state.sector=6u;
    assert(motor_continuous_update(&state,&probe,(int32_t[3]){0,0,0})==MOTOR_PROBE_CONFIG);
    puts("PASS:20s blended signed phase currents,all6sectors,no reference jumps,tracking/PWM bounds,raw/filter/sum/no-response protections");
    return 0;
}
