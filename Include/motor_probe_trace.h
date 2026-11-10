#pragma once

#include "motor_probe.h"

#ifdef DRV_GATE_SMOOTH
enum { MOTOR_TRACE_CAPACITY = 32, MOTOR_TRACE_DECIMATION = 200 };
#else
enum { MOTOR_TRACE_CAPACITY = 32, MOTOR_TRACE_DECIMATION = 20 };
#endif

typedef struct
{
    uint32_t sample;
    uint16_t command_ccr, raw[3];  //V4: source-minus-sink compare, not leg duty.
    int32_t nominal_ma[3], filtered_ma[3];
#ifdef DRV_GATE_SMOOTH
    int32_t reference_ma[3];
#endif
} MotorProbeFrame;

typedef struct
{
    MotorProbeFrame frames[MOTOR_TRACE_CAPACITY];
    uint32_t samples, count, next;
    uint16_t raw_min[3], raw_max[3];
} MotorProbeTrace;

void motor_probe_trace_reset(MotorProbeTrace *trace);
void motor_probe_trace_record(MotorProbeTrace *trace, const MotorProbe *probe,
                              uint16_t command_ccr, const uint16_t raw[3],
                              const int32_t nominal_ma[3], bool force);
// Call only while sampling is stopped. Entries are returned oldest first.
const MotorProbeFrame *motor_probe_trace_frame(const MotorProbeTrace *trace, uint32_t index);
