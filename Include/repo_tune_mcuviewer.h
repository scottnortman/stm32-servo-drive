#pragma once
#include "current_tuning.h"
#include <stdint.h>

// Edit the request, then increment apply_seq. A start also requires USER arming.
extern volatile float g_tune_mode,g_tune_axis,g_tune_amplitude,g_tune_kp,g_tune_ki;
extern volatile float g_tune_f_start,g_tune_f_end,g_tune_duration,g_tune_voltage_limit;
extern volatile uint32_t g_tune_interface_version;
extern volatile uint32_t g_tune_apply_seq, g_tune_start_seq, g_tune_stop_seq;
extern volatile uint32_t g_tune_nv_save_seq, g_tune_nv_status;
extern volatile float g_tune_command_mech_rev;
extern volatile uint32_t g_tune_half_moves;
extern volatile float g_tune_reference, g_tune_measured, g_tune_cross_current, g_tune_voltage;
extern volatile uint32_t g_tune_saturated, g_tune_state, g_tune_reason, g_tune_result;
extern volatile uint32_t g_tune_recorder_max_cycles, g_tune_recorder_fault;
void repo_tune_recorder_tick(void);
bool repo_tune_recorder_self_test(void);
#ifdef REPO_MOTION_24V
extern volatile uint32_t g_tune_hall_raw,g_tune_hall_state,g_tune_hall_changes;
extern volatile uint32_t g_tune_hall_seen,g_tune_hall_invalid,g_tune_hall_multibit;
extern volatile uint32_t g_tune_hall_period_ticks;
void repo_tune_hall_tick(void);
#endif
