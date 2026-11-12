// MCUViewer is an observer and command mailbox; the repository PI remains the controller.
#include "repo_tune_link.h"
#include "repo_tune_mcuviewer.h"
#include "tune_gain_store.h"
#include "config.h"
#include "stm32f4xx.h"
#include "recorder.h"
#ifdef REPO_MOTION_24V
#include "hall_inputs.h"
#endif

volatile float g_tune_mode=TUNE_CURRENT_STEP,g_tune_axis,g_tune_amplitude=0.5f;
volatile float g_tune_kp=0.2f,g_tune_ki=50,g_tune_f_start=5,g_tune_f_end=200;
volatile float g_tune_duration=1,g_tune_voltage_limit=0.75f;
// RAM interface version also keeps editable scalars away from the SRAM boundary
// that MCUViewer 1.4 currently excludes during automatic address resolution.
#ifdef REPO_MOTION_24V
#define TUNE_INTERFACE_VERSION 0x0001000Eu
#elif defined(REPO_TUNING_24V)
#define TUNE_INTERFACE_VERSION 0x00010005u
#else
#define TUNE_INTERFACE_VERSION 0x00010004u
#endif
volatile uint32_t g_tune_interface_version=TUNE_INTERFACE_VERSION;
volatile uint32_t g_tune_apply_seq,g_tune_start_seq,g_tune_stop_seq;
volatile uint32_t g_tune_nv_save_seq,g_tune_nv_status;
volatile float g_tune_command_mech_rev;
volatile uint32_t g_tune_half_moves;
volatile float g_tune_reference,g_tune_measured,g_tune_cross_current,g_tune_voltage;
volatile uint32_t g_tune_saturated,g_tune_state,g_tune_reason,g_tune_result;
volatile uint32_t g_tune_recorder_max_cycles,g_tune_recorder_fault;
static CurrentTuning tune;
static uint32_t applied,started,stopped,saved;
static bool start_requested,stop_requested,config_ok;
static bool loaded_nv;
static float loaded_kp,loaded_ki;

#ifdef REPO_MOTION_24V
volatile uint32_t g_tune_hall_raw,g_tune_hall_state,g_tune_hall_changes;
volatile uint32_t g_tune_hall_seen,g_tune_hall_invalid,g_tune_hall_multibit;
volatile uint32_t g_tune_hall_period_ticks;
static uint32_t hall_ticks,hall_edge,hall_candidate,hall_count;
static bool hall_ready;

// Observation only: no Hall value can arm, stop or steer the motor.
// Three consecutive 20 kHz samples qualify a state (100..150 us delay).
void repo_tune_hall_tick(void)
{
    ++hall_ticks;
    const uint32_t raw=hall_inputs_read();
    g_tune_hall_raw=raw;
    if(raw!=hall_candidate) {hall_candidate=raw;hall_count=1u;}
    else if(hall_count<3u) ++hall_count;
    if(hall_count<3u || (hall_ready && raw==g_tune_hall_state)) return;
    if(hall_ready) {
        const uint32_t changed=raw^g_tune_hall_state;
        if((changed & (changed-1u))!=0u) ++g_tune_hall_multibit;
        g_tune_hall_period_ticks=g_tune_hall_changes ? hall_ticks-hall_edge : 0u;
        ++g_tune_hall_changes;
    }
    hall_ready=true;hall_edge=hall_ticks;
    g_tune_hall_state=raw;
    g_tune_hall_seen|=1u<<raw;
    // Diagnostic only; 000/111 are invalid under the 120-electrical-degree assumption.
    if(raw==0u || raw==7u) ++g_tune_hall_invalid;
}
#endif

static CurrentTuneConfig requested(void)
{
    return (CurrentTuneConfig){g_tune_mode,g_tune_axis,g_tune_amplitude,
        g_tune_kp,g_tune_ki,g_tune_f_start,g_tune_f_end,g_tune_duration,g_tune_voltage_limit};
}
void repo_tune_init(void)
{
    float kp,ki;
    g_tune_nv_status=tune_gain_load(tune_gain_stm32_flash(),&kp,&ki);
    loaded_nv=g_tune_nv_status==TUNE_GAIN_LOADED;
    if(loaded_nv) { g_tune_kp=loaded_kp=kp; g_tune_ki=loaded_ki=ki; }
    const CurrentTuneConfig p=requested();
    current_tuning_init(&tune,&p);
    config_ok=current_tuning_valid(&p) && g_tune_interface_version==TUNE_INTERFACE_VERSION;
}
static bool same_experiment(const CurrentTuneConfig *a,const CurrentTuneConfig *b)
{
    return a->mode==b->mode && a->axis==b->axis && a->amplitude==b->amplitude &&
        a->f_start==b->f_start && a->f_end==b->f_end &&
        a->duration==b->duration && a->voltage_limit==b->voltage_limit;
}
void repo_tune_poll(uint32_t now,bool armed,uint32_t reason,uint32_t irq_cycles)
{
    (void)now;(void)irq_cycles;
    // Stop has priority over simultaneous apply/start. Sequences are consumed,
    // including rejected commands: a stale write cannot start after re-arming.
    if(g_tune_stop_seq!=stopped) {
        stopped=g_tune_stop_seq;started=g_tune_start_seq;
        stop_requested=true;start_requested=false;
    }
    if(g_tune_apply_seq!=applied) {
        const uint32_t seq=g_tune_apply_seq;
        __DMB();const CurrentTuneConfig p=requested();__DMB();
        applied=seq;
        if(seq!=g_tune_apply_seq || !current_tuning_valid(&p)) {
            config_ok=false;g_tune_result=1;
        } else {
            const uint32_t mask=__get_PRIMASK();__disable_irq();
            if(armed && (p.mode==TUNE_D_RECIPROCAL || tune.config.mode==TUNE_D_RECIPROCAL)) {
                config_ok=false;g_tune_result=2;
            } else if(tune.active && !same_experiment(&p,&tune.config)) g_tune_result=2;
            else {
                if(tune.active) {
                    tune.config=p;tune.d.kp=tune.q.kp=p.kp;tune.d.ki=tune.q.ki=p.ki;
                } else current_tuning_init(&tune,&p);
                config_ok=true;g_tune_result=0;
            }
            __set_PRIMASK(mask);
        }
    }
    if(g_tune_nv_save_seq!=saved) {
        saved=g_tune_nv_save_seq;
        const bool safe=!armed && !tune.active && !start_requested &&
            (reason==0u || reason==1u || reason==9u) &&
            (GPIOB->ODR & 1u)==0u &&
            (GPIOC->ODR & ((1u<<6)|(1u<<7)|(1u<<9)))==0u &&
            (TIM1->BDTR & TIM_BDTR_MOE)==0u &&
            config_ok && g_tune_kp==tune.config.kp && g_tune_ki==tune.config.ki;
        g_tune_nv_status=safe ? tune_gain_save(tune_gain_stm32_flash(),
            tune.config.kp,tune.config.ki) : TUNE_GAIN_REJECTED;
    }
    if(g_tune_start_seq!=started) {
        started=g_tune_start_seq;
        const CurrentTuneConfig r=requested();
        const bool motion_ok=tune.config.mode!=TUNE_D_RECIPROCAL ||
            (loaded_nv && loaded_kp==TUNE_MOTION_KP && loaded_ki==TUNE_MOTION_KI &&
             tune.config.kp==loaded_kp && tune.config.ki==loaded_ki &&
             same_experiment(&r,&tune.config) &&
             r.kp==tune.config.kp && r.ki==tune.config.ki);
        if(!armed || reason || stop_requested || !config_ok ||
           g_tune_recorder_fault || !motion_ok) g_tune_result=3;
        else if(tune.active) g_tune_result=2;
        else {start_requested=true;g_tune_result=0;}
    }
    if(tune.completed) stop_requested=true;
    g_tune_reason=reason;
    g_tune_state=(armed?1u:0u)|(tune.active?2u:0u);
}
bool repo_tune_take_start(void)
{
    if(!start_requested) return false;
    start_requested=false;
    const uint32_t mask=__get_PRIMASK();__disable_irq();
    current_tuning_start(&tune);__set_PRIMASK(mask);
    return true;
}
bool repo_tune_take_stop(void)
{const bool value=stop_requested;stop_requested=false;return value;}
bool repo_tune_link_alive(uint32_t now)
{
    (void)now;
    // SWD has no application heartbeat. The 6 A continuous motion profile relies
    // on physical USER and fault/current/ADC/timing guards until stopped.
    return g_tune_recorder_fault==0;
}
void repo_tune_disarm(void)
{
    current_tuning_stop(&tune);tune.completed=false;start_requested=false;g_tune_state=0;
    g_tune_reference=g_tune_voltage=0;g_tune_saturated=0;
}
bool repo_tune_d_reciprocal_selected(void)
{ return tune.config.mode==TUNE_D_RECIPROCAL; }
float repo_tune_motion_amplitude(void)
{ return tune.config.amplitude; }
float repo_tune_field_angle(void)
{ return current_tuning_field_angle(&tune); }
bool repo_tune_control_tick(uint32_t tick,float id,float iq,const float logical[3],float *vd,float *vq)
{
    (void)tick;(void)logical;
    current_tuning_step(&tune,id,iq);*vd=tune.vd;*vq=tune.vq;
    const bool d=tune.config.axis==0;
    const bool voltage=tune.config.mode==TUNE_VOLTAGE_STEP || tune.config.mode==TUNE_VOLTAGE_CHIRP;
    g_tune_reference=voltage ? (d?*vd:*vq) : (d?tune.id_ref:tune.iq_ref);
    g_tune_measured=d?id:iq;g_tune_cross_current=d?iq:id;
    g_tune_voltage=d?*vd:*vq;g_tune_saturated=tune.saturated;
    g_tune_command_mech_rev=tune.mechanical_position/FOC_TWO_PI;
    g_tune_half_moves=tune.half_moves;
    return tune.active;
}

static bool signal_address(uint32_t address)
{
    return address==(uint32_t)&g_tune_reference || address==(uint32_t)&g_tune_measured ||
        address==(uint32_t)&g_tune_cross_current || address==(uint32_t)&g_tune_voltage ||
        address==(uint32_t)&g_tune_saturated;
}
void repo_tune_recorder_tick(void)
{
    extern ____Recorder ____recorder;
    if(g_tune_recorder_fault) return;
    // Vendor requires recorderStep not to be preempted. Restrict all reads to
    // these five RAM scalars and all copies to <=20 bytes before masking IRQs.
    // Internal DRV protections remain active; software nFAULT EXTI can be
    // deferred by this measured section. Do not change the IRQ priorities.
    const uint32_t mask=__get_PRIMASK();__disable_irq();
    const uint32_t cycle=DWT->CYCCNT;
    if(____recorder.state==____STATE_RUNNING) {
        const uint32_t count=____recorder.variableCount;
        bool ok=count>0 && count<=5 && ____recorder.samplesPackSize==count*4u &&
            ____recorder.maxActualBufferSize<=____RECORDER_BUFFERSIZE &&
            ____recorder.maxActualBufferSize>=count*4u &&
            ____recorder.recorderbuf.head<=____recorder.maxActualBufferSize-count*4u;
        for(uint32_t i=0;ok && i<count;++i)
            ok=____recorder.sampleList[i].size==4 && signal_address(____recorder.sampleList[i].address);
        if(____recorder.trigger.state!=____STATE_SETUP)
            ok=ok && signal_address(____recorder.trigger.sourceAddress) &&
                (____recorder.trigger.type==____F32 || ____recorder.trigger.type==____U32) &&
                ____recorder.trigger.preTriggerSamples<____recorder.maxActualBufferSize/(count?count*4u:4u);
        if(!ok) {____recorder.state=____STATE_INIT;g_tune_recorder_fault=1;}
        else recorderStep();
    } else recorderStep();
    const uint32_t cycles=DWT->CYCCNT-cycle;
    __set_PRIMASK(mask);
    if(cycles>g_tune_recorder_max_cycles) g_tune_recorder_max_cycles=cycles;
    // 8us at 100MHz. A timing/configuration fault disarms through the normal
    // foreground/SysTick guard and inhibits further captures until reset.
    if(cycles>800u) g_tune_recorder_fault=2;
}

bool repo_tune_recorder_self_test(void)
{
    // Boot only, before TIM1 starts and with ENABLE/INLs already LOW. Exercise
    // the full five-variable/float-trigger path and enforce its measured budget.
    extern ____Recorder ____recorder;
    volatile void *signals[]={&g_tune_reference,&g_tune_measured,&g_tune_cross_current,
                              &g_tune_voltage,&g_tune_saturated};
    ____recorder.variableCount=5;____recorder.samplesPackSize=20;
    ____recorder.maxActualBufferSize=32760;____recorder.state=____STATE_RUNNING;
    ____recorder.trigger.state=____STATE_READY;____recorder.trigger.type=____F32;
    ____recorder.trigger.sourceAddress=(uint32_t)&g_tune_reference;
    ____recorder.trigger.preTriggerSamples=10;____recorder.trigger.triggerValueFloat=0.5f;
    for(unsigned i=0;i<5;++i) {
        ____recorder.sampleList[i].address=(uint32_t)signals[i];
        ____recorder.sampleList[i].size=4;
    }
    for(unsigned i=0;i<16;++i) {g_tune_reference=(float)(i&1u);repo_tune_recorder_tick();}
    const bool ok=!g_tune_recorder_fault && ____recorder.samplesSinceStart==16 &&
        ____recorder.recorderbuf.head==320 && ____recorder.trigger.state==____STATE_TRIGGERED;
    g_tune_reference=0;____recorder.state=____STATE_INIT;
    ____recorder.trigger.state=____STATE_SETUP;
    ____recorder.recorderbuf.head=____recorder.recorderbuf.start=0;
    ____recorder.samplesSinceStart=____recorder.skippedSamples=0;
    return ok;
}
