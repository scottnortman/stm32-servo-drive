#pragma once

// Bench settings only. The shared PI, sine/PWM and ADC acquisition functions
// remain the existing repository implementations.
#ifdef REPO_RECIPROCAL
#ifdef REPO_CURRENT_6250MA
#define REPO_PHASE_TARGET_A  6.25f
#define REPO_PHASE_VMAX_V    4.0f
#define REPO_FILTER_TRIP_A   7.5f
#define REPO_RAW_TRIP_A      8.0f
#define REPO_ADC_SAMPLE_CODE 1u  //15cycles at25MHz;3.24us for all3channels.
#define REPO_ADC_TRIGGER_CCR 2489u
#elif defined(REPO_CURRENT_5000MA)
#define REPO_PHASE_TARGET_A  5.0f
#define REPO_PHASE_VMAX_V    3.2f
#define REPO_FILTER_TRIP_A   6.5f
#define REPO_RAW_TRIP_A      8.0f
#define REPO_ADC_SAMPLE_CODE 2u  //28cycles at25MHz;4.8us for all3channels.
#elif defined(REPO_CURRENT_2500MA)
#define REPO_PHASE_TARGET_A  2.5f
#define REPO_PHASE_VMAX_V    1.6f
#define REPO_FILTER_TRIP_A   3.3f
#else
#define REPO_PHASE_TARGET_A  2.0f
#define REPO_PHASE_VMAX_V    1.3f
#define REPO_FILTER_TRIP_A   2.8f
#endif
#ifndef REPO_CURRENT_5000MA
#define REPO_RAW_TRIP_A      4.5f
#define REPO_ADC_SAMPLE_CODE 3u
#endif
#elif defined(REPO_DOUBLE_CURRENT)
#define REPO_PHASE_TARGET_A  1.4f
#define REPO_PHASE_VMAX_V    1.1f
#define REPO_FILTER_TRIP_A   2.0f
#define REPO_RAW_TRIP_A      4.0f
#define REPO_ADC_SAMPLE_CODE 3u  //56cycles at25MHz;8.16us for all3channels.
#else
#define REPO_PHASE_TARGET_A  0.7f
#define REPO_PHASE_VMAX_V    0.55f
#define REPO_FILTER_TRIP_A   1.0f
#define REPO_RAW_TRIP_A      2.0f
#define REPO_ADC_SAMPLE_CODE 4u  //Original84cycle setting.
#endif
