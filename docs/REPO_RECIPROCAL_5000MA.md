# Reciprocal trial at 5.0 A, same slow motion

Scott reports improved motion at2.5A and explicitly requests double current,
keeping speed unchanged. Separate `repo-reciprocal-5000ma.elf/.hex` image.

- Same1s alignment,0.5s current ramp,20s per commanded revolution forward/back,
  repeating continuously until USER/fault. Trajectory source unchanged.
- Nominal phase-current magnitude reference5A;3.2V phase-voltage ceiling.
- Per-phase filtered trip6.5A, sampled raw trip8A.
- Bench supply12V/2.5A input limit. Prior0.75A setting cannot support this load.
- Gain20 rather than40 to leave sense headroom. Target compile definition
  SHUNT_GAIN=20.0f feeds the original ADC conversion; config.h only adds an
  override guard, retaining default40 for all other targets.
- Original driver configure runs unchanged. With PWM/INLs disabled, the adapter
  writes R6=0x283, verifies twice, waits for settling and takes fresh ADC zeros.
  Any mismatch stops before drive. No SPI traffic during motion.
- ADC acquisition source/IRQ/trigger unchanged; board sample-time setting28cycles
  at25MHz. Sequence4.8us; worst later trigger plus0.13us latency ends30.42us.
  MaxphaseCCR1915 gives next switching edge30.83us, leaving0.41us margin.
- Gain20 conversion0.005754743A/count;8A raw threshold1390counts. Bounds410..3690
  fit accepted offsets1800..2300 without underflow/overflow.
- USER20ms,nFAULT,ADC freshness/rail/offset guards retained. Bus preflight only;
  fixed12V assumed during run, matching earlier reciprocal images.

Build passes with the existing unused static header declaration warning. ELF
checks verify original call chain, ADC vector, early FPU startup, compiled gain20
conversion literal, default gain40, profile values and preserved2A/2.5A images.
The original PI/PWM/ADC/open-loop/trajectory implementations remain unchanged.

The EVM schematic shows56ohm/2.2nF current-output filters. With maximum7pF ADC
sampling capacitance, a simple charge-sharing/RC model settles below0.5LSB in
1.12us acquisition. This is a model check; bench data remains needed.

Sources: [TI gain codes and linear range](https://www.ti.com/lit/ds/symlink/drv8353.pdf),
[TI EVM schematic](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/38/DRV8353RH_2D00_EVM-schematic.pdf),
[ST ADC characteristics](https://www.st.com/resource/en/datasheet/stm32f411re.pdf).

HEX SHA256: 1AC5B931C3EE8C5C16FBF693AB3B330D7D44776B1958CA4B3FEC8C76634D23A3
Evidence: `bench/2026-11-10-repo-reciprocal-5000ma-*`.
Boot symbols: selftest0x200000C0,ticks0x200000BC,faultIRQ0x200000B0,
command0x20000068,moves0x2000006C,CSA readback0x20000094.

Flashed/verified/reset after fresh EVM OFF confirmation. HOTPLUG checks PASS:
selftest1,counters0,ENABLE/INLs/MOE LOW,ADC28cycles,original timer,PA4analog,
FPU enabled and no core fault. CSA setup remains pending USER/powered preflight.
After flash/disabled-output verification: apply12V/2.5A,USER start, compare one
forward/back pair then USER stop and inspect motion/current reports. Twice the
current produces approximately four times winding copper loss at fixed resistance.

## V4 configuration correction

V3 stopped CONFIG7 before drive. Powered no-motion A/B calls to original flashed
driver functions reproduced first extraCSAread0x2C2 after R2, then stable0x2C3.
Original configure verification passes; direct gain20 write + double read0x283
passes8/8 trials. Removed only the redundant pre-write R6 read, retaining original
configure verification, output-disable checks and both new-gain readbacks.
The underlying first-read LSB anomaly is not fully explained; it is not evidence
of a current or torque failure. No control/motion/protection parameters changed.
V4 HEX SHA256: BF83A3DD74126AEAD3E065C7E616C78093D8F2930245C0F1B7336C93ADC8E6D3
V4 build/link checks pass; awaiting EVM OFF before programming. RAM debug probe
currently ends disabled; do not rerun its v3 function addresses after v4 flash.

V4 flashed/verified/reset after fresh EVM OFF confirmation. Boot selftest PASS,
interrupts enabled, counters zero, ENABLE/INLs/MOE LOW, ADC28cycles and no core
fault. Ready for powered USER trial at same12V/2.5A input limit and5A phase target.
