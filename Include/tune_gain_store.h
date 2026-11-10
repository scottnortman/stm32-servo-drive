#pragma once
#include <stdbool.h>
#include <stdint.h>

// STM32F411xE sector 7 is reserved for this target: 0x08060000..0x0807ffff.
// The module only appends committed, checked records; it never erases a sector.
enum { TUNE_GAIN_FLASH_WORDS = 128u * 1024u / 4u,
       TUNE_GAIN_RECORD_WORDS = 6u };
typedef struct {
    uint32_t (*read_word)(uint32_t index);
    bool (*write_word)(uint32_t index, uint32_t value);
} TuneGainFlash;
typedef enum {
    TUNE_GAIN_EMPTY = 0,
    TUNE_GAIN_LOADED = 1,
    TUNE_GAIN_SAVED = 2,
    TUNE_GAIN_REJECTED = 3,
    TUNE_GAIN_FULL = 4,
    TUNE_GAIN_WRITE_ERROR = 5,
    TUNE_GAIN_CORRUPT = 6
} TuneGainStatus;

TuneGainStatus tune_gain_load(const TuneGainFlash *flash, float *kp, float *ki);
TuneGainStatus tune_gain_save(const TuneGainFlash *flash, float kp, float ki);
const TuneGainFlash *tune_gain_stm32_flash(void);
