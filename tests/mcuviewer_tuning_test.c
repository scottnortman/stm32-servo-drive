#include "repo_tune_link.h"
#include "repo_tune_mcuviewer.h"
#include "stm32f4xx.h"
#include "recorder.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

TestDwt test_dwt;
extern ____Recorder ____recorder;
static void poll(bool armed) {repo_tune_poll(0,armed,0,0);}
int main(void)
{
    repo_tune_init();
    assert(repo_tune_recorder_self_test());
    ++g_tune_start_seq;poll(false);assert(!repo_tune_take_start() && g_tune_result==3);
    poll(true);assert(!repo_tune_take_start()); // no deferred/replayed start
    g_tune_amplitude=NAN;++g_tune_apply_seq;poll(true);
    ++g_tune_start_seq;poll(true);assert(!repo_tune_take_start());
    g_tune_amplitude=0.5f;++g_tune_apply_seq;poll(true);assert(g_tune_result==0);
    ++g_tune_start_seq;poll(true);assert(repo_tune_take_start());
    g_tune_axis=1;++g_tune_apply_seq;poll(true);assert(g_tune_result==2);
    g_tune_axis=0;g_tune_kp=0.3f;++g_tune_apply_seq;poll(true);
    assert(g_tune_result==0); // gains can change while the experiment is active
    const float phase[3]={0};float vd,vq;
    for(unsigned n=0;n<5000;++n) repo_tune_control_tick(n,0,0,phase,&vd,&vq);
    assert(g_tune_reference==0.5f && vd>0 && vq==0);
    ++g_tune_start_seq;++g_tune_stop_seq;poll(true);
    assert(repo_tune_take_stop() && !repo_tune_take_start());
    repo_tune_disarm();assert(g_tune_reference==0 && g_tune_voltage==0);
    poll(false);poll(true);assert(!repo_tune_take_start());
    // Exercise the actual vendor sampler with five real scalar addresses.
    volatile void *signals[]={&g_tune_reference,&g_tune_measured,&g_tune_cross_current,
                              &g_tune_voltage,&g_tune_saturated};
    ____recorder.variableCount=5;____recorder.samplesPackSize=20;
    ____recorder.maxActualBufferSize=100;____recorder.state=____STATE_RUNNING;
    for(unsigned i=0;i<5;++i) {
        ____recorder.sampleList[i].address=(uint32_t)(uintptr_t)signals[i];
        ____recorder.sampleList[i].size=4;
    }
    g_tune_reference=0.5f;g_tune_measured=0.49f;
    for(unsigned n=0;n<5;++n)repo_tune_recorder_tick();
    assert(____recorder.state==____STATE_FULL && !g_tune_recorder_fault);
    float captured;memcpy(&captured,____recorder.recorderbuf.data+4,4);
    assert(captured==0.49f);
    // An out-of-bounds capture is rejected before a vendor copy can happen.
    ____recorder.state=____STATE_RUNNING;____recorder.variableCount=6;
    repo_tune_recorder_tick();assert(g_tune_recorder_fault==1 && !repo_tune_link_alive(0));
    puts("PASS: physical-arm gate, rejected/stale commands, gain commit, stop priority, real five-signal Recorder copy, invalid capture bounds");
}
