#include "current_tuning.h"
#include "config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static CurrentTuneConfig settings(void)
{ return (CurrentTuneConfig){TUNE_CURRENT_STEP,0,0.5f,0.2f,50,5,200,1,0.75f}; }
int main(void)
{
    CurrentTuneConfig p=settings();assert(current_tuning_valid(&p));
    p.kp=NAN;assert(!current_tuning_valid(&p));p=settings();
    p.voltage_limit=4.01f;assert(!current_tuning_valid(&p));p=settings();
    p.amplitude=6.26f;assert(!current_tuning_valid(&p));p=settings();
    p.axis=2;assert(!current_tuning_valid(&p));p=settings();
    p.duration=0;assert(!current_tuning_valid(&p));p=settings();
    p.f_end=TUNE_MAX_FREQUENCY_HZ;assert(current_tuning_valid(&p));
    p.f_end=TUNE_MAX_FREQUENCY_HZ+1.0f;assert(!current_tuning_valid(&p));p=settings();
    p.mode=TUNE_VOLTAGE_STEP;p.amplitude=1;assert(!current_tuning_valid(&p));
    for(unsigned axis=0;axis<2;++axis) {
        p=settings();p.axis=(float)axis;
        CurrentTuning s;current_tuning_init(&s,&p);current_tuning_start(&s);
        float id=0,iq=0,peak=0;
        // PhysicalRL plant, separate from the implementation. Exact discrete
        // response avoids a numerical integration artifact in the check.
        const float decay=expf(-0.6f*DT_CURRENT/0.0003f);
        for(unsigned n=0;n<20000;++n) {
            current_tuning_step(&s,id,iq);
            id=id*decay+s.vd/0.6f*(1-decay);
            iq=iq*decay+s.vq/0.6f*(1-decay);
            const float measured=axis==0 ? id : iq;
            if(measured>peak) peak=measured;
            if(n>8000 && n<12000) assert(fabsf(measured-0.5f)<0.005f);
            assert(fabsf(axis==0 ? iq : id)<1e-6f);
            assert(hypotf(s.vd,s.vq)<=p.voltage_limit+1e-5f);
        }
        assert(peak<0.525f);current_tuning_step(&s,id,iq);
        assert(!s.active && s.vd==0 && s.vq==0);
    }
    // Worst-error cross-axis demand must respect the TOTAL vector ceiling.
    p=settings();p.kp=10;p.ki=10000;
    CurrentTuning s;current_tuning_init(&s,&p);current_tuning_start(&s);
    current_tuning_step(&s,-10,-10);
    assert(s.saturated && hypotf(s.vd,s.vq)<=p.voltage_limit+1e-5f);
    p=settings();p.mode=TUNE_CURRENT_CHIRP;p.duration=0.1f;
    current_tuning_init(&s,&p);current_tuning_start(&s);
    for(unsigned n=0;n<2002;++n) {
        current_tuning_step(&s,0,0);
        assert(isfinite(s.vd) && isfinite(s.vq));
        assert(s.id_ref>=0 && s.id_ref<=p.amplitude);
    }
    assert(!s.active);
#ifdef REPO_TUNING_24V
    p=settings();p.mode=TUNE_CURRENT_CHIRP;p.duration=0.1f;
    p.f_start=p.f_end=2000.0f;p.voltage_limit=2.0f;
    assert(current_tuning_valid(&p));
    current_tuning_init(&s,&p);current_tuning_start(&s);
    for(unsigned n=0;n<2002u;++n) {
        current_tuning_step(&s,0,0);
        assert(isfinite(s.id_ref) && s.id_ref>=0 && s.id_ref<=0.5f);
        assert(isfinite(s.vd) && hypotf(s.vd,s.vq)<=2.00001f);
    }
    assert(!s.active);
#endif
#if defined(REPO_MOTION_24V)
    p=settings();p.mode=TUNE_D_RECIPROCAL;p.amplitude=6.0f;
    p.duration=6.0f;p.voltage_limit=4.0f;p.kp=TUNE_MOTION_KP;p.ki=TUNE_MOTION_KI;
    assert(current_tuning_valid(&p));
    p.axis=1;assert(!current_tuning_valid(&p));p.axis=0;
    p.duration=6.01f;assert(!current_tuning_valid(&p));p.duration=6.0f;
    p.amplitude=5;assert(!current_tuning_valid(&p)); // 5 A ceiling must be <=3.2 V
    p.voltage_limit=3.2f;assert(current_tuning_valid(&p));
    p.voltage_limit=3.21f;assert(!current_tuning_valid(&p));
    p.voltage_limit=3.2f;p.duration=0;assert(current_tuning_valid(&p));
    p.duration=21;assert(!current_tuning_valid(&p));
    p.duration=6;p.amplitude=5.01f;assert(!current_tuning_valid(&p));
    const float five_decay=expf(-0.6f*DT_CURRENT/0.0003f);
    for(unsigned amps=1;amps<=5;++amps) {
        p.amplitude=(float)amps;p.duration=6;p.voltage_limit=tune_motion_voltage_ceiling(p.amplitude);
        assert(current_tuning_valid(&p));
        p.voltage_limit+=0.01f;assert(!current_tuning_valid(&p));p.voltage_limit-=0.01f;
        p.duration=0;assert(current_tuning_valid(&p)==(amps==5u));p.duration=6;
        current_tuning_init(&s,&p);current_tuning_start(&s);
        float id=0;
        for(unsigned n=0;n<120001u;++n) {
            current_tuning_field_angle(&s);current_tuning_step(&s,id,0);
            id=id*five_decay+s.vd/0.6f*(1-five_decay);
            assert(hypotf(s.vd,s.vq)<=p.voltage_limit+0.00001f);
            if(n==18000u) assert(fabsf(id-p.amplitude)<0.02f && !s.saturated);
            if(n==70000u) assert(fabsf(s.mechanical_position-FOC_TWO_PI)<1e-4f);
        }
        assert(!s.active && s.completed && s.half_moves==2 && s.vd==0);
        assert(tune_motion_filtered_trip(p.amplitude)>p.amplitude);
        assert(tune_motion_raw_trip(p.amplitude)>tune_motion_filtered_trip(p.amplitude));
    }
    assert(tune_motion_filtered_trip(1)==1.5f && tune_motion_raw_trip(1)==2);
    assert(tune_motion_filtered_trip(5)==6.5f && tune_motion_raw_trip(5)==7);
    p.amplitude=0;assert(!current_tuning_valid(&p));
    p.amplitude=6;p.voltage_limit=4;
    p.voltage_limit=4.01f;assert(!current_tuning_valid(&p));p.voltage_limit=4;
    const float motion_decay=expf(-0.6f*DT_CURRENT/0.0003f);
    for(unsigned continuous=0;continuous<2;++continuous) {
        p.duration=continuous ? 0 : 6;
        current_tuning_init(&s,&p);current_tuning_start(&s);
        assert(s.samples==120000u && current_tuning_field_angle(&s)==0);
        float id=0;
        const unsigned count=continuous ? 220000u : 120001u;
        for(unsigned n=0;n<count;++n) {
            current_tuning_field_angle(&s);
            current_tuning_step(&s,id,0);
            id=id*motion_decay+s.vd/0.6f*(1-motion_decay);
            assert(hypotf(s.vd,s.vq)<=4.00001f);
            if(n==18000u) assert(fabsf(id-6.0f)<0.02f && !s.saturated);
            if(n==70000u || n==170000u)
                assert(fabsf(s.mechanical_position-FOC_TWO_PI)<1e-4f);
        }
        if(continuous) {
            assert(s.active && !s.completed && s.half_moves==4u);
            current_tuning_stop(&s);
        } else assert(!s.active && s.completed && s.half_moves==2u);
        assert(s.vd==0 && s.vq==0);
    }
    // Continuous 5A uses the same repeated trajectory and retains its 3.2V bound.
    p.amplitude=5;p.duration=0;p.voltage_limit=3.2f;
    assert(current_tuning_valid(&p));
    current_tuning_init(&s,&p);current_tuning_start(&s);
    float scope_id=0;
    for(unsigned n=0;n<220000u;++n) {
        current_tuning_step(&s,scope_id,0);
        scope_id=scope_id*motion_decay+s.vd/0.6f*(1-motion_decay);
        assert(hypotf(s.vd,s.vq)<=3.20001f);
        if(n==18000u) assert(fabsf(scope_id-5.0f)<0.02f);
    }
    assert(s.active && !s.completed && s.half_moves==4u);
    current_tuning_stop(&s);assert(!s.active && s.vd==0 && s.vq==0);
    p=settings();p.amplitude=0.51f;assert(!current_tuning_valid(&p));
    p=settings();p.voltage_limit=2.01f;assert(!current_tuning_valid(&p));
    p=settings();p.mode=TUNE_VOLTAGE_STEP;p.amplitude=0.101f;
    assert(!current_tuning_valid(&p));
#elif !defined(REPO_TUNING_24V)
    // Open-loop field command: one cosine-eased mechanical turn out and back,
    // with D-axis current control and a hard 41 s deadline.
    p=settings();p.mode=TUNE_D_RECIPROCAL;p.amplitude=3.0f;
    p.duration=41.0f;p.voltage_limit=2.5f;p.kp=0.8f;p.ki=2000.0f;
    assert(current_tuning_valid(&p));
    p.axis=1;assert(!current_tuning_valid(&p));p.axis=0;
    p.amplitude=3.01f;assert(!current_tuning_valid(&p));p.amplitude=3.0f;
    p.duration=40.0f;assert(!current_tuning_valid(&p));p.duration=41.0f;
    p.voltage_limit=2.51f;assert(!current_tuning_valid(&p));p.voltage_limit=2.5f;
    current_tuning_init(&s,&p);current_tuning_start(&s);
    assert(current_tuning_field_angle(&s)==0.0f);
    s.sample=10000u;current_tuning_step(&s,0,0);
    assert(s.id_ref==3.0f && s.iq_ref==0.0f &&
           hypotf(s.vd,s.vq)<=2.50001f);
    current_tuning_init(&s,&p);current_tuning_start(&s);
    float aligned_id=0.0f;
    const float aligned_decay=expf(-0.6f*DT_CURRENT/0.0003f);
    for(unsigned n=0;n<18000u;++n) {
        assert(current_tuning_field_angle(&s)==0.0f);
        current_tuning_step(&s,aligned_id,0.0f);
        aligned_id=aligned_id*aligned_decay+s.vd/0.6f*(1.0f-aligned_decay);
        assert(hypotf(s.vd,s.vq)<=2.50001f);
    }
    assert(fabsf(aligned_id-3.0f)<0.01f && !s.saturated);
    const uint32_t align=20000u,leg=(s.samples-align)/2u;
    s.sample=align+leg/2u;s.prepared_sample=UINT32_MAX;
    current_tuning_field_angle(&s);
    assert(fabsf(s.mechanical_position-M_PI)<1e-4f && s.half_moves==0u);
    s.sample=align+leg;s.prepared_sample=UINT32_MAX;
    current_tuning_field_angle(&s);
    assert(fabsf(s.mechanical_position-FOC_TWO_PI)<1e-4f && s.half_moves==1u);
    s.sample=s.samples;s.prepared_sample=UINT32_MAX;
    assert(current_tuning_field_angle(&s)==0.0f && s.half_moves==2u);
    current_tuning_step(&s,0,0);
    assert(s.completed && !s.active && s.vd==0 && s.vq==0);
    // Faster 5 A profile keeps the same trajectory shape and tighter bounds.
    p.amplitude=5.0f;p.duration=21.0f;p.voltage_limit=3.2f;
    assert(current_tuning_valid(&p));
    p.duration=41.0f;assert(!current_tuning_valid(&p));p.duration=21.0f;
    p.voltage_limit=3.21f;assert(!current_tuning_valid(&p));p.voltage_limit=3.2f;
    current_tuning_init(&s,&p);current_tuning_start(&s);
    assert(s.samples==420000u);
    aligned_id=0.0f;
    for(unsigned n=0;n<18000u;++n) {
        current_tuning_field_angle(&s);
        current_tuning_step(&s,aligned_id,0.0f);
        aligned_id=aligned_id*aligned_decay+s.vd/0.6f*(1.0f-aligned_decay);
        assert(hypotf(s.vd,s.vq)<=3.20001f);
    }
    assert(fabsf(aligned_id-5.0f)<0.02f && !s.saturated);
    const uint32_t fast_leg=(s.samples-align)/2u;
    s.sample=align+fast_leg;s.prepared_sample=UINT32_MAX;
    current_tuning_field_angle(&s);
    assert(fabsf(s.mechanical_position-FOC_TWO_PI)<1e-4f && s.half_moves==1u);
    s.sample=s.samples;s.prepared_sample=UINT32_MAX;
    current_tuning_field_angle(&s);current_tuning_step(&s,0,0);
    assert(s.completed && s.half_moves==2u && !s.active && s.vd==0);
    // A zero-duration 6 A request repeats 2.5-second legs until explicitly
    // stopped. The cycle counter must wrap without ending the experiment.
    p.amplitude=6.0f;p.duration=0.0f;p.voltage_limit=4.0f;
    assert(current_tuning_valid(&p));
    p.duration=11.0f;assert(!current_tuning_valid(&p));p.duration=0.0f;
    p.amplitude=6.01f;assert(!current_tuning_valid(&p));p.amplitude=6.0f;
    p.voltage_limit=4.01f;assert(!current_tuning_valid(&p));p.voltage_limit=4.0f;
    current_tuning_init(&s,&p);current_tuning_start(&s);
    assert(s.samples==120000u && s.active && !s.completed);
    aligned_id=0.0f;
    for(uint32_t n=0;n<220000u;++n) {
        current_tuning_field_angle(&s);
        current_tuning_step(&s,aligned_id,0.0f);
        aligned_id=aligned_id*aligned_decay+s.vd/0.6f*(1.0f-aligned_decay);
        assert(s.active && !s.completed && hypotf(s.vd,s.vq)<=4.00001f);
        if(n==70000u || n==170000u)
            assert(fabsf(s.mechanical_position-FOC_TWO_PI)<1e-4f);
        if(n==119999u || n==219999u)
            assert(s.sample==align && s.half_moves==(n==119999u ? 2u : 4u));
    }
    assert(fabsf(aligned_id-6.0f)<0.02f && s.id_ref==6.0f);
    current_tuning_field_angle(&s);
    assert(s.mechanical_position==0.0f && s.half_moves==4u);
    current_tuning_stop(&s);
    assert(!s.active && s.vd==0.0f && !s.completed);
#else
    p=settings();p.mode=TUNE_D_RECIPROCAL;p.amplitude=6.0f;
    p.duration=0.0f;p.voltage_limit=2.0f;
    assert(!current_tuning_valid(&p));
    p=settings();p.amplitude=0.51f;assert(!current_tuning_valid(&p));
    p=settings();p.voltage_limit=2.01f;assert(!current_tuning_valid(&p));
    p=settings();p.mode=TUNE_VOLTAGE_STEP;p.amplitude=0.101f;
    assert(!current_tuning_valid(&p));
    p.amplitude=0.1f;assert(current_tuning_valid(&p));
#endif
    puts("PASS: config bounds,actual repo PI againstRL plant,bothaxes,vector saturation,finite chirp,test completion");
}
