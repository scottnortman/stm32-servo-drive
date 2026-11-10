#include "current_tuning.h"
#include "config.h"
#include <math.h>

bool current_tuning_valid(const CurrentTuneConfig *p)
{
    const float values[]={p->mode,p->axis,p->amplitude,p->kp,p->ki,p->f_start,
                          p->f_end,p->duration,p->voltage_limit};
    for (unsigned i=0;i<9;++i) if (!isfinite(values[i])) return false;
    const bool voltage=p->mode==TUNE_VOLTAGE_STEP || p->mode==TUNE_VOLTAGE_CHIRP;
    return p->mode>=0 && p->mode<=3 && p->mode==(int)p->mode &&
        (p->axis==0 || p->axis==1) && p->amplitude>=0 &&
        p->amplitude<=(voltage ? p->voltage_limit : 6.25f) &&
        p->kp>=0 && p->kp<=10 && p->ki>=0 && p->ki<=10000 &&
        p->f_start>=1 && p->f_end>=p->f_start && p->f_end<=800 &&
        p->duration>=0.1f && p->duration<=10 &&
        p->voltage_limit>0 && p->voltage_limit<=4;
}

void current_tuning_init(CurrentTuning *s,const CurrentTuneConfig *p)
{
    *s=(CurrentTuning){0}; s->config=*p;
    pi_init(&s->d,p->kp,p->ki,-p->voltage_limit,p->voltage_limit);
    pi_init(&s->q,p->kp,p->ki,-p->voltage_limit,p->voltage_limit);
}
void current_tuning_start(CurrentTuning *s)
{
    const CurrentTuneConfig p=s->config; current_tuning_init(s,&p);
    s->samples=(uint32_t)(p.duration/DT_CURRENT); s->active=true;
}
void current_tuning_stop(CurrentTuning *s)
{ s->active=false; s->id_ref=s->iq_ref=s->vd=s->vq=0; s->saturated=false; }

void current_tuning_step(CurrentTuning *s,float id,float iq)
{
    if (!s->active) return;
    if (s->sample>=s->samples) { current_tuning_stop(s); return; }
    const CurrentTuneConfig *p=&s->config;
    const float t=s->sample*DT_CURRENT;
    const bool chirp=p->mode==TUNE_CURRENT_CHIRP || p->mode==TUNE_VOLTAGE_CHIRP;
    float reference;
    if (chirp) {
        // The repo sysid chirp frequency law, biased positive so this initial
        // fixed-frame experiment does not reverse the holding field.
        const float frequency=p->f_start*powf(p->f_end/p->f_start,t/p->duration);
        s->phase+=FOC_TWO_PI*frequency*DT_CURRENT;
        if (s->phase>=FOC_TWO_PI) s->phase-=FOC_TWO_PI;
        reference=p->amplitude*0.5f*(1+sinf(s->phase));
    } else {
        const float fraction=(float)s->sample/(float)s->samples;
        reference=fraction>=0.2f && fraction<0.7f ? p->amplitude : 0;
    }
    s->id_ref=s->iq_ref=0;
    if (p->mode==TUNE_VOLTAGE_STEP || p->mode==TUNE_VOLTAGE_CHIRP) {
        s->vd=p->axis==0 ? reference : 0;
        s->vq=p->axis==1 ? reference : 0;
    } else {
        s->id_ref=p->axis==0 ? reference : 0;
        s->iq_ref=p->axis==1 ? reference : 0;
        // Same signed d/q PI calls as foc_sysid.c; no replacement controller.
        s->vd=pi_step(&s->d,s->id_ref-id,DT_CURRENT);
        s->vq=pi_step(&s->q,s->iq_ref-iq,DT_CURRENT);
    }
    const float norm=sqrtf(s->vd*s->vd+s->vq*s->vq);
    s->saturated=norm>=p->voltage_limit*0.999f;
    if (norm>p->voltage_limit) {
        s->vd*=p->voltage_limit/norm; s->vq*=p->voltage_limit/norm;
    }
    ++s->sample;
}
