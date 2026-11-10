# First slow motor-rotation attempt

This separate image follows the passing V5 stationary-current test and Scott's
instruction to proceed toward a slow spin. It combines reverse-pair validation
with a short stepped rotating-field attempt. It is built/host-tested, flashed
and readback verified. Startup software-triggered shutdown-self-test PASS was
read at SWD0x20002EB8=00000001. Evidence is in
`bench/2026-11-09-motor-spin-v1-flash.txt` and
`bench/2026-11-09-motor-spin-v1-self-test-swd.txt`. Its first run completed all
electrical steps, but Scott reported no shaft rotation. See
`bench/2026-11-09-motor-spin-v1-first-run-analysis.md`. A separate
[250mA trial](MOTOR_STEPPED_CRAWL_250MA.md) is now flashed; the100mA images
remain available unchanged.

Image: `build-bringup/drv8353-motor-spin.hex`

```text
Diagnostic: DRV8353RS reverse-and-stepped-crawl v1
```

## What one USER press does

1. Runs the same zero-drive ADC/bus preflight and fresh gain40 offsets as V5.
2. Tests B->A, C->B, A->C at nominal100mA,250ms each,100ms COAST gaps. All three
   must recognize the target and pass protection/register checks before crawling.
3. Holds A->B for250ms as initial alignment, then advances the field in60degree
   electrical steps: A->C, B->C, B->A, C->A, C->B, A->B, repeated for24 advances.
   It finishes with the original A->B field orientation after four electrical
   turns. Each hold is250ms with brief5ms COAST gaps plus bus/register overhead.
4. Drops ENABLE/INLs/PWM and reports all results through `FEEDBACK END`.

The [GM42BLF60-240 specification](https://gemsmotor.com/nema-17-square-brushless-dc-motor)
lists8poles, hence4polepairs. Four electrical field turns correspond to one
mechanical revolution **only if the rotor follows**. The24 advances take about
6..7seconds including gaps, roughly9..10rpm commanded field speed. Expect
stepped motion, not smooth speed regulation. Reverse tests/initial alignment
can move the rotor before that turn. No Hall/encoder input measures movement,
speed, direction or loss of synchronism. A completed field sequence is not
proof the shaft turned; Scott must report the observed movement.

The100mA target may be too weak to overcome friction. Do not restrain or
hand-start the shaft, or automatically increase current if it only twitches.

## Limits retained from the passing image

- Same current conversion,20ms ramp,500mA filtered and1A sampled raw trips.
- Same centered PWM, differential<=24/1250=1.92%, leg49.04..50.96%.
- Same150/300mA gates,100ns deadtime, latched0.2V VDS/2us deglitch,40V/V CSA.
- Every pulse has the existing independent SysTick250ms cutoff. The whole
  sequence has a12second armed maximum; deadline does not extend a pulse.
- nFAULT EXTI, USER20ms stop, fresh-sample/timing, GPIO/compare checks remain.
- No SPI within a pulse; verify all registers before and after every pulse.
  ADC bus checks remain during the pulse. No automatic retry.
- Same12V/0.25A supply setting. Target/trips remain nominal sampled current,
  not independently calibrated amperes or continuous peak-current guarantees.

V5's `drv8353-motor-probe.hex` remains a separate unchanged passing baseline.
The unmodified normal AKM firmware is not used: its encoder/pole count/gains and
3V alignment command do not describe this bounded GEMS/no-encoder test.

## Operator sequence after flash verification

1. Keep all wiring, motor phases and Hall disconnections as in V5. Secure the
   motor with an unloaded, freely turning shaft and keep hands clear.
2. Confirm the reverse-and-stepped-crawl banner and startup shutdown-self-test
   PASS on USB. Apply EVM12V/0.25A, then press/release USER once.
3. It prints three REVERSE DONE lines, then REVERSE PASS / CRAWL START. Observe
   shaft motion; USER remains available to stop. No scope is required for this run.
4. Wait for `FEEDBACK END`, switch EVM OFF and send the whole log plus whether
   the shaft crawled around, only twitched, or stayed still. Do not restart
   repeatedly to force movement if a fault stops the run.

Expected electrical completion: MOTOR/SYNC/BUS failure0, STOP11,
`SPIN_RESULT reverse_passed=3 field_holds_completed=25 field_advances=24`, and
`shaft_tracking=UNMEASURED`. `field_elapsed_ms` runs from the end of initial
alignment to the end of the final hold. It includes gaps/housekeeping. A guard
stop is a failed sequence even if earlier STEP DONE summaries exist.

Detailed STEP DONE summaries print only after shutdown. Reverse feedback tails
are retained in full; nine frames from the final hold of each crawl orientation
are printed to keep the log bounded. During crawling, the inactive synchronous
stats bank is cleared/swapped between holds without UART, retaining freshness
and per-sample guards while preventing the60000-unreported-sample guard filling.

## Software evidence

Five GCC ASan/UBSan suites pass: conversion/controller, history, regular ADC queue,
centered PWM, and the reverse/crawl plan. Tests verify all bounds/trace indices,
ordered60degree Clarke vectors including wrap, the final return to the initial
vector and24 advances/four electrical turns. Synthetic current plants include
all ordered pairs; none is a measured model or stall detector for this motor.
Evidence: `bench/2026-11-09-motor-spin-v1-host-tests.txt`.

All six variants build without warnings. Four older diagnostic HEX files and
the passing V5 HEX retain their hashes. New spin image text16895/data4/bss11984.
HEX SHA256: `76E07576C71998460068D9711A268D165D4B52699CF3FCD1959AEBFEF2888105`.
ELF shutdown-self-test address resolves0x20002EB8; resolve again before SWD.
