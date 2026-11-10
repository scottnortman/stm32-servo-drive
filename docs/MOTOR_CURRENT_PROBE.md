# Bounded motor-current probe

This is the consolidated first motor-connected test Scott requested. It uses
the existing complete Nucleo/DRV harness, the known-working GM42BLF60-240,
and no external power resistor. Hall leads stay disconnected. No scope-marker
measurements or manual winding R/L measurements are prerequisites for this test.

The separate image is `build-bringup/drv8353-motor-probe.hex`, with banner:

```text
Diagnostic: DRV8353RS bounded MOTOR current probe v5
```

**Do not connect the motor to the older fixed20/40/60% diagnostic images.**
Version1 was flashed/readback verified and passed the startup shutdown self-test.
Its first motor run stopped during A->B on an ADC watchdog event; see
`bench/2026-11-09-motor-probe-v1-first-run-analysis.md`. The requested three
unpowered wire checks subsequently matched. Version2 adds feedback reporting,
was flashed/readback verified, and its startup software-triggered shutdown
self-test passed on November9,2026 (SWD0x20000EE4=00000001). Evidence is in
`bench/2026-11-09-motor-probe-v2-flash.txt` and
`bench/2026-11-09-motor-probe-v2-self-test-swd.txt`. Its powered motor run is
complete: it stopped with STOP6 after536 A->B updates, current still near zero.
See `bench/2026-11-09-motor-probe-v2-first-run-analysis.md`. V3 is built and
host-tested, flashed and readback verified. Its startup software-triggered
shutdown self-test passed (SWD0x20000F0C=00000001). Evidence is in
`bench/2026-11-09-motor-probe-v3-flash.txt` and
`bench/2026-11-09-motor-probe-v3-self-test-swd.txt`. Its run passed bus/timing
checks, then stopped with no-response failure5 at34.6ms;100mA was not reached.
It schedules regular bus/VREF conversions from
the injected ADC IRQ and distinguishes bus failure classes. Current and duty limits and
the controller module are unchanged. A scope capture is not required for the
next firmware-feedback run Scott requested. V4 is built/tested, flashed and
readback verified. Startup software-triggered shutdown self-test PASS was read
at SWD0x20000F0C=00000001. Evidence: `bench/2026-11-09-motor-probe-v4-flash.txt`
and `bench/2026-11-09-motor-probe-v4-self-test-swd.txt`. Its run showed the
expected current direction but stopped at20.55ms on a B-channel watchdog event.
It did not reach100mA or complete a phase pair. See
`bench/2026-11-09-motor-probe-v4-first-run-analysis.md`.
Following review of the working motor path, V4 uses its centered PWM convention,
150/300mA gates and100ns dead time. Comparison and rationale:
`bench/2026-11-09-motor-probe-v3-first-run-analysis.md`.

V5 is built/host-tested, flashed and readback verified. Its startup
software-triggeredshutdownselftest passed (SWD0x20000F24=00000001); evidence is
in `bench/2026-11-09-motor-probe-v5-flash.txt` and
`bench/2026-11-09-motor-probe-v5-self-test-swd.txt`. Its first powered run PASSED
all three phase pairs and ended STOP11 with MOTOR/SYNC/BUS failure0. Final
nominal A/B/C mA: A->B92/-97/-2,B->C1/85/-101,C->A-101/6/80. Max reported
active-pulse sampled magnitude168mA. This is nominal-current functional
validation, not independent current calibration or rotating control. See
`bench/2026-11-09-motor-probe-v5-first-run-analysis.md` and matching full log,
feedback CSV and summary JSON. No further firmware change after this PASS.
It tests a possible correlation between
the watchdog events and10ms SPI register scans by moving register checks to the
pulse boundaries. The working motor path likewise keeps SPI out of its running
current loop. PWM/controller/current thresholds are exactly as V4. This is an
isolation test, not a confirmed analog-noise fix.

## What one USER press does

1. Checks bus voltage, default registers, and faults with all bridge inputs LOW.
2. Configures protected3PWM operation and verifies each write twice. Sets CSA
   gain40 (R6=0x2C3), then records256 fresh zero samples per current channel
   under COAST with all INLs LOW.
3. Starts the timer at ZERO phase compares with all INLs LOW. Checks20kHz
   synchronous ADC phase, sequence intervals, and freshness for10ms before
   allowing differential motor voltage.
   V3 also proves16 IRQ-queued VREF/bus sample pairs at zero drive before a pulse.
4. Runs A->B, B->C, C->A stationary phase-pair tests. Each pair ramps its nominal
   current request to100mA over20ms and lasts at most250ms. The source phase
   and sink get equal/opposite compare changes around625/1250=50%; the third
   compare stays625. All three INLs are HIGH during a pulse. Matched50%
   preloads settle under COAST before clearing COAST. Between pulses all INLs
   go LOW, all phase compares go to
   ZERO, and COAST is set for at least100ms.
5. Disables the driver and reports completion or the first fault, followed by
   buffered current feedback through `FEEDBACK END`. There is no automatic restart.

V5 verifies all configured driver registers immediately before arming each
pulse and again after SysTick drops INLs at its end. There are no SPI transfers
during a pulse; an attempted transfer while motor_on stops before CS/SCLK.
Register verification is at most250ms apart rather than every10ms. nFAULT
shutdown,20kHz current/CCR/INL supervision and periodic ADC bus checks remain.

The motor may twitch or step. This is not a rotating-field or speed command.
Secure the motor, remove shaft loads, and keep hands clear of the shaft.

## Limits and supervision

- Target100mA is a **nominal shunt-derived current**, not an independently
  calibrated current measurement. The EVM's [7milliohm shunts](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/38/DRV8353RH_2D00_EVM-schematic.pdf)
  and verified40V/V gain give nominal280mV/A. Actual zero offsets are measured
  afresh. Current conversion uses factory-VREFINT-derived VDDA.
- Source-minus-sink compare is capped at24/1250=1.92%, approximately0.2304V
  average line-to-line at12V before switching effects. Individual leg duties
  are49.04..50.96%: long gate pulses, small winding voltage difference.
  Zero request gives matched50% duties. Odd compare requests round DOWN.
- Filtered current exceeding500mA or a raw channel sample exceeding1000mA stops
  the driver. An injected-channel ADC watchdog adds a coarse raw-current window.
  These protections operate on sampled current; they are not a continuous
  comparator guarantee of instantaneous peak current.
- Missing response at the drive ceiling, inconsistent active-pair currents,
  excess inactive-phase current, and altered PWM/INL configuration stop the test.
- A SysTick cutoff drops INLs at250ms independently of polling and UART. The
  complete armed sequence has a2second maximum. USER stops after20ms LOW.
- nFAULT EXTI, driver protection, bus/register/SPI checks, ADC rail checks, DWT
  interval checks, completion-phase checks, and a2ms stale-sample stop remain.
  Every stop drops ENABLE/INLs before further serial work. Physical fault
  injection and calibrated current accuracy remain unvalidated.

## Operator sequence after the image is verified

1. With EVM power OFF and Nucleo USB unplugged, connect the three motor phase
   leads to J6's MOTA/MOTB/MOTC terminals. Use the board's printed terminal
   markings; do not infer terminal order from a viewing direction. Leave Hall
   leads disconnected. Keep all existing signal/analog wires unchanged.
2. Power Nucleo USB. Confirm the exact MOTOR-current-probe banner and startup
   shutdown self-test PASS. ENABLE stays LOW while waiting.
3. Power EVM at12V with0.25A supply current limit. The supply setting is a backup
   input-current limit, distinct from the firmware's phase-current supervision.
4. Press/release USER once. Capture all serial output. No additional button
   action is required for the automatic sequence; USER can interrupt it.
5. Successful completion is three PAIR DONE lines, MOTOR_STOP failure0,
   SYNC_STOP failure0, STOP reason11 (complete), and ENABLE/PWM LOW. Send the
   entire log through `FEEDBACK END`. If any other stop occurs, preserve its
   lines before resetting. No scope setup is needed for this run.

## Output and failure codes

Version2 retains up to32 entries per phase pair at1ms spacing (the last32ms),
plus the first sample while it remains in that ring and any completed sample
that caused a software-current fault. Each `FB` entry reports the source CCR
command, raw A/B/C readings, and nominal raw/filtered currents. Sample indices
are converted to elapsed time at50us per update, rather than wall-clock timestamps.
`FEEDBACK_PAIR` includes raw min/max over every completed pulse sample, including
samples between history entries. Logs print only after the driver is disabled;
they cannot extend a pulse or consume the armed deadline between pairs.

`ADC_STOP` records status, raw ranks, watchdog thresholds, PWM commands and
controller progress immediately after ENABLE/INLs go LOW and the timer/next
trigger stop. An in-flight ADC sequence can still finish: these raw ranks may
span different sequences and are explicitly labeled `after_disable_may_partial`.
An AWD stop is distinguished from a software raw-current trip by the retained
AWD status. The existing `last_nominal_mA` and `SYNC_STOP last_raw` still describe
the preceding completed sequence, and must not be substituted for the trip snapshot.
V4 `diff_CCR` means source-minus-sink compare; ADC_STOP `CCR` still prints
actual A/B/C leg compares around625. This reporting does not independently
calibrate amperes or distinguish physical current from analog interference.
V5 adds `spi_active`, `spi_busy`, `spi_cs_low` and `spi_last_reg` to ADC_STOP.
These values are captured after ENABLE/INLs go LOW; they identify transfer
activity at shutdown, not an independently measured SPI-edge/noise correlation.

V3 `BUS_CHECK` distinguishes the STOP6 subcauses. Its `ref_bus_pairs` field is
the number of completed VREF/bus measurement pairs in the latest16-pair check,
not the number of motor phase pairs. It also prints the last requested ADC
channel, queue state (0idle,1requested,2running,3ready), sums and last computed
VDDA. Failure codes:0none,2conversion/request timeout,3calibration/VDDA/bus outside
limits,4queue overlap,5ADC overrun,6no regular EOC at the following injected
completion,7unsafe launch phase,8unexpected regular result. `IRQ_WORK` reports
maximum measured work after injected IRQ entry and the last exit timer phase.

Foreground bus reads now publish a request, with no foreground timer-window
polling or regular mux write during injection. The ADC IRQ consumes any prior
result and launches a request immediately after injection, before current-loop
work; actual launch phase is checked under a brief interrupt mask. A missing
result at the next sequence or a foreground wait reaching2ms stops the test.

PAIR DONE prints nominal filtered A/B/C currents, last differential CCR, observed
peak raw-current estimate, and the number of samples in the target band. During
the quiet sampling window source/sink shunt currents should oppose each other,
with the midpoint-phase channel near its zero. Global current sign is observed, not
assumed by the magnitude controller. Target recognition needs at least100
samples in70..130mA with opposite source/sink signs. This is a first response
check, not proof of tuned current-loop bandwidth or absolute current calibration.

STOP10 is a motor-probe fault. MOTOR_STOP failure codes are:

| Code | Meaning |
|---:|---|
| 0 | No motor-probe fault recorded |
| 1 | Invalid phase/PWM/INL configuration or rejected SPI during pulse |
| 2 | Raw current / ADC watchdog trip |
| 3 | Filtered current trip |
| 4 | Active-pair balance, sign, or inactive-channel mismatch |
| 5 | Drive ceiling reached without sufficient response |
| 6 | Pulse ended without recognizing the target band |

Other existing STOP codes remain:1 USER,2 nFAULT,3 deadline,4 SPI,5 register,
6 bus,7 startup self-test,8 ADC rail/baseline,9 synchronous timing/freshness.
STOP11 means the complete three-pair sequence finished.

## Software validation

`tests/motor_probe_test.c` tests the actual conversion/controller module using
WSL GCC with AddressSanitizer and UndefinedBehaviorSanitizer. Tests cover zero
and signed conversion, invalid phase assignment, extreme current inputs,
filtered-current shutdown, no-response shutdown, mapping errors, and24
dead-band/delayed synthetic regulation cases. The synthetic plant is test data,
not assumed resistance or inductance for this motor. Results are saved in
`bench/2026-11-09-motor-probe-v1-host-tests.txt`.

All five diagnostic variants build without warnings. Existing low-side,
bridge-scope, asynchronous ADC and synchronous ADC HEX hashes are unchanged.
No normal motor-control, encoder, Hall, or FOC module is linked to this probe.

Version2 additionally tests the real trace module for ring wrap/order, bounds,
decimation, signed feedback, extrema and retention of the observed C-trip values.
Both suites pass with ASan/UBSan; evidence is in
`bench/2026-11-09-motor-probe-v2-host-tests.txt`. The four older diagnostic HEX
hashes remain unchanged. Version2 HEX SHA256 is
`F5C141EC01685D4AC53D94E823B021E9B08DAD988EE67AB08558BF658D81E34A`.

V3 adds portable queue tests for delayed foreground,32 alternating reads,
result retention, overlap, overrun and missing/unexpected results. All three
ASan/UBSan suites pass; evidence is in
`bench/2026-11-09-motor-probe-v3-host-tests.txt`. This does not validate MCU
timing or establish that the observed hardware fault is fixed. V3 HEX SHA256:
`26070D477794E94016BC4B657A5C964BAA257440BAD5126FA30A43AF908695B7`.

V4 adds centered-PWM mapping tests and runs the24 synthetic regulation cases
through that exact mapping. All four ASan/UBSan suites pass, saved in
`bench/2026-11-09-motor-probe-v4-host-tests.txt`. Older diagnostic hashes stay
unchanged. V4 HEX SHA256:
`A59D239DB825AE423397BE4D565F797D9B376D77F9BA829895FB2FE3E85347F8`.

V5 builds warning-free and all four portable suites pass, saved in
`bench/2026-11-09-motor-probe-v5-host-tests.txt`. They do not validate analog
interference or hardware SPI timing. V5 HEX SHA256:
`C9D469614FFE26BFF756EA52AE4AB4BE0B9C2AEAF751B9991F9386E4B716C654`.
