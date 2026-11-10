ENTRY(Reset_Handler)

/* STM32F411RE: reserve sector 7 (0x08060000..0x0807ffff) for the
 * append-only current-gain journal. Only repo-current-tuning uses this file. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 384K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text :
  {
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
  } > FLASH
  _sidata = LOADADDR(.data);
  .data :
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM AT>FLASH
  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM
  _end = .;
}
