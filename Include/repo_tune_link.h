#pragma once
#include <stdbool.h>
#include <stdint.h>
void repo_tune_init(void);
// Foreground services only. Never blocks on a UART transfer.
void repo_tune_poll(uint32_t now,bool armed,uint32_t reason,uint32_t irq_cycles);
bool repo_tune_take_start(void);
bool repo_tune_take_stop(void);
bool repo_tune_link_alive(uint32_t now);
void repo_tune_disarm(void);
bool repo_tune_d_reciprocal_selected(void);
float repo_tune_motion_amplitude(void);
float repo_tune_field_angle(void);
// Called by the existing20kHz board IRQ, after the usual current guards.
bool repo_tune_control_tick(uint32_t tick,float id,float iq,const float logical[3],
                            float *vd,float *vq);
