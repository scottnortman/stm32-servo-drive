#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "control.h"

// Runtime experiment settings. Controllers are the repository PI implementation.
typedef struct {
    float mode, axis, amplitude, kp, ki, f_start, f_end, duration, voltage_limit;
} CurrentTuneConfig;
typedef struct {
    CurrentTuneConfig config;
    PIState d, q;
    uint32_t sample, samples;
    float phase, id_ref, iq_ref, vd, vq;
    bool active, saturated;
} CurrentTuning;
enum { TUNE_VOLTAGE_STEP, TUNE_CURRENT_STEP, TUNE_CURRENT_CHIRP, TUNE_VOLTAGE_CHIRP };
bool current_tuning_valid(const CurrentTuneConfig *p);
void current_tuning_init(CurrentTuning *s, const CurrentTuneConfig *p);
void current_tuning_start(CurrentTuning *s);
void current_tuning_stop(CurrentTuning *s);
void current_tuning_step(CurrentTuning *s, float id, float iq);
