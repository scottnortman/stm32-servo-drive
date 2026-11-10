#include "tune_gain_store.h"
#include "stm32f4xx.h"

enum { TUNE_GAIN_BASE = 0x08060000u, FLASH_WAIT_LIMIT = 1000000u };
static const uint32_t FLASH_ERRORS = FLASH_SR_OPERR | FLASH_SR_WRPERR |
    FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR | FLASH_SR_RDERR;

static uint32_t read_word(uint32_t index)
{ return *(volatile const uint32_t *)(TUNE_GAIN_BASE + index * 4u); }

static bool wait_ready(void)
{
    for(uint32_t i = 0; i < FLASH_WAIT_LIMIT; ++i)
        if((FLASH->SR & FLASH_SR_BSY) == 0u) return true;
    return false;
}

static bool write_word(uint32_t index, uint32_t value)
{
    if(index >= TUNE_GAIN_FLASH_WORDS || !wait_ready()) return false;
    if(FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123u;
        FLASH->KEYR = 0xcdef89abu;
    }
    if(FLASH->CR & FLASH_CR_LOCK) return false;
    FLASH->SR = FLASH_ERRORS | FLASH_SR_EOP;
    FLASH->CR = (FLASH->CR & ~FLASH_CR_PSIZE) | FLASH_CR_PSIZE_1 | FLASH_CR_PG;
    *(volatile uint32_t *)(TUNE_GAIN_BASE + index * 4u) = value;
    const bool ready = wait_ready();
    FLASH->CR &= ~FLASH_CR_PG;
    const bool ok = ready && (FLASH->SR & FLASH_ERRORS) == 0u && read_word(index) == value;
    FLASH->CR |= FLASH_CR_LOCK;
    return ok;
}

const TuneGainFlash *tune_gain_stm32_flash(void)
{
    static const TuneGainFlash flash = { read_word, write_word };
    return &flash;
}
