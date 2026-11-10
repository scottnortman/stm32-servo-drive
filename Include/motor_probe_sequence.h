#pragma once

#include <stdbool.h>
#include <stdint.h>

enum { MOTOR_SEQUENCE_REVERSE = 3, MOTOR_SEQUENCE_SECTORS = 24,
       MOTOR_SEQUENCE_HOLDS = 25, MOTOR_SEQUENCE_STEPS = 28, MOTOR_SEQUENCE_TRACES = 9,
       MOTOR_SEQUENCE_POLE_PAIRS = 4, MOTOR_SEQUENCE_SECTORS_PER_ELEC_TURN = 6 };

typedef struct
{
    uint8_t source, sink, trace_bank;
    uint16_t coast_ms;
    bool rotating;
} MotorProbeStep;

bool motor_probe_sequence_step(uint32_t index, MotorProbeStep *step);
