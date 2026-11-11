#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "control.h"

#ifdef REPO_TUNING_24V
#define TUNE_MAX_FREQUENCY_HZ 2000.0f
#else
#define TUNE_MAX_FREQUENCY_HZ 800.0f
#endif

// Runtime experiment settings. Controllers are the repository PI implementation.
typedef struct {
    float mode, axis, amplitude, kp, ki, f_start, f_end, duration, voltage_limit;
} CurrentTuneConfig;
typedef struct {
    CurrentTuneConfig config;
    PIState d, q;
    uint32_t sample, samples, prepared_sample, half_moves;
    float phase, id_ref, iq_ref, vd, vq, field_theta, mechanical_position;
    bool active, saturated;
    volatile bool completed;
} CurrentTuning;
enum { TUNE_VOLTAGE_STEP, TUNE_CURRENT_STEP, TUNE_CURRENT_CHIRP,
       TUNE_VOLTAGE_CHIRP, TUNE_D_RECIPROCAL };
bool current_tuning_valid(const CurrentTuneConfig *p);
void current_tuning_init(CurrentTuning *s, const CurrentTuneConfig *p);
void current_tuning_start(CurrentTuning *s);
void current_tuning_stop(CurrentTuning *s);
float current_tuning_field_angle(CurrentTuning *s);
void current_tuning_step(CurrentTuning *s, float id, float iq);
