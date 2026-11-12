#include "repo_tune_link.h"
#include "repo_tune_mcuviewer.h"
#include "stm32f4xx.h"
#include "recorder.h"
#include "tune_gain_store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef REPO_MOTION_24V
#include "hall_inputs.h"
static void hall_sample(uint32_t state,unsigned count)
{
    test_gpiob.IDR=((state>>2)&1u)<<6 | ((state>>1)&1u)<<7 | (state&1u)<<8;
    while(count--) repo_tune_hall_tick();
}
static void hall_test(void)
{
    test_gpiob.MODER=0xa5a5a5a5u;test_gpiob.PUPDR=0x5a5a5a5au;
    hall_inputs_init();
    assert(test_gpiob.MODER==(0xa5a5a5a5u & ~HALL_GPIO_MASK));
    assert(test_gpiob.PUPDR==((0x5a5a5a5au & ~HALL_GPIO_MASK)|HALL_GPIO_PULLUPS));
    hall_sample(1,3);
    assert(g_tune_hall_state==1 && g_tune_hall_changes==0);
    hall_sample(7,1); // A brief noise pulse must not become a qualified state.
    assert(g_tune_hall_raw==7 && g_tune_hall_state==1);
    hall_sample(1,3);assert(g_tune_hall_invalid==0);
    const uint32_t sequence[]={5,4,6,2,3,1,3,2,6,4,5,1};
    for(unsigned n=0;n<sizeof(sequence)/sizeof(sequence[0]);++n) {
        hall_sample(sequence[n],2);assert(g_tune_hall_changes==n);
        hall_sample(sequence[n],1);assert(g_tune_hall_state==sequence[n]);
    }
    assert(g_tune_hall_changes==12 && g_tune_hall_seen==0x7eu);
    assert(g_tune_hall_multibit==0 && g_tune_hall_period_ticks==3);
    hall_sample(6,3);assert(g_tune_hall_multibit==1);
    hall_sample(7,3);hall_sample(0,3);
    assert(g_tune_hall_invalid==2 && g_tune_hall_multibit==2);
    assert(test_gpiob.ODR==0 && test_tim1.BDTR==0 && g_tune_state==0);
}
#endif

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
    assert(g_tune_interface_version==0x0001000Eu);
    hall_test();
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
    for(unsigned amps=1;amps<=5;++amps) {
        g_tune_amplitude=(float)amps;g_tune_voltage_limit=tune_motion_voltage_ceiling((float)amps);
        ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
        ++g_tune_start_seq;poll(false);assert(!repo_tune_take_start());
        ++g_tune_start_seq;poll(true);assert(repo_tune_take_start());
        for(unsigned n=0;n<120001u;++n) repo_tune_control_tick(n,(float)amps,0,phase,&vd,&vq);
        poll(true);assert(repo_tune_take_stop());
        repo_tune_disarm();poll(false);
    }
    g_tune_amplitude=5;g_tune_voltage_limit=3.2f;g_tune_duration=0;
    ++g_tune_apply_seq;poll(false);assert(g_tune_result==0);
    ++g_tune_start_seq;poll(true);assert(repo_tune_take_start());
    for(unsigned n=0;n<220000u;++n) repo_tune_control_tick(n,5,0,phase,&vd,&vq);
    poll(true);assert(!repo_tune_take_stop()); // Continues beyond the bounded cycle.
    ++g_tune_stop_seq;poll(true);assert(repo_tune_take_stop());
    repo_tune_disarm();poll(false);
    g_tune_amplitude=6;g_tune_voltage_limit=4;
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
