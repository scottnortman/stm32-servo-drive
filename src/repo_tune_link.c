#include "repo_tune_link.h"
#include "current_tuning.h"
#include "protocol.h"
#include "stm32f4xx.h"
#include <stddef.h>
#include <string.h>
#include <math.h>

enum { BURST=2048, RX_SIZE=512, SAMPLE_KIND=16, BURST_KIND=17, STATUS_KIND=18 };
static CurrentTuning tune;
static SysIdSample latest,burst[BURST];
static volatile uint32_t latest_version;
static volatile uint16_t rx_head,rx_tail;
static uint8_t rx[RX_SIZE],tx[70],command[46];
static uint32_t heartbeat,last_live,last_status,ack_seq,bad_rx,tx_drops;
static uint16_t command_size,result;
static bool start_requested,stop_requested;
static uint32_t head,recorded,remaining,dump_index,dump_count;
static bool capture_enabled,triggered,frozen;
static float prior_reference;

_Static_assert(sizeof(SysIdSample)==32,"Reuse the32byte repo sample layout");
typedef struct {
    uint32_t tick,ack_seq,bad_rx,tx_drops,irq_cycles;
    uint16_t reason,state,result,capabilities;
    CurrentTuneConfig config;
} TuneStatus;
_Static_assert(sizeof(TuneStatus)==64,"Version1 status size");

static uint16_t crc16(const uint8_t *p,unsigned n)
{
    uint16_t crc=0xffff;
    while(n--) { crc^=(uint16_t)*p++<<8;
        for(unsigned b=0;b<8;++b) crc=(crc&0x8000) ? (uint16_t)((crc<<1)^0x1021) : (uint16_t)(crc<<1);
    } return crc;
}
static bool send_frame(uint8_t kind,const void *payload,unsigned length)
{
    if (DMA1_Stream6->CR & DMA_SxCR_EN) return false;
    DMA1->HIFCR=DMA_HIFCR_CTCIF6|DMA_HIFCR_CHTIF6|DMA_HIFCR_CTEIF6|
                  DMA_HIFCR_CDMEIF6|DMA_HIFCR_CFEIF6;
    tx[0]=0xa5;tx[1]=0x5a;tx[2]=kind;tx[3]=(uint8_t)length;
    memcpy(tx+4,payload,length);
    const uint16_t crc=crc16(tx+2,length+2);
    tx[length+4]=(uint8_t)crc;tx[length+5]=(uint8_t)(crc>>8);
    DMA1_Stream6->M0AR=(uint32_t)tx;DMA1_Stream6->NDTR=length+6;
    __DMB();DMA1_Stream6->CR|=DMA_SxCR_EN;
    return true;
}
void USART2_IRQHandler(void)
{
    const uint32_t sr=USART2->SR;
    if (sr & (USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE)) {
        const uint8_t b=(uint8_t)USART2->DR;
        if (sr & (USART_SR_ORE|USART_SR_FE|USART_SR_NE)) { ++bad_rx;return; }
        const uint16_t next=(rx_head+1u)&(RX_SIZE-1u);
        if(next==rx_tail) ++bad_rx;else {rx[rx_head]=b;rx_head=next;}
    }
}
void repo_tune_init(void)
{
    const CurrentTuneConfig p={TUNE_CURRENT_STEP,0,0.5f,0.2f,50,5,200,1,0.75f};
    current_tuning_init(&tune,&p);
    RCC->AHB1ENR|=RCC_AHB1ENR_DMA1EN;
    DMA1_Stream6->CR=0;while(DMA1_Stream6->CR & DMA_SxCR_EN){}
    DMA1_Stream6->PAR=(uint32_t)&USART2->DR;
    DMA1_Stream6->CR=(4u<<DMA_SxCR_CHSEL_Pos)|DMA_SxCR_DIR_0|DMA_SxCR_MINC;
    USART2->CR1=0;USART2->BRR=54u; //50MHz/54=925926baud,0.47% from921600.
    USART2->CR3=USART_CR3_DMAT;
    USART2->CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE|USART_CR1_RXNEIE;
    NVIC_SetPriority(USART2_IRQn,6);NVIC_EnableIRQ(USART2_IRQn);
}
static bool same_experiment(const CurrentTuneConfig *a,const CurrentTuneConfig *b)
{
    return a->mode==b->mode && a->axis==b->axis && a->amplitude==b->amplitude &&
        a->f_start==b->f_start && a->f_end==b->f_end &&
        a->duration==b->duration && a->voltage_limit==b->voltage_limit;
}
static void accept_command(uint32_t now,bool armed)
{
    uint32_t seq;memcpy(&seq,command+4,4);
    CurrentTuneConfig p;memcpy(&p,command+8,sizeof(p));
    const unsigned op=command[2];
    if(op==1) {heartbeat=now;return;}
    // Configuration/start/capture commands are ordered and never replayed.
    if((int32_t)(seq-ack_seq)<=0) return;
    ack_seq=seq;result=0;
    if(op==2) {
        if(!current_tuning_valid(&p)) {result=1;return;}
        if(tune.active && !same_experiment(&p,&tune.config)) {result=2;return;}
        const uint32_t mask=__get_PRIMASK();__disable_irq();
        if(tune.active) {
            tune.config=p;tune.d.kp=tune.q.kp=p.kp;tune.d.ki=tune.q.ki=p.ki;
        } else current_tuning_init(&tune,&p);
        __set_PRIMASK(mask);
    } else if(op==3) {
        if(!armed || !repo_tune_link_alive(now)) {result=3;return;}
        if(tune.active || dump_count || frozen) {result=2;return;}
        start_requested=true;
    } else if(op==4) stop_requested=true;
    else {result=4;}
}
void repo_tune_poll(uint32_t now,bool armed,uint32_t reason,uint32_t irq_cycles)
{
    while(rx_tail!=rx_head) {
        const uint8_t b=rx[rx_tail];rx_tail=(rx_tail+1u)&(RX_SIZE-1u);
        if(command_size==0 && b!=0x54) continue;
        if(command_size==1 && b!=0x43) {command_size=b==0x54 ? 1 : 0;continue;}
        command[command_size++]=b;
        if(command_size==4 && command[3]!=40) {command_size=0;++bad_rx;}
        if(command_size==sizeof(command)) {
            const uint16_t expected=command[44]|((uint16_t)command[45]<<8);
            if(crc16(command+2,42)==expected) accept_command(now,armed);else ++bad_rx;
            command_size=0;
        }
    }
    if(!armed) current_tuning_stop(&tune);
    if(!tune.active && frozen && !dump_count && dump_index==0) dump_count=recorded;
    if((uint32_t)(now-last_status)>=50u) {
        const TuneStatus status={latest.t,ack_seq,bad_rx,tx_drops,irq_cycles,
            (uint16_t)reason,(uint16_t)((armed?1:0)|(tune.active?2:0)|(dump_count?4:0)),
            result,15,tune.config};
        if(send_frame(STATUS_KIND,&status,sizeof(status))) last_status=now;
        return;
    }
    if(dump_count) {
        const uint32_t start=(head+BURST-recorded)%BURST;
        if(send_frame(BURST_KIND,&burst[(start+dump_index)%BURST],32)) {
            ++dump_index;--dump_count;if(!dump_count) frozen=false;
        } return;
    }
    // DMA is foreground-only. An occupied UART drops a display sample, never
    // delays the20kHz control loop. Version retry protects coherent snapshots.
    if(latest_version!=last_live && latest.t%10u==0) {
        SysIdSample s;uint32_t v;
        do {v=latest_version;__DMB();s=latest;__DMB();} while(v!=latest_version || (v&1u));
        if(!send_frame(SAMPLE_KIND,&s,32)) ++tx_drops;
        last_live=v;
    }
}
bool repo_tune_take_start(void)
{
    if(!start_requested) return false;
    start_requested=false;
    const uint32_t mask=__get_PRIMASK();__disable_irq();
    current_tuning_start(&tune);head=recorded=remaining=dump_index=dump_count=0;
    capture_enabled=true;triggered=frozen=false;prior_reference=0;
    __set_PRIMASK(mask);return true;
}
bool repo_tune_take_stop(void) {bool v=stop_requested;stop_requested=false;return v;}
bool repo_tune_link_alive(uint32_t now) {return heartbeat!=0 && (uint32_t)(now-heartbeat)<500u;}
void repo_tune_disarm(void)
{
    current_tuning_stop(&tune);start_requested=false;
    if(capture_enabled && recorded) {frozen=true;capture_enabled=false;}
}
bool repo_tune_control_tick(uint32_t tick,float id,float iq,const float amps[3],float *vd,float *vq)
{
    current_tuning_step(&tune,id,iq);*vd=tune.vd;*vq=tune.vq;
    const bool voltage=tune.config.mode==TUNE_VOLTAGE_STEP || tune.config.mode==TUNE_VOLTAGE_CHIRP;
    const float reference=voltage ? fabsf(tune.vd)+fabsf(tune.vq) : tune.id_ref+tune.iq_ref;
    SysIdSample s={tick,(int16_t)(amps[0]*1000),(int16_t)(amps[2]*1000),
        (int16_t)(amps[1]*1000),(int16_t)(id*1000),(int16_t)(iq*1000),
        (int16_t)(*vd*1000),(int16_t)(*vq*1000),0,
        (uint16_t)(int16_t)(tune.id_ref*1000),(uint16_t)(int16_t)(tune.iq_ref*1000),0,
        (uint16_t)((tune.active?1:2)|(tune.saturated?256:0)),0,
        (uint16_t)(100+(unsigned)tune.config.mode)};
    ++latest_version;__DMB();latest=s;__DMB();++latest_version;
    if(capture_enabled && tune.active) {
        if(!triggered && reference>0.00001f && prior_reference<=0.00001f) {
            triggered=true;remaining=BURST-512u;
        }
        burst[head]=s;head=(head+1u)%BURST;if(recorded<BURST)++recorded;
        if(triggered && remaining && --remaining==0) {frozen=true;capture_enabled=false;}
    }
    prior_reference=reference;
    if(!tune.active && capture_enabled) {frozen=recorded!=0;capture_enabled=false;}
    return tune.active;
}
