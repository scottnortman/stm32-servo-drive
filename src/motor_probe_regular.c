#include "motor_probe_regular.h"

bool motor_probe_regular_request(volatile MotorProbeRegular *queue, uint8_t channel)
{
    if (queue->state != MOTOR_REG_IDLE || (channel != 4u && channel != 17u)) { return false; }
    queue->channel = channel;
    queue->state = MOTOR_REG_REQUESTED;
    return true;
}

bool motor_probe_regular_take(volatile MotorProbeRegular *queue, uint16_t *value)
{
    if (queue->state != MOTOR_REG_READY) { return false; }
    *value = queue->value;
    queue->state = MOTOR_REG_IDLE;
    return true;
}

MotorRegularAction motor_probe_regular_step(volatile MotorProbeRegular *queue,
                                            bool eoc, bool overrun, uint16_t value)
{
    if (overrun) { return MOTOR_REG_OVERRUN; }
    if (queue->state == MOTOR_REG_RUNNING)
    {
        if (!eoc) { return MOTOR_REG_MISSING_RESULT; }
        queue->value = value;
        queue->state = MOTOR_REG_READY;
        return MOTOR_REG_NONE;
    }
    if (eoc) { return MOTOR_REG_UNEXPECTED_RESULT; }
    if (queue->state == MOTOR_REG_REQUESTED)
    {
        queue->state = MOTOR_REG_RUNNING;
        return MOTOR_REG_START;
    }
    return MOTOR_REG_NONE;
}
