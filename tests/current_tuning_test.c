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
    p.f_end=801;assert(!current_tuning_valid(&p));p=settings();
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
    puts("PASS: config bounds,actual repo PI againstRL plant,bothaxes,vector saturation,finite chirp,test completion");
}
