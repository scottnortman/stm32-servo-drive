#include "motor_continuous.h"
#include "motor_probe_sequence.h"
#include "motor_probe_pwm.h"

#if !defined(DRV_GATE_SMOOTH) || !defined(DRV_GATE_CONTINUOUS)
#error "Smooth current blend requires its isolated continuous build profile"
#endif

static int32_t absolute(int32_t x) { return x<0 ? -x : x; }
static MotorProbeResult fail(MotorProbe *probe, MotorProbeResult result)
{ probe->duty=0u; return result; }

bool motor_smooth_pwm_valid(const uint16_t compare[3])
{
    for (unsigned int c=0u;c<3u;++c)
    {
        if (compare[c]<MOTOR_PWM_CENTER-MOTOR_PROBE_MAX_CCR/2u ||
            compare[c]>MOTOR_PWM_CENTER+MOTOR_PROBE_MAX_CCR/2u) { return false; }
    }
    return true;
}

bool motor_smooth_compares(const MotorContinuous *state, uint16_t compare[3])
{
    for (unsigned int c=0u;c<3u;++c) { compare[c]=state->compare[c]; }
    return motor_smooth_pwm_valid(compare);
}

bool motor_continuous_reset(MotorContinuous *state, MotorProbe *probe)
{
    *state=(MotorContinuous){0};
    for (unsigned int c=0u;c<3u;++c)
    {
        state->compare[c]=MOTOR_PWM_CENTER;
#ifdef DRV_GATE_TORQUE_700MA
        // Exact shared repo PI implementation; preserve v2's integral gain.
        // Units: error mA, output timer counts, dt50us =>32Q16/mA per step.
        pi_init(&state->phase_pi[c],0.0f,9.765625f,-(float)MOTOR_PROBE_MAX_CCR,(float)MOTOR_PROBE_MAX_CCR);
#endif
    }
    return motor_probe_reset(probe,0u,1u);
}

MotorProbeResult motor_continuous_update(MotorContinuous *state, MotorProbe *probe,
                                         const int32_t current_ma[3])
{
    const uint32_t duration=state->aligned ? MOTOR_CONTINUOUS_SECTOR_SAMPLES : MOTOR_CONTINUOUS_ALIGN_SAMPLES;
    if (state->sector>=6u || state->sector_samples>=duration ||
        probe->source>=3u || probe->sink>=3u || probe->source==probe->sink)
    { return fail(probe,MOTOR_PROBE_CONFIG); }
    // Current trips are unconditional, including alignment and commutation.
    for (unsigned int c=0u;c<3u;++c)
    {
        if (current_ma[c]<-MOTOR_PROBE_RAW_TRIP_MA || current_ma[c]>MOTOR_PROBE_RAW_TRIP_MA)
        { return fail(probe,MOTOR_PROBE_RAW_CURRENT); }
    }
    int32_t measured[3],sum=0;
    for (unsigned int c=0u;c<3u;++c)
    {
        const uint32_t peak=(uint32_t)absolute(current_ma[c]);
        if (peak>probe->peak_raw_ma) { probe->peak_raw_ma=peak; }
        probe->filtered_q8[c]+=(current_ma[c]*256-probe->filtered_q8[c])/16;
        if (absolute(probe->filtered_q8[c])>MOTOR_PROBE_FILTER_TRIP_MA*256)
        { return fail(probe,MOTOR_PROBE_FILTER_CURRENT); }
        measured[c]=probe->filtered_q8[c]/256; sum+=measured[c];
    }
    ++probe->samples;
    // Three conducting phases replace the old unused-phase/pair-sum check.
    // The individually sampled raw/filtered trips above never use this allowance.
    if (probe->samples>600u && absolute(sum)>150)
    { return fail(probe,MOTOR_PROBE_MAPPING); }

    MotorProbeStep next;
    if (!motor_probe_sequence_step(3u+(state->sector+1u)%6u,&next))
    { return fail(probe,MOTOR_PROBE_CONFIG); }
    const int32_t ramp=probe->samples<MOTOR_PROBE_RAMP_SAMPLES ?
        (int32_t)(MOTOR_PROBE_TARGET_MA*probe->samples/MOTOR_PROBE_RAMP_SAMPLES) : MOTOR_PROBE_TARGET_MA;
    const int32_t blend=state->aligned ? (int32_t)state->sector_samples : 0;
    int32_t ref[3]={0};
    ref[probe->source]+=ramp*(MOTOR_CONTINUOUS_SECTOR_SAMPLES-blend);
    ref[probe->sink]-=ramp*(MOTOR_CONTINUOUS_SECTOR_SAMPLES-blend);
    ref[next.source]+=ramp*blend; ref[next.sink]-=ramp*blend;
    // Set C from A+B so rounding cannot introduce a zero-sequence command.
    for (unsigned int c=0u;c<2u;++c) { ref[c]/=MOTOR_CONTINUOUS_SECTOR_SAMPLES; }
    ref[2]=-ref[0]-ref[1];

    int32_t candidate[3],minimum=INT32_MAX,maximum=INT32_MIN,max_error=0;
    for (unsigned int c=0u;c<3u;++c)
    {
        probe->reference_ma[c]=ref[c];
        // Reject common measurement offset from the three-wire current loop.
        const int32_t error=ref[c]-measured[c]+sum/3;
        if (absolute(error)>max_error) { max_error=absolute(error); }
#ifdef DRV_GATE_TORQUE_700MA
        candidate[c]=(int32_t)(pi_step(&state->phase_pi[c],(float)error,0.00005f)*65536.0f);
#else
        candidate[c]=state->integral_q16[c]+error*32;
#endif
        if (candidate[c]<minimum) { minimum=candidate[c]; }
        if (candidate[c]>maximum) { maximum=candidate[c]; }
    }
    const int32_t ceiling=MOTOR_PROBE_MAX_CCR*65536;
    const int32_t span=maximum-minimum, midpoint=minimum+span/2;
    const bool saturated=span>ceiling;
    uint16_t low=UINT16_MAX,high=0u;
    for (unsigned int c=0u;c<3u;++c)
    {
        int32_t value=candidate[c]-midpoint;
        // Vector anti-windup: retain direction, project into the same PWM span.
        if (saturated) { value=(int32_t)((int64_t)value*ceiling/span); }
#ifdef DRV_GATE_TORQUE_700MA
        // Back-calculate the shared PI state from the bounded applied vector.
        state->phase_pi[c].integrator=(float)value/65536.0f;
#else
        state->integral_q16[c]=value;
#endif
        const int32_t offset=value>=0 ? (value+32768)/65536 : -((-value+32768)/65536);
        state->compare[c]=(uint16_t)(MOTOR_PWM_CENTER+offset);
        if (state->compare[c]<low) { low=state->compare[c]; }
        if (state->compare[c]>high) { high=state->compare[c]; }
    }
    probe->duty=high-low;
    if (!motor_smooth_pwm_valid(state->compare)) { return fail(probe,MOTOR_PROBE_CONFIG); }
    if (probe->samples>=MOTOR_PROBE_RAMP_SAMPLES && max_error<=MOTOR_PROBE_TARGET_MA*30/100)
    { ++probe->target_samples; }
    if (saturated && probe->samples>600u && max_error>75)
    {
        if (++probe->saturated_samples>=200u) { return fail(probe,MOTOR_PROBE_NO_RESPONSE); }
    }
    else { probe->saturated_samples=0u; }

    if (++state->sector_samples>=duration)
    {
        if (!motor_probe_target_seen(probe)) { return fail(probe,MOTOR_PROBE_TARGET_MISSED); }
        state->aligned=true; state->sector=(uint8_t)((state->sector+1u)%6u);
        // First rotating interval begins at AB after alignment, not AC.
        if (state->advances==0u) { state->sector=0u; }
        state->sector_samples=0u;
        ++state->advances;
        MotorProbeStep step;
        if (!motor_probe_sequence_step(3u+state->sector,&step)) { return fail(probe,MOTOR_PROBE_CONFIG); }
        probe->source=step.source; probe->sink=step.sink;
        probe->samples=MOTOR_PROBE_RAMP_SAMPLES;
        probe->target_samples=probe->saturated_samples=0u;
    }
    return MOTOR_PROBE_OK;
}
