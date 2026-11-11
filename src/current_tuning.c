#include "current_tuning.h"
#include "config.h"
#include <math.h>

bool current_tuning_valid(const CurrentTuneConfig *p)
{
    const float values[]={p->mode,p->axis,p->amplitude,p->kp,p->ki,p->f_start,
                          p->f_end,p->duration,p->voltage_limit};
    for (unsigned i=0;i<9;++i) if (!isfinite(values[i])) return false;
    const bool voltage=p->mode==TUNE_VOLTAGE_STEP || p->mode==TUNE_VOLTAGE_CHIRP;
    const bool motion=p->mode==TUNE_D_RECIPROCAL;
    const bool motion_3a=p->amplitude==3.0f && p->duration==41.0f &&
        p->voltage_limit<=2.5f;
    const bool motion_5a=p->amplitude==5.0f && p->duration==21.0f &&
        p->voltage_limit<=3.2f;
    const bool motion_6a=p->amplitude==6.0f && (p->duration==0.0f
#ifdef REPO_MOTION_24V
        || p->duration==6.0f // one bounded cycle before continuous bench use
#endif
        ) &&
        p->voltage_limit<=4.0f;
#ifdef REPO_TUNING_24V
    if(motion) {
#ifdef REPO_MOTION_24V
        if(!motion_6a) return false;
#else
        return false; // stationary identification image stays isolated
#endif
    } else if(p->amplitude>(voltage ? 0.1f : 0.5f) ||
              p->voltage_limit>2.0f) return false;
#endif
    return p->mode>=0 && p->mode<=TUNE_D_RECIPROCAL && p->mode==(int)p->mode &&
        (p->axis==0 || p->axis==1) && p->amplitude>=0 &&
        p->amplitude<=(voltage ? p->voltage_limit : motion ? 6.0f : 6.25f) &&
        p->kp>=0 && p->kp<=10 && p->ki>=0 && p->ki<=10000 &&
        p->f_start>=1 && p->f_end>=p->f_start &&
        p->f_end<=TUNE_MAX_FREQUENCY_HZ &&
        (motion ? (p->axis==0 && (motion_3a || motion_5a || motion_6a)) :
                  (p->duration>=0.1f && p->duration<=10)) &&
        p->voltage_limit>0 && p->voltage_limit<=4;
}

void current_tuning_init(CurrentTuning *s,const CurrentTuneConfig *p)
{
    *s=(CurrentTuning){0}; s->config=*p; s->prepared_sample=UINT32_MAX;
    pi_init(&s->d,p->kp,p->ki,-p->voltage_limit,p->voltage_limit);
    pi_init(&s->q,p->kp,p->ki,-p->voltage_limit,p->voltage_limit);
}
void current_tuning_start(CurrentTuning *s)
{
    const CurrentTuneConfig p=s->config; current_tuning_init(s,&p);
    // Duration zero is reserved for the 6 A continuous reciprocal profile.
    s->samples=p.mode==TUNE_D_RECIPROCAL && p.duration==0.0f ?
        (uint32_t)(6.0f/DT_CURRENT) : (uint32_t)(p.duration/DT_CURRENT);
    s->active=true;
}
void current_tuning_stop(CurrentTuning *s)
{ s->active=false; s->id_ref=s->iq_ref=s->vd=s->vq=0; s->saturated=false; }

float current_tuning_field_angle(CurrentTuning *s)
{
    if(!s->active || s->config.mode!=TUNE_D_RECIPROCAL) return 0.0f;
    if(s->prepared_sample==s->sample) return s->field_theta;
    // At the 2.5 s leg speed the field moves under 16 electrical rad/s.
    // Updating every 0.25 ms bounds its step below 0.004 electrical rad.
    const uint32_t stride=s->config.amplitude==6.0f ? 5u : 10u;
    if(s->prepared_sample!=UINT32_MAX && s->sample<s->samples &&
       s->sample%stride!=0u) return s->field_theta;
    const uint32_t align=(uint32_t)(1.0f/DT_CURRENT);
    const uint32_t leg=(s->samples-align)/2u;
    float position=0.0f;
    if(s->sample>=align && s->sample<align+leg) {
        const float u=(float)(s->sample-align)/(float)leg;
        position=M_PI*(1.0f-cosf(M_PI*u));
    } else if(s->sample>=align+leg && s->sample<s->samples) {
        const float u=(float)(s->sample-align-leg)/(float)(s->samples-align-leg);
        position=M_PI*(1.0f+cosf(M_PI*u));
    }
    if(s->config.duration==0.0f) {
        if(s->sample==align+leg && s->half_moves!=UINT32_MAX) ++s->half_moves;
    } else s->half_moves=s->sample>=s->samples ? 2u : s->sample>=align+leg ? 1u : 0u;
    s->mechanical_position=position;
    s->field_theta=fmodf(4.0f*position,FOC_TWO_PI);
    s->prepared_sample=s->sample;
    return s->field_theta;
}

void current_tuning_step(CurrentTuning *s,float id,float iq)
{
    if (!s->active) return;
    const bool continuous=s->config.mode==TUNE_D_RECIPROCAL &&
        s->config.duration==0.0f;
    if (!continuous && s->sample>=s->samples) {
        if(s->config.mode==TUNE_D_RECIPROCAL) s->completed=true;
        current_tuning_stop(s); return;
    }
    const CurrentTuneConfig *p=&s->config;
    const float t=s->sample*DT_CURRENT;
    const bool chirp=p->mode==TUNE_CURRENT_CHIRP || p->mode==TUNE_VOLTAGE_CHIRP;
    float reference;
    if(p->mode==TUNE_D_RECIPROCAL) {
        current_tuning_field_angle(s);
        reference=p->amplitude*fminf(t/0.5f,1.0f);
    } else if (chirp) {
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
    if(continuous && s->sample>=s->samples) {
        s->sample=(uint32_t)(1.0f/DT_CURRENT);
        s->prepared_sample=UINT32_MAX;
        if(s->half_moves!=UINT32_MAX) ++s->half_moves;
    }
}
