#include "repo_pole_count.h"
#include "config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static float last_angle;
static unsigned int calls;

// Hardware output sink only. The angle-advance/wrap function is the actual
// existing src/loops.c implementation, not a reimplementation in the test.
void pwm_apply_dq(float vd,float vq,float theta)
{
    assert(vd==0.0f && vq==0.47f);
    assert(isfinite(theta) && theta>=0.0f && theta<FOC_TWO_PI);
    float advance=theta-last_angle;
    if (advance<0.0f) { advance+=FOC_TWO_PI; }
    assert(advance<=FOC_TWO_PI*0.1f*DT_CURRENT+0.000001f);
    last_angle=theta; ++calls;
}

int main(void)
{
    float theta=0.0f; uint32_t turns=0u;
    const float delta=FOC_TWO_PI*0.1f*DT_CURRENT;
    for (unsigned int n=0u;n<20000u;++n)
    { repo_pole_count_step(&theta,&turns,0.47f,0.0f); }
    assert(theta==0.0f && turns==0u);
    unsigned int samples=0u;
    while (turns<REPO_POLE_TEST_TURNS && samples<2100000u)
    {
        const uint32_t before=turns;
        repo_pole_count_step(&theta,&turns,0.47f,delta);
        assert(turns==before || turns==before+1u); ++samples;
    }
    assert(turns==10u && theta==0.0f && samples>1900000u && samples<2100000u);
    for (unsigned int n=0u;n<20000u;++n)
    { repo_pole_count_step(&theta,&turns,0.47f,delta); assert(turns==10u && theta==0.0f); }
    printf("PASS: original open_loop_step,10 electrical wraps in%u samples,continuous phase,final0angle held;calls=%u\n",samples,calls);
    return 0;
}
