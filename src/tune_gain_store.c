#include "tune_gain_store.h"
#include <math.h>
#include <string.h>

enum { MAGIC = 0x47454d53u, VERSION = 1u, COMMIT = 0xa55a5aa5u,
       RECORDS = TUNE_GAIN_FLASH_WORDS / TUNE_GAIN_RECORD_WORDS };

static bool valid(float kp, float ki)
{ return isfinite(kp) && isfinite(ki) && kp >= 0 && kp <= 10 && ki >= 0 && ki <= 10000; }

static uint32_t bits(float value)
{ uint32_t word; memcpy(&word, &value, sizeof(word)); return word; }
static float number(uint32_t word)
{ float value; memcpy(&value, &word, sizeof(value)); return value; }

static uint32_t checksum(const uint32_t words[4])
{
    uint32_t crc = 0xffffffffu;
    for(unsigned i = 0; i < 4; ++i)
        for(unsigned shift = 0; shift < 32; shift += 8) {
            crc ^= (words[i] >> shift) & 0xffu;
            for(unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
        }
    return ~crc;
}

typedef struct { bool found, damaged; uint32_t free_record, kp_bits, ki_bits; } Scan;
static Scan scan(const TuneGainFlash *flash)
{
    Scan s = { .free_record = RECORDS };
    for(uint32_t rec = 0; rec < RECORDS; ++rec) {
        const uint32_t base = rec * TUNE_GAIN_RECORD_WORDS;
        uint32_t words[TUNE_GAIN_RECORD_WORDS];
        bool erased = true;
        for(unsigned j = 0; j < TUNE_GAIN_RECORD_WORDS; ++j) {
            words[j] = flash->read_word(base + j);
            erased &= words[j] == 0xffffffffu;
        }
        if(erased) { s.free_record = rec; break; }
        const float kp = number(words[2]), ki = number(words[3]);
        if(words[0] == MAGIC && words[1] == VERSION &&
           words[4] == checksum(words) && words[5] == COMMIT && valid(kp, ki)) {
            s.found = true; s.kp_bits = words[2]; s.ki_bits = words[3];
        } else s.damaged = true;
    }
    return s;
}

TuneGainStatus tune_gain_load(const TuneGainFlash *flash, float *kp, float *ki)
{
    const Scan s = scan(flash);
    if(s.found) { *kp = number(s.kp_bits); *ki = number(s.ki_bits); return TUNE_GAIN_LOADED; }
    return s.damaged ? TUNE_GAIN_CORRUPT : TUNE_GAIN_EMPTY;
}

TuneGainStatus tune_gain_save(const TuneGainFlash *flash, float kp, float ki)
{
    if(!valid(kp, ki)) return TUNE_GAIN_REJECTED;
    const Scan s = scan(flash);
    if(s.free_record == RECORDS) return TUNE_GAIN_FULL;
    const uint32_t base = s.free_record * TUNE_GAIN_RECORD_WORDS;
    const uint32_t words[TUNE_GAIN_RECORD_WORDS] = {
        MAGIC, VERSION, bits(kp), bits(ki), 0, COMMIT
    };
    for(unsigned j = 0; j < 4; ++j)
        if(!flash->write_word(base + j, words[j])) return TUNE_GAIN_WRITE_ERROR;
    if(!flash->write_word(base + 4, checksum(words)) ||
       !flash->write_word(base + 5, COMMIT)) return TUNE_GAIN_WRITE_ERROR;
    float actual_kp, actual_ki;
    if(tune_gain_load(flash, &actual_kp, &actual_ki) != TUNE_GAIN_LOADED ||
       bits(actual_kp) != words[2] || bits(actual_ki) != words[3])
        return TUNE_GAIN_WRITE_ERROR;
    return TUNE_GAIN_SAVED;
}
