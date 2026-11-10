#pragma once
#include <stdbool.h>
#include <stdint.h>

// Protected phase-pair controller. Nominal7mOhm shunts, verified gain40.
#if (defined(DRV_GATE_CRAWL_250MA) + defined(DRV_GATE_CRAWL_350MA) + defined(DRV_GATE_TORQUE_700MA)) > 1
#error "Select only one current trial"
#endif
#ifdef DRV_GATE_TORQUE_700MA
enum { MOTOR_PROBE_TARGET_MA = 700, MOTOR_PROBE_MAX_CCR = 100 };
#elif defined(DRV_GATE_CRAWL_350MA)
enum { MOTOR_PROBE_TARGET_MA = 350, MOTOR_PROBE_MAX_CCR = 56 };
#elif defined(DRV_GATE_CRAWL_250MA)
enum { MOTOR_PROBE_TARGET_MA = 250, MOTOR_PROBE_MAX_CCR = 40 };
#else
enum { MOTOR_PROBE_TARGET_MA = 100, MOTOR_PROBE_MAX_CCR = 24 };
#endif
#ifdef DRV_GATE_TORQUE_700MA
enum { MOTOR_PROBE_FILTER_TRIP_MA = 1000, MOTOR_PROBE_RAW_TRIP_MA = 2000 };
#else
enum { MOTOR_PROBE_FILTER_TRIP_MA = 500, MOTOR_PROBE_RAW_TRIP_MA = 1000 };
#endif
enum { MOTOR_PROBE_RAMP_SAMPLES = 400, MOTOR_PROBE_PULSE_MS = 250 };
typedef enum
{
    MOTOR_PROBE_OK, MOTOR_PROBE_CONFIG, MOTOR_PROBE_RAW_CURRENT,
    MOTOR_PROBE_FILTER_CURRENT, MOTOR_PROBE_MAPPING, MOTOR_PROBE_NO_RESPONSE,
    MOTOR_PROBE_TARGET_MISSED
} MotorProbeResult;
typedef struct
{
    int32_t filtered_q8[3], integrator_q16;
    uint32_t samples, saturated_samples, target_samples, peak_raw_ma;
    uint16_t duty;
    uint8_t source, sink;
#ifdef DRV_GATE_CONTINUOUS
    uint16_t mapping_grace_samples, commutation_settle_samples;
#endif
#ifdef DRV_GATE_SMOOTH
    int32_t reference_ma[3];
#endif
} MotorProbe;

bool motor_probe_reset(MotorProbe *probe, uint8_t source, uint8_t sink);
int32_t motor_probe_current_ma(uint16_t raw, uint32_t zero_sum, uint32_t vdda_mv);
MotorProbeResult motor_probe_update(MotorProbe *probe, const int32_t current_ma[3]);
bool motor_probe_target_seen(const MotorProbe *probe);
