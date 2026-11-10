#include "tune_gain_store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t memory[TUNE_GAIN_FLASH_WORDS];
static int writes_remaining = -1;
static uint32_t read_word(uint32_t index)
{ assert(index < TUNE_GAIN_FLASH_WORDS); return memory[index]; }
static bool write_word(uint32_t index, uint32_t value)
{
    assert(index < TUNE_GAIN_FLASH_WORDS);
    if(writes_remaining == 0) return false;
    if(writes_remaining > 0) --writes_remaining;
    if((memory[index] & value) != value) return false;
    memory[index] &= value;
    return true;
}
static const TuneGainFlash flash = { read_word, write_word };

int main(void)
{
    memset(memory, 0xff, sizeof(memory));
    float kp = -1, ki = -1;
    assert(tune_gain_load(&flash,&kp,&ki)==TUNE_GAIN_EMPTY);
    assert(kp==-1 && ki==-1);
    assert(tune_gain_save(&flash,NAN,2000)==TUNE_GAIN_REJECTED);
    assert(tune_gain_save(&flash,0.8f,2000)==TUNE_GAIN_SAVED);
    assert(tune_gain_load(&flash,&kp,&ki)==TUNE_GAIN_LOADED);
    assert(kp==0.8f && ki==2000);

    // A reset at any word of the next append retains the preceding record.
    uint32_t baseline[TUNE_GAIN_RECORD_WORDS];
    memcpy(baseline,memory,sizeof(baseline));
    for(int committed_words=0; committed_words<6; ++committed_words) {
        memset(memory+TUNE_GAIN_RECORD_WORDS,0xff,
               sizeof(memory)-sizeof(baseline));
        writes_remaining=committed_words;
        assert(tune_gain_save(&flash,1.1f,3000)==TUNE_GAIN_WRITE_ERROR);
        assert(tune_gain_load(&flash,&kp,&ki)==TUNE_GAIN_LOADED);
        assert(kp==0.8f && ki==2000);
    }
    writes_remaining=-1;
    assert(tune_gain_save(&flash,1.1f,3000)==TUNE_GAIN_SAVED);
    assert(tune_gain_load(&flash,&kp,&ki)==TUNE_GAIN_LOADED);
    assert(kp==1.1f && ki==3000);

    // Damage to the newest committed record falls back to the older one.
    memory[2*TUNE_GAIN_RECORD_WORDS+2] ^= 1u;
    assert(tune_gain_load(&flash,&kp,&ki)==TUNE_GAIN_LOADED);
    assert(kp==0.8f && ki==2000);
    memset(memory,0xff,sizeof(memory));
    memory[0]=0x47454d53u; // Torn first record, no valid predecessor.
    assert(tune_gain_load(&flash,&kp,&ki)==TUNE_GAIN_CORRUPT);
    memset(memory,0,sizeof(memory));
    assert(tune_gain_save(&flash,0.8f,2000)==TUNE_GAIN_FULL);
    puts("PASS: empty, save/load, invalid gains, interrupted writes, retry, corruption and full journal");
}
