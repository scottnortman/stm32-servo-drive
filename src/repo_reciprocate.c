#include "repo_reciprocate.h"
#include "loops.h"
#include "config.h"
#include <math.h>

void repo_reciprocate_reset(RepoReciprocate *state)
{ *state=(RepoReciprocate){0}; }

void repo_reciprocate_step(RepoReciprocate *state,float *theta,float voltage)
{
    const bool endpoint=++state->sample>=REPO_RECIP_SAMPLES_PER_LEG;
    const float u=(float)state->sample/(float)REPO_RECIP_SAMPLES_PER_LEG;
    // The same cosine position-command pattern used by run_cine_sweep, with
    // fixed frequency and offset/scaling to travel0->one revolution->0.
    float position=M_PI*(1.0f-cosf(M_PI*u));
    if (state->reverse) { position=FOC_TWO_PI-position; }
    if (endpoint) { position=state->reverse ? 0.0f : FOC_TWO_PI; }
    state->mechanical_position=position;
    const float command=fmodf((float)REPO_RECIP_POLE_PAIRS*position,FOC_TWO_PI);
    float delta=command-*theta;
    if (delta>M_PI) { delta-=FOC_TWO_PI; }
    if (delta<-M_PI) { delta+=FOC_TWO_PI; }
    // Actual repo helper produces the waveform. Negative angles are periodic
    // for its sin/cos output; canonicalize its returned accumulator here.
    open_loop_step(theta,voltage,delta);
    if (*theta<0.0f) { *theta+=FOC_TWO_PI; }
    if (endpoint)
    {
        state->sample=0u; ++state->half_moves; state->reverse=!state->reverse;
    }
}
