#include "motor_probe_sequence.h"

_Static_assert(MOTOR_SEQUENCE_SECTORS == MOTOR_SEQUENCE_POLE_PAIRS *
               MOTOR_SEQUENCE_SECTORS_PER_ELEC_TURN, "One mechanical FIELD turn");
_Static_assert(MOTOR_SEQUENCE_HOLDS == MOTOR_SEQUENCE_SECTORS + 1,
               "Starting alignment plus24 advances, ending at the initial vector");

bool motor_probe_sequence_step(uint32_t index, MotorProbeStep *step)
{
    static const uint8_t reverse_source[3] = {1u,2u,0u}, reverse_sink[3] = {0u,1u,2u};
    // Positive60-degree electrical progression of the phase-current vector.
    static const uint8_t source[6] = {0u,0u,1u,1u,2u,2u};
    static const uint8_t sink[6] = {1u,2u,2u,0u,0u,1u};
    if (index >= MOTOR_SEQUENCE_STEPS) { return false; }
    if (index < MOTOR_SEQUENCE_REVERSE)
    {
        *step = (MotorProbeStep){reverse_source[index],reverse_sink[index],(uint8_t)index,100u,false};
    }
    else
    {
        const uint32_t sector = (index - MOTOR_SEQUENCE_REVERSE) % 6u;
        *step = (MotorProbeStep){source[sector],sink[sector],(uint8_t)(3u+sector),5u,true};
    }
    return true;
}
