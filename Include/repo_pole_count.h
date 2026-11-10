#pragma once
#include <stdint.h>

enum { REPO_POLE_TEST_TURNS=10u };
// Count the actual wrap of the existing open_loop_step angle. Hold its final
// field after ten turns; no assumption about the motor's pole count is used.
void repo_pole_count_step(float *theta,uint32_t *turns,float voltage,float delta);
