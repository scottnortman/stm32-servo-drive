#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef REPO_RECIP_QUAD_SPEED
enum { REPO_RECIP_SAMPLES_PER_LEG=100000u, REPO_RECIP_POLE_PAIRS=4u };
#elif defined(REPO_RECIP_DOUBLE_SPEED)
enum { REPO_RECIP_SAMPLES_PER_LEG=200000u, REPO_RECIP_POLE_PAIRS=4u };
#else
enum { REPO_RECIP_SAMPLES_PER_LEG=400000u, REPO_RECIP_POLE_PAIRS=4u };
#endif
typedef struct
{
    uint32_t sample,half_moves;
    float mechanical_position;
    bool reverse;
} RepoReciprocate;

void repo_reciprocate_reset(RepoReciprocate *state);
void repo_reciprocate_step(RepoReciprocate *state,float *theta,float voltage);
