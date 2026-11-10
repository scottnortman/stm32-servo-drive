#include "motor_probe_trace.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    MotorProbe probe;
    MotorProbeTrace trace;
    assert(motor_probe_reset(&probe, 0u, 1u));
    motor_probe_trace_reset(&trace);
    assert(motor_probe_trace_frame(&trace, 0u) == 0);
    for (uint32_t n = 1u; n <= 1000u; ++n)
    {
        const uint16_t raw[3] = {(uint16_t)(2000u + n), 2031u, 2028u};
        const int32_t current[3] = {100, -100, 0};
        probe.filtered_q8[0] = 90 * 256;
        probe.filtered_q8[1] = -90 * 256;
        motor_probe_trace_record(&trace, &probe, 19u, raw, current, false);
    }
    assert(trace.samples == 1000u && trace.count == MOTOR_TRACE_CAPACITY);
    for (uint32_t i = 0u; i < MOTOR_TRACE_CAPACITY; ++i)
    {
        const MotorProbeFrame *frame = motor_probe_trace_frame(&trace, i);
        assert(frame && frame->sample == 380u + i * 20u);
        assert(frame->raw[0] == 2000u + frame->sample);
        assert(frame->command_ccr == 19u && frame->nominal_ma[1] == -100);
        assert(frame->filtered_ma[1] == -90);
    }
    assert(motor_probe_trace_frame(&trace, MOTOR_TRACE_CAPACITY) == 0);
    assert(trace.raw_min[0] == 2001u && trace.raw_max[0] == 3000u);

    // Reproduce the observed C excursion and retain this failing completed
    // sample even when it lands between the regular 1ms history entries.
    const uint16_t trip_raw[3] = {2057u, 2031u, 1669u};
    const uint32_t zero[3] = {0x80165u, 0x7eedeu, 0x7e861u};
    int32_t current[3];
    for (unsigned int c = 0u; c < 3u; ++c)
        current[c] = motor_probe_current_ma(trip_raw[c], zero[c], 3316u);
    assert(current[2] == -1027);
    assert(motor_probe_update(&probe, current) == MOTOR_PROBE_RAW_CURRENT);
    motor_probe_trace_record(&trace, &probe, 19u, trip_raw, current, true);
    const MotorProbeFrame *last = motor_probe_trace_frame(&trace, trace.count - 1u);
    assert(last && last->sample == 1001u && last->raw[2] == 1669u);
    assert(last->nominal_ma[2] == current[2] && last->command_ccr == 19u);
    assert(trace.raw_min[2] == 1669u);
    motor_probe_trace_reset(&trace);
    assert(trace.count == 0u && trace.samples == 0u && trace.raw_min[2] == 4095u);
    puts("PASS: chronological ring wrap, decimation, extrema, signed feedback, and retained C-trip trace");
    return 0;
}
