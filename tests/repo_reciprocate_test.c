#include "repo_reciprocate.h"
#include "config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static float last_angle,last_step;
static double electrical_travel;
static unsigned int calls;

// Only the hardware output is stubbed. src/loops.c supplies the real helper.
void pwm_apply_dq(float vd,float vq,float theta)
{
    assert(vd==0.0f && vq==0.9f && isfinite(theta));
    if (theta<0.0f) { theta+=FOC_TWO_PI; }
    assert(theta>=0.0f && theta<FOC_TWO_PI);
    float delta=theta-last_angle;
    if (delta>M_PI) { delta-=FOC_TWO_PI; }
    if (delta<-M_PI) { delta+=FOC_TWO_PI; }
    const float max_step=REPO_RECIP_POLE_PAIRS*M_PI*M_PI/REPO_RECIP_SAMPLES_PER_LEG+0.00002f;
    assert(fabsf(delta)<max_step); //Analytic peak step plus float-rounding allowance.
    last_step=delta; electrical_travel+=delta; last_angle=theta; ++calls;
}

int main(void)
{
    RepoReciprocate state; repo_reciprocate_reset(&state);
    float theta=0.0f;
    for (unsigned int leg=0u;leg<4u;++leg)
    {
        const double start=electrical_travel;
        float previous=state.mechanical_position;
        for (unsigned int sample=0u;sample<REPO_RECIP_SAMPLES_PER_LEG;++sample)
        {
            repo_reciprocate_step(&state,&theta,0.9f);
            assert(state.mechanical_position>=0.0f && state.mechanical_position<=FOC_TWO_PI);
            if (leg%2u==0u) { assert(state.mechanical_position>=previous-0.000001f); }
            else { assert(state.mechanical_position<=previous+0.000001f); }
            if (sample==0u || sample+1u==REPO_RECIP_SAMPLES_PER_LEG)
            { assert(fabsf(last_step)<0.00001f); }
            previous=state.mechanical_position;
        }
        const double expected=(leg%2u==0u ? 1.0 : -1.0)*4.0*FOC_TWO_PI;
        assert(fabs(electrical_travel-start-expected)<0.0001);
        assert(theta==0.0f && state.half_moves==leg+1u);
        assert(state.mechanical_position==(leg%2u==0u ? FOC_TWO_PI : 0.0f));
    }
    assert(calls==4u*REPO_RECIP_SAMPLES_PER_LEG);
    puts("PASS: actual repo helper,4 alternating single-revolution commands,4electrical turns/leg,continuous wraps/reversals,zero endpoint velocity");
    return 0;
}
