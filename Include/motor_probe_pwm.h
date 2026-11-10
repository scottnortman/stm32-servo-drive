#pragma once

#include "motor_probe.h"

enum { MOTOR_PWM_ARR = 1250, MOTOR_PWM_CENTER = 625 };

// Stationary pair voltage using the repo's centered-PWM convention.
// Differential compare is rounded DOWN to an even value; the third phase
// stays at the exact midpoint. This does not use the AKM phase permutation.
bool motor_probe_pwm_compares(uint8_t source, uint8_t sink, uint16_t differential,
                              uint16_t compare[3]);
bool motor_probe_pwm_valid(uint8_t source, uint8_t sink, const uint16_t compare[3]);
