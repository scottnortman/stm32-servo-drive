#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum { MOTOR_REG_IDLE, MOTOR_REG_REQUESTED, MOTOR_REG_RUNNING, MOTOR_REG_READY } MotorRegularState;
typedef enum { MOTOR_REG_NONE, MOTOR_REG_START, MOTOR_REG_OVERRUN, MOTOR_REG_MISSING_RESULT,
               MOTOR_REG_UNEXPECTED_RESULT } MotorRegularAction;
typedef struct
{
    MotorRegularState state;
    uint8_t channel;
    uint16_t value;
} MotorProbeRegular;

bool motor_probe_regular_request(volatile MotorProbeRegular *queue, uint8_t channel);
bool motor_probe_regular_take(volatile MotorProbeRegular *queue, uint16_t *value);
// Service once at injected-sequence completion, before current-loop work.
MotorRegularAction motor_probe_regular_step(volatile MotorProbeRegular *queue,
                                            bool eoc, bool overrun, uint16_t value);
