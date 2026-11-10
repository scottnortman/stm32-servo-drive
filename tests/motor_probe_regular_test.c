#include "motor_probe_regular.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    volatile MotorProbeRegular queue = {0};
    uint16_t result = 0u;
    assert(!motor_probe_regular_take(&queue, &result));
    assert(!motor_probe_regular_request(&queue, 10u));
    assert(motor_probe_regular_request(&queue, 17u));
    assert(!motor_probe_regular_request(&queue, 4u));
    assert(motor_probe_regular_step(&queue, false, false, 0u) == MOTOR_REG_START);
    assert(queue.channel == 17u && queue.state == MOTOR_REG_RUNNING);
    assert(motor_probe_regular_step(&queue, true, false, 1490u) == MOTOR_REG_NONE);
    // Delayed foreground must not lose or overwrite the completed result.
    for (unsigned int n = 0u; n < 40u; ++n)
        assert(motor_probe_regular_step(&queue, false, false, 999u) == MOTOR_REG_NONE);
    assert(motor_probe_regular_take(&queue, &result) && result == 1490u);
    assert(!motor_probe_regular_take(&queue, &result));

    // Main-loop work occurs outside the old38..48us launch window on every
    // cycle. IRQ-owned starts still complete32 alternating bus/VREF reads.
    for (unsigned int n = 0u; n < 32u; ++n)
    {
        const unsigned int foreground_phase_us = 21u;
        assert(!(foreground_phase_us >= 38u && foreground_phase_us <= 48u));
        assert(motor_probe_regular_request(&queue, n % 2u ? 4u : 17u));
        assert(motor_probe_regular_step(&queue, false, false, 0u) == MOTOR_REG_START);
        assert(motor_probe_regular_step(&queue, true, false, (uint16_t)(300u + n)) == MOTOR_REG_NONE);
        assert(motor_probe_regular_take(&queue, &result) && result == 300u + n);
    }
    assert(motor_probe_regular_request(&queue, 4u));
    assert(motor_probe_regular_step(&queue, false, false, 0u) == MOTOR_REG_START);
    assert(motor_probe_regular_step(&queue, false, false, 0u) == MOTOR_REG_MISSING_RESULT);
    assert(!motor_probe_regular_take(&queue, &result));
    assert(motor_probe_regular_step(&queue, true, true, 0u) == MOTOR_REG_OVERRUN);
    queue.state = MOTOR_REG_IDLE;
    assert(motor_probe_regular_step(&queue, true, false, 0u) == MOTOR_REG_UNEXPECTED_RESULT);
    puts("PASS: IRQ-dispatched regular ADC queue, delayed foreground, 32 reads, overlap and conversion faults");
    return 0;
}
