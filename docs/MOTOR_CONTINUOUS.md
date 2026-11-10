# Continuous slow motor run

User requested continuous rotation on November 10, 2026 after the complete
250 mA electrical sequence only twitched or moved the shaft partway.
The separate `drv8353-motor-continuous.elf` target replaces the finite crawl
with an initial 0.5 second alignment and repeating six-sector commutation.
There is no scheduled motion stop, coast gap, or current ramp reset between sectors.
The existing stationary and finite diagnostic images remain available unchanged.

## Operation

1. Program with EVM power OFF and Nucleo USB connected. Motor may remain connected
   and unloaded. Hall leads remain disconnected; retain the verified harness.
2. Check the `CONTINUOUS slow rotation v1 (350mA, Hi-Z SPI)` startup banner and
   shutdown self-test PASS. This self-test does not prove the physical fault wire.
3. Secure the motor, keep clear of the shaft, apply 12 V with the existing 0.25 A
   supply current limit, then press and release USER.
4. Motion commands continue until USER is held for at least 20 ms or a protection
   trips. There is no timed run limit. Serial output is quiet during operation;
   feedback prints after stopping. Report actual shaft motion and the stop log.
5. Turn EVM power off before changing wiring or programming another image.

The current reference is nominal 350 mA, with 500 mA filtered and 1 A sampled
raw trips unchanged. Maximum differential PWM is 56/1250 (4.48%); individual
leg duties remain 47.76..52.24%. ADC current scaling is not independently calibrated.
The bench supply input-current limit is distinct from phase-current limits.

The field advances 60 electrical degrees every 250 ms. With the motor's four
pole pairs this commands approximately 10 mechanical rpm if the rotor follows.
Actual shaft speed and following are unmeasured without position feedback.
This is a current-regulated stepped rotating field, not closed-loop speed control.

## Implementation and validation

`motor_continuous.c` reuses the tested physical phase sequence and centered PWM.
It retains the integral voltage demand and all three current filters across
commutation. Integral accumulation pauses for 4 ms during each current transition;
phase-pair consistency checks allow 30 ms for redistribution. Raw and filtered
current trips remain active on every driven sample, including those intervals.
Each completed sector must have reached the current target band.

Hi-Z SPI preflight and quiet startup arming are retained. No SPI or UART runs
while the bridge is energized. Regular bus/VREF reads are queued by the injected
ADC IRQ; statistics banks are recycled once a second. USER, nFAULT, bus, current,
ADC timing/freshness and configuration guards remain. A 750 ms field-progress
watchdog detects failed commutation; it is not a normal run-duration limit.

Host ASan/UBSan tests cover 20 seconds of continuous operation with multiple
synthetic current-response delays and polarities, retained duty/filter state,
PWM bounds, and protection failures. These tests do not establish rotor tracking
or reproduce the motor's physical R/L and switching transients.

Build SHA256: `110F55835E1D07C54CCCF3ACB3308B2E272CC8D417BB5FB4F90A97FE330F31F4`.
ELF shutdown self-test symbol: `0x20002AC8`.
Host results: `bench/2026-11-10-motor-continuous-v1-host-tests.txt`.
Flashed November 10 after EVM-off/USB-connected confirmation. Programmer verified
flash and reset; HOTPLUG read self-test `1` at `0x20002AC8` and GPIOB ODR `0`
(ENABLE LOW). Evidence: matching `bench/2026-11-10-motor-continuous-v1-flash.txt`
and `-self-test-swd.txt`. Powered continuous rotation remains unverified.
