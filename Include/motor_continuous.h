#pragma once

#include "motor_probe.h"
#ifdef DRV_GATE_TORQUE_700MA
#include "control.h"
#endif

enum { MOTOR_CONTINUOUS_ALIGN_SAMPLES=10000, MOTOR_CONTINUOUS_SECTOR_SAMPLES=5000,
       MOTOR_CONTINUOUS_SETTLE_SAMPLES=80, MOTOR_CONTINUOUS_MAPPING_GRACE=600 };

typedef struct
{
    uint32_t sector_samples, advances;
    uint8_t sector;
    bool aligned;
#ifdef DRV_GATE_SMOOTH
#ifdef DRV_GATE_TORQUE_700MA
    PIState phase_pi[3];
#else
    int32_t integral_q16[3];
#endif
    uint16_t compare[3];
#endif
} MotorContinuous;

bool motor_continuous_reset(MotorContinuous *state, MotorProbe *probe);
MotorProbeResult motor_continuous_update(MotorContinuous *state, MotorProbe *probe,
                                         const int32_t current_ma[3]);
#ifdef DRV_GATE_SMOOTH
bool motor_smooth_compares(const MotorContinuous *state, uint16_t compare[3]);
bool motor_smooth_pwm_valid(const uint16_t compare[3]);
#endif
