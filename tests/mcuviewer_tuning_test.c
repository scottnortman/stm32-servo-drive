#include "repo_tune_link.h"
#include "repo_tune_mcuviewer.h"
#include "stm32f4xx.h"
#include "recorder.h"
#include "tune_gain_store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

TestDwt test_dwt;
TestGpio test_gpiob,test_gpioc;
TestTim test_tim1;
static uint32_t stored[TUNE_GAIN_FLASH_WORDS];
static uint32_t nv_read(uint32_t index) {return stored[index];}
static bool nv_write(uint32_t index,uint32_t value)
{if((stored[index] & value)!=value) return false;stored[index]&=value;return true;}
const TuneGainFlash *tune_gain_stm32_flash(void)
{static const TuneGainFlash flash={nv_read,nv_write};return &flash;}
extern ____Recorder ____recorder;
static void poll(bool armed) {repo_tune_poll(0,armed,0,0);}
int main(void)
{
    memset(stored,0xff,sizeof(stored));
    repo_tune_init();
#ifdef REPO_MOTION_24V
    assert(g_tune_interface_version==0x00010006u);
#elif defined(REPO_TUNING_24V)
    assert(g_tune_interface_version==0x00010005u);
#else
    assert(g_tune_interface_version==0x00010004u);
#endif
    assert(g_tune_nv_status==TUNE_GAIN_EMPTY);
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
    ++g_tune_nv_save_seq;poll(true);assert(g_tune_nv_status==TUNE_GAIN_REJECTED);
    g_tune_kp=0.8f;
    ++g_tune_nv_save_seq;poll(false);assert(g_tune_nv_status==TUNE_GAIN_REJECTED);
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
    test_gpiob.ODR=1;
    ++g_tune_nv_save_seq;poll(false);assert(g_tune_nv_status==TUNE_GAIN_REJECTED);
    test_gpiob.ODR=0;
    test_gpioc.ODR=1u<<6;
    ++g_tune_nv_save_seq;poll(false);assert(g_tune_nv_status==TUNE_GAIN_REJECTED);
    test_gpioc.ODR=0;
    test_tim1.BDTR=TIM_BDTR_MOE;
    ++g_tune_nv_save_seq;poll(false);assert(g_tune_nv_status==TUNE_GAIN_REJECTED);
    test_tim1.BDTR=0;
    ++g_tune_nv_save_seq;repo_tune_poll(0,false,2,0);
    assert(g_tune_nv_status==TUNE_GAIN_REJECTED);
    ++g_tune_nv_save_seq;poll(false);assert(g_tune_nv_status==TUNE_GAIN_SAVED);
    float kp,ki;
    assert(tune_gain_load(tune_gain_stm32_flash(),&kp,&ki)==TUNE_GAIN_LOADED);
    assert(kp==0.8f && ki==50);
    g_tune_kp=0.2f;repo_tune_init();
    assert(g_tune_nv_status==TUNE_GAIN_LOADED && g_tune_kp==0.8f && g_tune_ki==50);
#if defined(REPO_MOTION_24V)
    // The prior persisted pair cannot start the new 24V motion image.
    g_tune_mode=TUNE_D_RECIPROCAL;g_tune_axis=0;g_tune_amplitude=6;
    g_tune_duration=6;g_tune_voltage_limit=4;
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
    ++g_tune_start_seq;poll(true);assert(!repo_tune_take_start() && g_tune_result==3);
    g_tune_kp=1;g_tune_ki=2500;
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
    // Editing RAM alone cannot satisfy the persisted-pair gate.
    ++g_tune_start_seq;poll(true);assert(!repo_tune_take_start() && g_tune_result==3);
    ++g_tune_nv_save_seq;poll(false);assert(g_tune_nv_status==TUNE_GAIN_SAVED);
    repo_tune_init();assert(g_tune_nv_status==TUNE_GAIN_LOADED);
    ++g_tune_start_seq;poll(true);assert(repo_tune_take_start());
    assert(repo_tune_d_reciprocal_selected());
    for(unsigned n=0;n<120001u;++n) repo_tune_control_tick(n,6,0,phase,&vd,&vq);
    poll(true);assert(repo_tune_take_stop());
    repo_tune_disarm();poll(false);
    g_tune_duration=0;++g_tune_apply_seq;poll(true);assert(g_tune_result==2);
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
    ++g_tune_start_seq;poll(true);assert(repo_tune_take_start());
    g_tune_kp=0.9f;++g_tune_apply_seq;poll(true);assert(g_tune_result==2);
    ++g_tune_stop_seq;poll(true);assert(repo_tune_take_stop());
    repo_tune_disarm();poll(false);
#elif !defined(REPO_TUNING_24V)
    // Motion needs the exact persisted pair, a disarmed configuration commit,
    // and a fresh physical arm before its bounded D-axis command can start.
    g_tune_mode=TUNE_D_RECIPROCAL;g_tune_axis=0;g_tune_amplitude=3.0f;
    g_tune_duration=41.0f;g_tune_voltage_limit=2.0f;
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
    ++g_tune_start_seq;poll(true);
    assert(g_tune_result==3 && !repo_tune_take_start());
    g_tune_ki=2000.0f;++g_tune_apply_seq;poll(false);
    assert(g_tune_result==0);
    ++g_tune_nv_save_seq;poll(false);assert(g_tune_nv_status==TUNE_GAIN_SAVED);
    repo_tune_init();assert(g_tune_nv_status==TUNE_GAIN_LOADED);
    ++g_tune_start_seq;poll(true);assert(repo_tune_take_start());
    assert(repo_tune_d_reciprocal_selected() && repo_tune_field_angle()==0.0f);
    g_tune_amplitude=5.0f;g_tune_duration=21.0f;g_tune_voltage_limit=3.2f;
    ++g_tune_apply_seq;poll(true);
    assert(g_tune_result==2); // no motion reconfiguration after arming
    repo_tune_disarm();poll(false);
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
    ++g_tune_start_seq;poll(true);assert(repo_tune_take_start());
    repo_tune_disarm();poll(false);
    g_tune_amplitude=6.0f;g_tune_duration=0.0f;g_tune_voltage_limit=4.0f;
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
    ++g_tune_start_seq;poll(true);assert(repo_tune_take_start());
    ++g_tune_stop_seq;poll(true);
    assert(repo_tune_take_stop());
    repo_tune_disarm();poll(false);
#else
    g_tune_mode=TUNE_D_RECIPROCAL;g_tune_amplitude=6.0f;
    g_tune_duration=0.0f;g_tune_voltage_limit=2.0f;
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==1);
    g_tune_mode=TUNE_CURRENT_CHIRP;g_tune_amplitude=0.5f;
    g_tune_f_start=g_tune_f_end=2000.0f;g_tune_duration=1.0f;
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
    ++g_tune_start_seq;poll(true);assert(repo_tune_take_start());
    repo_tune_disarm();poll(false);
#endif
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
