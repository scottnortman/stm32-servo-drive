#include "motor_continuous.h"
#include "motor_probe_sequence.h"

#ifndef DRV_GATE_CONTINUOUS
#error "Continuous controller must use its own build profile"
#endif

bool motor_continuous_reset(MotorContinuous *state, MotorProbe *probe)
{
    *state=(MotorContinuous){0};
    return motor_probe_reset(probe,0u,1u);
}

MotorProbeResult motor_continuous_update(MotorContinuous *state, MotorProbe *probe,
                                         const int32_t current_ma[3])
{
    if (state->sector>=6u)
    { probe->duty=0u; probe->integrator_q16=0; return MOTOR_PROBE_CONFIG; }
    const MotorProbeResult result=motor_probe_update(probe,current_ma);
    if (result!=MOTOR_PROBE_OK) { return result; }
    const uint32_t duration=state->aligned ? MOTOR_CONTINUOUS_SECTOR_SAMPLES : MOTOR_CONTINUOUS_ALIGN_SAMPLES;
    if (++state->sector_samples<duration) { return MOTOR_PROBE_OK; }
    if (!motor_probe_target_seen(probe))
    { probe->duty=0u; probe->integrator_q16=0; return MOTOR_PROBE_TARGET_MISSED; }
    state->aligned=true;
    state->sector=(uint8_t)((state->sector+1u)%6u);
    state->sector_samples=0u; ++state->advances;
    MotorProbeStep step;
    if (!motor_probe_sequence_step(3u+state->sector,&step))
    { probe->duty=0u; probe->integrator_q16=0; return MOTOR_PROBE_CONFIG; }
    // Keep the actual current filters and voltage demand. No zero-current ramp,
    // no coast interval and no timer/bridge shutdown at a sector transition.
    probe->source=step.source; probe->sink=step.sink;
    probe->samples=MOTOR_PROBE_RAMP_SAMPLES;
    probe->saturated_samples=probe->target_samples=probe->peak_raw_ma=0u;
    probe->mapping_grace_samples=MOTOR_CONTINUOUS_MAPPING_GRACE;
    probe->commutation_settle_samples=MOTOR_CONTINUOUS_SETTLE_SAMPLES;
    return MOTOR_PROBE_OK;
}
