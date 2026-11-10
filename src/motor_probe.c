#include "motor_probe.h"

static int32_t magnitude(int32_t value) { return value < 0 ? -value : value; }
static MotorProbeResult fail(MotorProbe *probe, MotorProbeResult result)
{
    probe->duty = 0u; probe->integrator_q16 = 0;
    return result;
}

bool motor_probe_reset(MotorProbe *probe, uint8_t source, uint8_t sink)
{
    *probe = (MotorProbe){0};
    probe->source = source; probe->sink = sink;
    return source < 3u && sink < 3u && source != sink;
}

int32_t motor_probe_current_ma(uint16_t raw, uint32_t zero_sum, uint32_t vdda_mv)
{
    // Zero is the sum of256 fresh samples, avoiding a rounded ADC offset.
    // I_mA = delta_counts * VDDA_mV *1000 / (4095 *40 *7mOhm).
    const int64_t delta = (int64_t)raw * 256 - zero_sum;
    return (int32_t)(delta * vdda_mv * 1000 / (256ll * 4095 * 40 * 7));
}

MotorProbeResult motor_probe_update(MotorProbe *probe, const int32_t current_ma[3])
{
    if (probe->source >= 3u || probe->sink >= 3u || probe->source == probe->sink)
    {
        return fail(probe, MOTOR_PROBE_CONFIG);
    }
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        // Compare before abs: even an INT32_MIN fault input cannot overflow.
        if (current_ma[c] < -MOTOR_PROBE_RAW_TRIP_MA ||
            current_ma[c] > MOTOR_PROBE_RAW_TRIP_MA)
        {
            return fail(probe, MOTOR_PROBE_RAW_CURRENT);
        }
    }
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        const int32_t absolute = magnitude(current_ma[c]);
        if ((uint32_t)absolute > probe->peak_raw_ma) { probe->peak_raw_ma = (uint32_t)absolute; }
        probe->filtered_q8[c] += (current_ma[c] * 256 - probe->filtered_q8[c]) / 16;
        if (magnitude(probe->filtered_q8[c]) > MOTOR_PROBE_FILTER_TRIP_MA * 256)
        {
            return fail(probe, MOTOR_PROBE_FILTER_CURRENT);
        }
    }
    ++probe->samples;
    const int32_t source = probe->filtered_q8[probe->source] / 256;
    const int32_t sink = probe->filtered_q8[probe->sink] / 256;
    const unsigned int unused = 3u - probe->source - probe->sink;
#ifdef DRV_GATE_CONTINUOUS
    const bool settling=probe->commutation_settle_samples!=0u;
    if (settling) { --probe->commutation_settle_samples; }
    if (probe->mapping_grace_samples!=0u) { --probe->mapping_grace_samples; }
    if (probe->samples > 600u && magnitude(sink) > 40 && probe->mapping_grace_samples==0u)
#else
    if (probe->samples > 600u && magnitude(sink) > 40)
#endif
    {
        if (magnitude(source + sink) > 75 || magnitude(probe->filtered_q8[unused] / 256) > 75 ||
            (magnitude(source) > 40 && source * sink >= 0))
        {
            return fail(probe, MOTOR_PROBE_MAPPING);
        }
    }
    const int32_t reference = probe->samples < MOTOR_PROBE_RAMP_SAMPLES ?
        (int32_t)(MOTOR_PROBE_TARGET_MA * probe->samples / MOTOR_PROBE_RAMP_SAMPLES) : MOTOR_PROBE_TARGET_MA;
    // Slow integral regulation, bounded differential compare, no R/L assumption.
    const int32_t error = reference - magnitude(sink);
#ifdef DRV_GATE_CONTINUOUS
    if (!settling) { probe->integrator_q16 += error * 64; }
#else
    probe->integrator_q16 += error * 64;
#endif
    if (probe->integrator_q16 < 0) { probe->integrator_q16 = 0; }
    if (probe->integrator_q16 > MOTOR_PROBE_MAX_CCR * 65536)
    {
        probe->integrator_q16 = MOTOR_PROBE_MAX_CCR * 65536;
    }
    probe->duty = (uint16_t)(probe->integrator_q16 / 65536);
#ifdef DRV_GATE_CONTINUOUS
    if (!settling && probe->duty == MOTOR_PROBE_MAX_CCR && magnitude(sink) < reference - 20)
#else
    if (probe->duty == MOTOR_PROBE_MAX_CCR && magnitude(sink) < reference - 20)
#endif
    {
        if (++probe->saturated_samples >= 200u) { return fail(probe, MOTOR_PROBE_NO_RESPONSE); }
    }
    else { probe->saturated_samples = 0u; }
    if (probe->samples >= MOTOR_PROBE_RAMP_SAMPLES &&
        magnitude(sink) >= MOTOR_PROBE_TARGET_MA * 70 / 100 &&
        magnitude(sink) <= MOTOR_PROBE_TARGET_MA * 130 / 100 &&
        magnitude(source) >= 30 && source * sink < 0)
    {
        ++probe->target_samples;
    }
    return MOTOR_PROBE_OK;
}

bool motor_probe_target_seen(const MotorProbe *probe)
{
    return probe->target_samples >= 100u;
}
