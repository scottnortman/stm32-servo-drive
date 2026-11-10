#include "motor_probe_trace.h"

void motor_probe_trace_reset(MotorProbeTrace *trace)
{
    trace->samples = trace->count = trace->next = 0u;
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        trace->raw_min[c] = 4095u;
        trace->raw_max[c] = 0u;
    }
}

void motor_probe_trace_record(MotorProbeTrace *trace, const MotorProbe *probe,
                              uint16_t command_ccr, const uint16_t raw[3],
                              const int32_t nominal_ma[3], bool force)
{
    ++trace->samples;
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        if (raw[c] < trace->raw_min[c]) { trace->raw_min[c] = raw[c]; }
        if (raw[c] > trace->raw_max[c]) { trace->raw_max[c] = raw[c]; }
    }
    if (!force && trace->samples != 1u && trace->samples % MOTOR_TRACE_DECIMATION != 0u) { return; }
    MotorProbeFrame * const frame = &trace->frames[trace->next];
    frame->sample = trace->samples;
    frame->command_ccr = command_ccr;
    for (unsigned int c = 0u; c < 3u; ++c)
    {
        frame->raw[c] = raw[c];
        frame->nominal_ma[c] = nominal_ma[c];
        frame->filtered_ma[c] = probe->filtered_q8[c] / 256;
#ifdef DRV_GATE_SMOOTH
        frame->reference_ma[c] = probe->reference_ma[c];
#endif
    }
    trace->next = (trace->next + 1u) % MOTOR_TRACE_CAPACITY;
    if (trace->count < MOTOR_TRACE_CAPACITY) { ++trace->count; }
}

const MotorProbeFrame *motor_probe_trace_frame(const MotorProbeTrace *trace, uint32_t index)
{
    if (index >= trace->count) { return 0; }
    const uint32_t oldest = trace->count == MOTOR_TRACE_CAPACITY ? trace->next : 0u;
    return &trace->frames[(oldest + index) % MOTOR_TRACE_CAPACITY];
}
