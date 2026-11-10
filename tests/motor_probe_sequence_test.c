#include "motor_probe_sequence.h"
#include "motor_probe_pwm.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    MotorProbeStep step;
    unsigned int reverse = 0u, rotating = 0u;
    uint32_t minimum_ms = 0u;
    for (uint32_t n = 0u; n < MOTOR_SEQUENCE_STEPS; ++n)
    {
        assert(motor_probe_sequence_step(n, &step));
        assert(step.source < 3u && step.sink < 3u && step.source != step.sink);
        assert(step.trace_bank < MOTOR_SEQUENCE_TRACES);
        uint16_t compare[3];
        assert(motor_probe_pwm_compares(step.source, step.sink, MOTOR_PROBE_MAX_CCR, compare));
        assert(motor_probe_pwm_valid(step.source, step.sink, compare));
        minimum_ms += step.coast_ms + MOTOR_PROBE_PULSE_MS;
        if (step.rotating) { ++rotating; assert(step.coast_ms == 5u); }
        else { ++reverse; assert(step.coast_ms == 100u && step.trace_bank == n); }
    }
    assert(reverse == 3u && rotating == 25u && minimum_ms == 7425u);
    assert(!motor_probe_sequence_step(MOTOR_SEQUENCE_STEPS, &step));
    assert(!motor_probe_sequence_step(UINT32_MAX, &step));
    assert(motor_probe_sequence_step(0u, &step) && step.source == 1u && step.sink == 0u);
    assert(motor_probe_sequence_step(1u, &step) && step.source == 2u && step.sink == 1u);
    assert(motor_probe_sequence_step(2u, &step) && step.source == 0u && step.sink == 2u);
    assert(motor_probe_sequence_step(MOTOR_SEQUENCE_STEPS-1u, &step) && step.source == 0u && step.sink == 1u);
    for (uint32_t sector = 0u; sector < 6u; ++sector)
    {
        MotorProbeStep current, next;
        assert(motor_probe_sequence_step(3u + sector, &current));
        assert(motor_probe_sequence_step(3u + (sector+1u)%6u, &next));
        int32_t phase[3] = {0}, next_phase[3] = {0};
        phase[current.source] = 1; phase[current.sink] = -1;
        next_phase[next.source] = 1; next_phase[next.sink] = -1;
        // Clarke vector (sqrt(3)*Ia, Ia+2*Ib): equal length, dot/norm=1/2,
        // and positive cross product prove successive vectors advance60deg.
        const int32_t x = phase[0], y = phase[0] + 2*phase[1];
        const int32_t nx = next_phase[0], ny = next_phase[0] + 2*next_phase[1];
        assert(3*x*x + y*y == 4 && 3*nx*nx + ny*ny == 4);
        assert(3*x*nx + y*ny == 2 && x*ny-y*nx == 2);
        for (uint32_t turn = 1u; turn < 4u; ++turn)
        {
            MotorProbeStep repeated;
            assert(motor_probe_sequence_step(3u + turn*6u + sector, &repeated));
            assert(repeated.source == current.source && repeated.sink == current.sink);
            assert(repeated.trace_bank == current.trace_bank);
        }
    }
    puts("PASS: reverse gate, 24 ordered60deg sectors, all PWM bounds, trace indices and one FIELD turn");
    return 0;
}
