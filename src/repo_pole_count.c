#include "repo_pole_count.h"
#include "loops.h"
#include "config.h"

void repo_pole_count_step(float *theta,uint32_t *turns,float voltage,float delta)
{
    if (*turns>=REPO_POLE_TEST_TURNS) { delta=0.0f; }
    else if (*turns==REPO_POLE_TEST_TURNS-1u && *theta+delta>=FOC_TWO_PI)
    { delta=FOC_TWO_PI-*theta; }
    const float before=*theta;
    open_loop_step(theta,voltage,delta);
    if (delta>0.0f && *theta<before) { ++*turns; }
}
