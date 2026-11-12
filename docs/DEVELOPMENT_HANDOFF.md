# Development handoff

Last engineering state: 2026-11-11. This is a curated project handoff, not the
full chat transcript. Verify live hardware and Git state before acting.

## Completed 8 us / 5 A scope investigation, 2026-11-11

**Latest state: disarmed, outputs LOW, EVM OFF confirmed by user.** All three
continuous captures ended with physical USER stops, state0/reason1/result0.
No monitor or motion remains active. COM3 untouched. Final interface0x1000E;
saved Kp1.0/Ki2500 remains intact. This baseline is preserved by tag
`checkpoint/hall-24v-grounding-8us-2026-11-11`.

Motion24 selects R5=0x139, meaning8 us OCP qualification. Previous0x119 means
2 us; the stale4 us comment was corrected. Other OCP fields and other profiles
are unchanged. This is internal OCP qualification, not nFAULT-wire filtering.
The new consistency check initially blocked arming because the first R5 frame
was0x138. Final code retains that frame for diagnosis and requires the next
TWO reads to match0x139; actual readbacks confirmed0x138/0x139/0x139.

Two bounded8 us trials completed6 s and48 valid Hall transitions each, with
7086 and10033 already-HIGH interrupts versus7300 in the earlier2 us trial.
User then requested continuous5 A for scope capture. E permits duration0 at
5 A with a3.2 V ceiling and6.5/7 A trips;1..4 A remain bounded only. Existing
6 A behavior, USER/ADC/fault/timing guards, NV gate and disarmed apply remain.

| Scope ground | Run time | Already-HIGH IRQs | Valid Hall changes | Half moves |
|---|---:|---:|---:|---:|
| EVM ground bar | about147 s | 297616 | 1413 | 58 |
| STM32 ground | about96 s | 4 | 930 | 38 |
| Improved STM32-to-DRV ground | about52 s | **0** | 486 | 20 |

The user improved the permanent STM32-to-DRV ground and requested another
repeat. Same E image/settings:51.84 s, ZERO total nFAULT IRQs,486 valid Hall
changes,20 half moves, USER stop. User reported similar scope ripple and
confirmed EVM OFF afterward. Final apply2/start3/host stop0. Probe attachment
was not separately reconfirmed, so do not claim a proven probe-free repair.

No invalid/multi-bit Hall transitions, CPU/Recorder faults or active-fault
captures occurred. IRQ maximum34.5 us. The3.2 V ceiling became active during
long runs: means4.942/4.899 A, final10 s means4.854/4.808 A. It was not raised.
The ground connection is strongly implicated, but probe loading and changing
motor/current conditions prevent claiming a permanent repair or exact cause.
Already-HIGH interrupts are not verified DRV fault assertions. The existing
IRQ policy also rejects a genuine pulse ending before PB1 is sampled.

Visible ripple remained in both photos. Second photo:20 us/div,200 mV/div,
Vpp450 mV/max520 mV, trigger3.25 V, period50.2 us/frequency19.9 kHz, consistent
with20 kHz PWM. Boxed B means the scope's20 MHz bandwidth limit is ON (Rigol
manual2-3). Probe attenuation/DC coupling, absolute Vmin and resolved glitch
width were not confirmed. Next diagnosis: full-bandwidth, matched-probe/DC
measurement with short MCU ground spring; inspect the logic-ground/nFAULT
return path. No further powered run is currently requested.

Full ARM build, upstream23 tests/42 checks, tuning/Recorder/mailbox/NV
ASan/UBSan and profile contracts PASS, including continuous5 A repetition and
explicit stop. Compiled IRQ4 cases, SPI reader10 cases and Hall GPIO audit PASS.
Each flash used fresh OFF confirmation, sectors0/1 only, preserved sector7,
exact flash/BIN comparison and disabled-output boot checks. Final E HEX SHA256:
`f96b7e43aee2876464965038d0b22f1a562e359a67bb771011d12b5958b58b28`.

Detailed report: `bench/2026-11-11-ocp-deglitch-8us-24v-5a.md` and matching JSON.
Comparison PNG/raw ZIP: `bench/current-tuning/ocp-deglitch-8us-24v-5a-20261111.*`.
Exact images, symbols, full checks, scripts and manifest are under
`outputs/checkpoints/ocp-deglitch-8us-24v-5a-2026-11-11`: C at root, D in
`verified-readback`, final E in `continuous-5a`. Matching Hall MCUViewer is STOP
on the focused capture group. Fresh OFF confirmation before future flash/reset.

## Next task: velocity-loop commissioning

User requested commit/tag/push/checkpoint of the validated Hall/grounding state,
then velocity-loop tuning. The current Hall code is observation only; no Hall
rotor-angle or measured-velocity feedback drives the PI yet. First verify Hall
sequence, mechanical scale and angle mapping, then commission a bounded
current-limited velocity loop using the existing PI/ADC/PWM/trajectory modules.
Keep current-loop Kp1.0/Ki2500 fixed. Commanded D-axis rotation is not measured
speed. Preserve the independent bench images and every existing hardware guard.
No hardware action is part of the Git checkpoint. EVM remains OFF.

## Historical 8 µs OCP deglitch comparison, 2026-11-11

User requested changing DRV OCP_DEG to8 µs and repeating the bounded5 A spin.
Motion24 only now selects R5=`0x139` (previous`0x119` actually2 µs; stale4 µs
source comment corrected). Latched OCP,100 ns dead time and0.6 V VDS threshold
are unchanged. Other images retain`0x119`. This qualifies the internal OCP
detector; it is not a filter on the nFAULT wire or STM32 EXTI input.
The existing already-HIGH IRQ policy remains for comparison; a genuine brief
assertion ending before the handler reads PB1 would also be rejected. Prior
spin success therefore does not establish that these events are harmless.

The initial C image's added two-read guard stopped at zero output/reason7,
R5 second read0x139/CSA not reached. Revised interface`0x1000D` records the
first frame, then requires TWO subsequent R5 reads equal0x139. Actual arming
readbacks were0x138,0x139,0x139, confirming the first-frame low-bit artifact.
Telemetry: `g_repo_ocp_first`, `g_repo_ocp_verify_first`, `g_repo_ocp_control`.
Diagnostics are cleared at each attempt. Full ARM/upstream23tests42checks/
tuning sanitizer/profile contracts and compiled IRQ4/SPI10/Hall audit PASS.

After fresh EVM-OFF confirmation, flashed/verified sectors0/1, exact uploaded
flash matches BIN, boot selftest/NV1/2500/outputLOW/CPURecorderfault0. Matching
ELF and all20 GUI variable addresses updated. D archive is
`outputs/checkpoints/ocp-deglitch-8us-24v-5a-2026-11-11/verified-readback`.
HEX SHA256`d59ab28c7b113b23f44c43fb0a8660db4664ef294b0b0a98fa0dfc7a7471205c`.

First8us5A/6s/3.2V cycle PASSED at24V/3A input, bus23.763V preflight,
NV1/2500, two half moves,48 valid Hall changes, invalid0/multibit0.
Mean5.00031A/MAE0.01952A, sampled peakV3.11619V, no sampled saturation;
inactiveIRQ delta7086 vs prior2us5A7300. CPU/Recorderfaults0, IRQmax34.24us,
no active fault capture, outputsLOW, state0/reason9/result0. User reported
normal motion and confirmed EVM OFF, then requested a repeat while observing
nFAULT on a scope. A single comparison does not establish a deglitch effect
or electrical cause; counts remain high.

Current: user confirmed scope-ready24V/3A ON, same5A6s3.2V reapplied disarmed
(apply2/start1/stop0), now awaiting fresh USER arm. Scope probe PB1CN10-24 to
STM32GND, optional second at EVMJ1-16; suggested falling1.5V trigger/1usdiv.
Next use D bench_support snapshot('scope-run-5a-armed',1), start acquisition,
run(5,'scope-run-5a'), snapshot after. Preserve run-5a-* first-run evidence;
never overwrite it. No flash/reset needed. After capture ask scope waveform
observations and physical EVM OFF. COM3 untouched.

## Previous inactive nFAULT edge policy and 1–5 A ramp, 2026-11-11

**Latest state: all five bounded cycles passed; user confirmed EVM OFF and
normal shaft motion.** Firmware interface0x1000B, saved Kp1.0/Ki2500, fixed24 V
with3 A supply-input limit. Each1/2/3/4/5 A trial completed six seconds, two
half moves, and48 qualified Hall transitions. All six codes were seen; invalid
and multi-bit counters remained0 (240 transitions total). Outputs were verified
LOW after every run. No active fault captures, CPU faults or Recorder faults;
maximum observed control IRQ34.25 us, below the45 us guard. COM3 untouched.

| Requested A | Plateau mean A | MAE A | Inactive nFAULT IRQs during run |
|---|---|---|---|
|1|0.9920|0.0301|0|
|2|1.9999|0.0199|38|
|3|3.0004|0.0215|27|
|4|4.0019|0.0197|1538|
|5|5.0010|0.0202|7300|

These are100 Hz trend statistics after the current reference reaches99.9%
of its target, not raw20 kHz extrema. The1 A/1 V test has26 sampled saturation
flags;2–5 A have none. At5 A sampled peak voltage3.0806 V, below its3.2 V ceiling.
The software false-stop path is fixed and reproduced successfully on hardware.
**Physical pulse origin remains unproven:** already-inactive nFAULT events
persist, especially at4/5 A. Next diagnosis should measure PB1/nFAULT, ground
and bus electrically; do not infer that every resolved transient is harmless.
No protection mode or threshold was relaxed. No additional motion is running.

MCUViewer is STOP on the focused `Hall and rotation capture` group with5 A
curves retained. Original controls and Recorder groups remain. XY X is commanded
mechanical revolutions assuming four pole pairs, not independent shaft angle.
Saved project: `tools/tuning/current-loop-free-hall-24v.mcuv`, matching the
archived B ELF. Evidence and full debug report:
`bench/2026-11-11-current-ramp-fault-fix.md` and matching JSON; selected trends,
comparison PNG and reproducible raw ZIP under `bench/current-tuning/current-ramp-fixed-*`.
Exact firmware, before/after SWD checks, raw curves, analysis script and SHA256
manifest: `outputs/checkpoints/current-ramp-fault-fix-24v-2026-11-11`.
Source remains uncommitted. Fresh OFF confirmation required before a future
flash/reset; the final OFF confirmation is not authorization for another run.

### Investigation and software verification

User explicitly requested fixing the fault, starting at1 A and ramping upward.
Continue through verification, not another unexplained stop. Source remains
uncommitted. The1 A six-second out/back trial passed at24 V/3 A input: two
half moves,48 valid Hall changes, invalid0/multibit0. At2 A, the first diagnostic
captured R0=1/R1=0/R2=0x20 with nFAULT already HIGH before ENABLE was lowered.

A refined diagnostic (flashed interface0x1000A) captured2 A again: GPIOB at IRQ
entry0x11b, before SPI0x15b (PB1/nFAULT HIGH in both), first R0=1 but repeated
R0=0, R1=0, R2=0x20. Thus the original phase-C fault interpretation of the first
read was not reliable. Capture237.03 us, outputs disabled before SPI, eventual
ENABLE LOW. These show the software bug: EXTI unconditionally converted a
pending falling edge into a stop even though nFAULT was already inactive HIGH.
The physical source of the short pulse is unproven (EMI versus resolved transient).
Do not claim that SPI status proves the absence of every transient supply event.

The fix, motion target only, acknowledges/counts an already-HIGH edge without
calling stop_drive. LOW nFAULT still calls the established immediate shutdown,
and SysTick retains its independent active-LOW level check. A boot-only flag
preserves the software fault-IRQ test with all outputs disabled. Protection
thresholds/modes are unchanged; no fault is masked while nFAULT remains LOW.
`g_repo_fault_irq_rejected` counts these inactive edges. Root regression executes
the compiled Thumb IRQ: old v2 FAILS inactive-edge case; fixed code PASS for
inactive edge, active-LOW stop, zero-output boot test and no pending interrupt.

Diagnostic support remains: first make INLs LOW and clear MOE; for active faults
only, read R0 twice/R1/R2 under one30000-cycle DWT budget, then ENABLE LOW on
success or failure. Reader refuses if phases/MOE are enabled, ENABLE is low,
SPI busy/disabled or DWT disabled. Actual2 A capture was237.03 us; a real fault
capture can inflate the recorded TIM1 IRQ maximum after outputs are already off.
During USER arming, zero-output reader self-check requires both R0 reads and R1
zero plus R2=0x20. Original ENABLE-immediate behavior remains on other stop paths.
TI SLVSDY6A Table2 confirms3PWM INL=0 means Hi-Z; latched OCP and register-reset
behavior are documented in8.3.6.3.1/8.4.1.1.

Motion interface0x1000B supports integer1..5 A, duration6 s only, one-second
alignment and2.5-second legs; voltage ceilings1/1.6/2.2/2.8/3.2 V. Filtered trips
1.25*amplitude+0.25 A, raw another0.5 A (1 A:1.5/2 A;5 A:6.5/7 A). Existing6 A,
stationary24 V and12 V profiles remain isolated. NV1.0/2500 gate unchanged.
Full ARM build/upstream23 tests42 checks/tuning ASanUBSan PASS. Ten compiled
fault-SPI refusal/timeout cases PASS; four compiled IRQ regression cases PASS.
Tools: `test_fault_snapshot_arm.py` and `test_fault_irq_arm.py` require unicorn;
local ignored `outputs/test-deps` has it. Emulation validates software paths,
not real electrical timing. The subsequent1–5 A hardware verification above passed.

Fixed candidate is archived in `outputs/checkpoints/current-ramp-fault-fix-24v-2026-11-11`.
HEX SHA256 `de46fa8006200989e5a10d60efede30ef7378f56afbf0bcfca301d3d4b9fe322`.
MCUViewer Hall/rotation view references that ELF, acquisition STOP. Candidate B
was flashed after fresh EVM-OFF confirmation; verified sectors0/1, preserved NV,
interface0x1000B/selftest1, outputs LOW, faultIRQ test1/rejected0, CPU/Recorder
faults0. User requested checking Hall IO before flashing: compiled before/after
startup comparison for5 GPIO seeds passes (only PB6/7/8 modes/pulls differ);
all8 Hall codes with PB1 toggling cause no peripheral writes. Live preflash
GPIOB MODER0x281/PUPDR0x15100, SYSCFG EXTICR1=0x10, EXTI IMR/FTSR=2/RTSR=0
confirm PB1 input/routing and no Hall interrupts. Hall code does not alter PB1.
Physical coupling/supply effects remain unexcluded; no software pin overlap found.
The subsequent hardware ramp above is complete. Do not repeat it automatically.

Evidence: tracked `bench/2026-11-11-current-ramp-diagnosis.json`;1/2 A selected
trends in `bench/current-tuning/current-ramp-1a-20261111-trend.json` and2a equivalent.
Raw v1 archive `current-ramp-diagnostic-24v-2026-11-11`; v2 archive
`current-ramp-diagnostic-24v-v2-2026-11-11`; both under `outputs/checkpoints`.
COM3 untouched. Earlier failed-run evidence remains historical and unchanged.

## Historical 5 A retry before the fault-handler fix, 2026-11-11

User explicitly requested a5 A repeat after the6 A nFAULT stop. Added a bounded
24 V mode4 profile:5 A, duration6 s, voltage ceiling<=3.2 V, same1 s alignment
and2.5 s cosine legs, persisted Kp1.0/Ki2500. Continuous5 A is rejected.
5 A filtered/raw trips are6.5/7 A;6 A retains7.5/8 A. Existing stationary24 V
and12 V validators, USER/ADC/nFAULT/timing guards remain. Motion interface
now0x10008; boot banner identifies connected Hall observation correctly.
`--hall24` now uses Hall state and commanded mechanical revolutions (replacing
the Hall count live slot; count remains available over SWD). Live time traces
and XY Hall code vs commanded revolutions are configured. Rotation is the
open-loop command assuming4 pole pairs, not an independently measured angle.
The generator sets schema3 `x_axis.variable` after MCP save and reopens the
project because MCP cannot configure the XY source. Voltage/saturation remain
live in the controls table and in the unchanged five-signal current Recorder.

Full ARM build and upstream23 tests/42 checks PASS; ASan/UBSan tuning suites
PASS including5 A validator limits, RL tracking, bounded out/back completion,
mailbox persisted-gain/arming gates. Fresh user EVM-OFF confirmation obtained;
flashed and verified sectors0/1 only, preserving sector7. Archive:
`outputs/checkpoints/hall-rotation-24v-5a-2026-11-11`, motion HEX SHA256
`2552d2d7ecd60ce555183423e93a57ffda127182f182516072aea9c0f6892e26`.
Boot interface0x10008/selftest1/NV1.0/2500, outputs LOW, pulls correct,
Recorder fault0/max512, CPU faults0. All GUI addresses and XY source verified.
MCUViewer1.4 crashed during project/plot-modal recovery (access violation);
reopened existing app and project, no MCU reset. User confirmed24 V/2 A ON,
motor assembled/secured and USER unarmed. Powered boot checks passed; applied
mode4/axis0/5 A/duration6/3.2 V/Kp1/Ki2500 while disarmed, result0. GUI acquiring
on Hall live group. The5 A/2 A-input attempt then stopped on nFAULT/reason2 at
host elapsed1.546 s, commanded0.0810644 rev, zero completed half moves.
Mean current4.99850 A/MAE0.01528 A, sampled peak5.04557 A, voltage peak3.00345 V,
no sampled saturation. Six qualified Hall changes, seen0x74, invalid0/multibit0;
physical fault IRQ1->2, IRQ max29.34 us, Recorder fault0/max512, CPU faults0.
Outputs LOW and user confirmed EVM OFF. Original reason2 is preserved (no
cleanup STOP overwrite). Report `bench/2026-11-11-hall-rotation-24v-5a.json`;
trend `bench/current-tuning/hall-rotation-24v-5a-20261111-trend.json`.
The earlier6 A post-stop raw readback gives0.0819239 commanded rev: both faults
occurred at approximately the same commanded position. Cause remains unknown.

User suspected supply inrush and raised the supply-input limit, initially
without recalling its value. Established a known24 V/3 A input setup; user
confirmed ON/USER unarmed. MCU actually read armed-idle state1, so sent STOP
while at zero drive, then re-applied the same5 A/6 s/3.2 V settings disarmed.
State0/reason9/result0; apply2/start1/stop1, then fresh USER arming confirmed.
Armed bus23.783 V, outputs OFF, no CPU/Recorder fault. Issued start2; the3 A-input
comparison stopped on nFAULT/reason2 after host elapsed1.500 s, commanded
0.0825281 rev, half moves0. Mean5.00158 A, MAE0.01781 A, sampled peak5.07655 A,
voltage peak2.99775 V/no saturation. Six additional Hall transitions, invalid0/
multibit0. Physical fault IRQ7->8 (other edges occurred during intervening power
cycling); IRQ max29.34 us, Recorder fault0/max512, CPU faults0. Outputs LOW;
user confirmed EVM OFF. No reset/flash or further run. Source unchanged and
all motor guards retained. Raw captures in `supply-3a` preserve2 A evidence;
tracked comparison report is updated and selected3 A trace is
`bench/current-tuning/hall-rotation-24v-5a-input3a-20261111-trend.json`.
MCUViewer STOP on Hall live group; XY plot retains captured Hall-vs-command data.
Raising the input limit2->3 A did not prevent nFAULT near the same commanded
position; this alone does not rule out a transient bus dip (only preflight bus
was measured). Diagnose nFAULT source before more powered cycles. No guard
bypass or completed out-and-back/Hall-reversal validation should be claimed.

## Internal Hall pull-ups and motion observer, 2026-11-11

User requested internal pull-ups followed by D-axis motion to observe Halls.
`Include/hall_inputs.h` configures PB6/PB7/PB8 as inputs with3.3 V internal
pull-ups, preserving all other GPIO fields. Both manual `hall-bringup.elf`
and `repo-current-motion-24v.elf` use it; other firmware profiles do not.
Motion interface is now0x10007. The motion observer runs at20 kHz while
disarmed or running, accepts three consecutive equal samples, and exposes
UVW code, transition count, seen mask,000/111 entries, multi-bit transitions
and transition period. It does not feed the controller or change motor guards.
Use `mcuviewer_project.py --hall24 --elf <matching ELF>` for the20-slot Hall
view (state/count replace commanded revolution/half-move display).

Software validation: upstream23 tests/42 checks PASS; current-tuning12 V,
24 V stationary and24 V motion ASan/UBSan suites PASS, including Hall GPIO
preservation, forward/reverse sequences, brief-glitch rejection and diagnostic
counters; full ARM build PASS (existing vendor Recorder signedness warning).
Hardware update: user confirmed EVM OFF and completed5 V sensor/common-ground/
PB6/PB7/PB8 wiring with no5 V signal pull-ups. Flashed the archived motion HEX,
verified sectors0/1, then reset with EVM OFF. Zero-output boot PASS:
interface0x10007, self-test1, NV1.0/2500 intact, state/reason0, Recorder fault0 /
max522cycles, CPU CFSR/HFSR0. GPIOB MODER0x281/PUPDR0x15100 confirm input
modes and pulls; ENABLE/INLs/MOE LOW. Initial qualified/raw Hall3 (011),
changes0/seen0x08/invalid0/multibit0. Boot readback is archived as
`boot-check.json` and `boot-readback.txt`. No powered motion yet; requested
one manual turn and back with EVM kept OFF and USER unarmed. Completed manual
check: all six codes seen (mask0x7e),30 transitions, invalid0/multibit0, final3.
The100 Hz trace repeats3,1,5,4,6,2 in one order despite reported out-and-back
hand motion; actual direction reversal and shaft angle remain unverified.
Saved `manual-hall-curve.json`/`manual-hall-summary.json` in the archive and
tracked summary `bench/2026-11-11-hall-observation-24v.json`.
Powered attempt: user confirmed assembled/secured/unloaded motor24 V/2 A ON,
then USER armed after disarmed mode4/axis0/6 A/Kp1/Ki2500/duration6/4 V apply.
Armed preflight23.735 V, outputs OFF, no faults. One start was issued; at host
elapsed1.515 s the drive stopped on reason2/nFAULT, before completing the cycle.
Physical fault IRQ count rose1->2. Six additional qualified Hall transitions,
invalid0/multibit0; mean selected current5.99315 A, MAE0.01777 A, sampled peak
6.11386 A, peak voltage3.53154 V, no sampled saturation. IRQ max2756cycles
(27.56 us), Recorder fault0/max522, CPU faults0. Outputs ENABLE/INLs/MOE LOW.
The host exception cleanup sent STOP after the fault, changing disarmed reason
to9; the saved initial reason2 is authoritative, NOT bounded completion.
Raw/progress/curves/preflight/post-fault checks are archived; selected trend is
tracked at `bench/current-tuning/hall-24v-20261111-fault-trend.json`.
User confirmed EVM OFF, brief motion normal, nFAULT J1-16->PB1/CN10-24 wiring
intact, no supply-limit event/fault LED noticed. Exact DRV status cause was not
captured; existing shutdown lowers ENABLE and resets driver fault registers.
No retry/reset after this event. Investigate nFAULT source before another
powered cycle; full out-and-back Hall reversal remains unvalidated.
Existing serial banner still says Halls disconnected; this Hall observation
run uses electrically verified connected Halls, with no Hall control feedback.
MCUViewer is STOP on the prepared Hall live group; all20 addresses checked
against the archived ELF. Candidate files are in ignored
`outputs/checkpoints/hall-pullups-24v-2026-11-11`; motion HEX SHA256 is
`53f5a9b852e4532ec93420f3920957896c5a3dbb0ce53daff89ce2932371ce8e`.
Any subsequent flash/reset requires fresh EVM-OFF confirmation. Internal pulls are30..50k and may
need external3.3 V pull-ups if powered switching introduces noise. The current
observations showed no qualified Hall invalid/multibit transitions, but do not
establish noise margins or the cause of the separate nFAULT indication.
The saved Kp1.0/Ki2500 journal is preserved; last confirmed EVM OFF.

## Hall identification update, 2026-11-11

Scott reports all Hall outputs HIGH and partly legible Honeywell `41F533`
markings. Likely SS41F (`41F` package mark plus unconfirmed traceability suffix).
Honeywell specifies4.5..24 V operating supply and open-collector outputs, so
EVM3.3 V is below rating if this identification is correct. Recommend sensor5 V
power with separate3.3 V signal pull-ups; Hall outputs may go directly to STM32
PB6/PB7/PB8 because our DRV runs3xPWM and does not use Halls directly. Optional
EVM direct1xPWM links R48/R50/R52 remain DNP. Nucleo +5 V output is CN6-5 or
CN7-18. Do not inject5 V into EVM J3-1 or bridge3.3 V rails. Proposed wiring,
primary-source links and uncertainties: `docs/HALL_SENSOR_SS41F_REVIEW.md`.
No new5 V Hall test or wiring/firmware changes have been performed. The current
motion image still ignores Hall feedback; observation-only Hall build remains
unflashed. Present power state unknown; fresh OFF confirmation before any reset.

## Hall-power clarification, 2026-11-11

Scott found EVM supply power was disconnected; after restoring it, he reports
Hall 3.3 V works. No Nucleo supply jumper is needed. TI SLVUB79A section 2.1
(page 6) and MD016A Rev A show the Hall supply comes from the on-board LDO,
via R36 to J3-1 and the Hall pull-ups. J1-2 exports that same rail to the original
controller. Keep Nucleo/EVM 3.3 V rails separate. Hall output transitions and
motor sensor voltage compatibility remain unverified. This was a user-reported
power check, with no firmware or hardware action by the agent. Current EVM
power state must not be assumed OFF from the earlier motion checkpoint.

## Latest checkpoint: 24 V rotating D-axis motion

User requested the previous 6 A, 2.5 s-per-leg open-loop shaft-motion test with
new Kp1.0/Ki2500 at 24 V. Added separate `repo-current-motion-24v.elf` and GUI
`tools/tuning/current-loop-free-motion-24v.mcuv` (interface 0x10006). The existing
12 V and stationary-only 24 V targets remain separate. Motion uses the same
repo PI/PWM/ADC and cosine trajectory: 1 s alignment, then one commanded
mechanical turn each way in 2.5 s legs. No rotor feedback is available.

The new target requires exactly 1.0/2500 loaded from NV, configuration applied
while disarmed, physical USER arming and a fresh start. Mode4 accepts 6 A with
a 4 V vector ceiling, duration6 for one bounded check or duration0 for continuous
motion until USER/GUI stop/fault. Motion trips remain 7.5 A filtered/8 A raw,
45 us IRQ guard; stationary tests retain 0.5 A reference, 0.1 V voltage steps,
2 V vector ceiling and 1/1.5 A trips. Bus preflight must be 22..26 V. There is
no SWD heartbeat: viewer disconnection alone does not stop continuous motion.

After fresh EVM-OFF confirmation, flashed/verified sectors0/1 only; saved sector7
gains were retained. Exact image is archived in ignored
`outputs/checkpoints/d-axis-reciprocal-24v-6a-2026-11-10`. HEX SHA256:
`504ddba2acf0e45edbb64e547c2b8da5b28ce4f4ebcdb846c519ae6cb5bf8dea`.
Boot: interface0x10006, self-test1, NV1.0/2500, disabled outputs, Recorder fault0 /
max516cycles, CFSR/HFSR0. User powered fixed24 V with a 2 A supply input limit,
secured unloaded motor/Halls disconnected; applied settings after power-up,
then physically armed USER. Bus preflight was 23.735 V.

A firmware-bounded 6 s cycle completed two half moves and auto-disarmed, reason9.
Selected current mean5.99693 A, MAE0.01908 A, peak D voltage3.62512 V and no
sampled saturation. IRQ maximum3252cycles; outputs LOW and CPU/Recorder faults0.
User reported motion normal and re-armed for continuous mode. The continuous
run lasted approximately46.76 s in the saved trace and completed18 half moves
before physical USER stop, reason1. Selected current mean6.00027 A,
MAE0.01784 A, peak D voltage4.000 V, three of4535 selected trend points flagged
vector saturation. Maximum cross current0.13781 A. IRQ maximum3277cycles,
Recorder fault0/max516cycles, CFSR/HFSR0 and ENABLE/INLs/MOE LOW. User reported
**smoother than the prior12 V run**. Actual shaft travel/speed remains unmeasured.

At about32 s the host monitor received HTTP404 from MCUViewer MCP. A fresh
`connect()` re-established the API session without resetting/flashing; GUI
acquisition continued and the full trace was saved. This is distinct from the
earlier SWD core-ID failure. The failed exception-path stop write did not reach
the MCU; the verified stop was physical USER. Future long monitor helpers should
renew/reconnect MCP sessions on404 without recreating projects or resetting.

Final: user confirmed **EVM OFF** after stopping; drive disarmed, zero reference
and voltage, outputs LOW. Viewer STOP on matching motion controls, mode4/6 A /
duration0/4 V still applied in RAM; USER plus fresh start remain necessary.
No further flash/reset occurred after the powered tests. Source and evidence
are recorded by tag `checkpoint/d-axis-reciprocal-24v-6a-2026-11-10`. Fresh OFF
confirmation is required before any later flash/reset; COM3 remains untouched.

Software validation: upstream23tests/42checks, all three tuning variants and
real Recorder/mailbox/NV/profile suites under ASan/UBSan pass, including bounded
completion and two continuous cycles; full Arm firmware build passes. Five GUI
profiles retain20 unique live variables. These checks are separate from the
bench observations above. Evidence: `bench/2026-11-10-d-axis-reciprocal-24v.json`;
raw trends and manifest: `bench/current-tuning/24v-d-axis-motion-2026-11-10.zip`;
plot: ignored `outputs/tuning/24v-motion-continuous.png`. Motion metrics use
100 Hz live trends, not synchronous20 kHz Recorder data.

## Latest checkpoint: 24 V retune and bandwidth complete

After two PC resets, the 24 V work resumed on 2026-11-10. Fresh EVM-OFF
confirmation was obtained in chat. The dedicated `repo-current-tuning-24v`
image was flashed and verified, erasing sectors 0/1 only; sector 7's saved
Kp0.8/Ki2000 record remained unchanged. HOTPLUG write could not erase;
under-reset programming succeeded while EVM power was OFF. Disabled boot
passed: interface0x10005, self-test1, state/reason/result0, zero reference and
voltage, Recorder fault0/max512cycles, CFSR/HFSR0, ENABLE/INLs/MOE LOW.

Exact flashed ELF/HEX are in ignored
`outputs/checkpoints/current-bw-24v-recovery-2026-11-10`. The HEX SHA256 is
`c8c4971245faba7bb05426940c111d08adec9c54b88512eef93fe29905baa3f6`.
This includes the 24 V UART correction to the 24 V UART boot
descriptions, so it differs from the earlier unflashed candidate archive.
MCUViewer was restarted and `tools/tuning/current-loop-free-24v.mcuv` was
generated against that exact archived ELF. Scott confirmed fixed 24 V with
0.25 A input limit, secured unloaded motor, Halls disconnected and USER arm.
Bus preflight read 23.714 V. Positive 0.1 V D/Q steps produced positive current
changes of 0.113/0.141 A. Compared gains 0.2/50, 0.8/2000, 1.0/2500 and
1.2/3000. Selected **Kp1.0/Ki2500**: 0.5 A step rise D0.40/Q0.25 ms,
final means D0.49993/Q0.50026 A, no saturation at the 0.75 V ceiling.
Crossing estimates use three-sample/150 us smoothing on raw 20 kHz data.
The 1.2/3000 pair briefly saturated that ceiling and was not selected.

Eighteen raw 20 kHz fixed-tone captures cover 20, 50, 100, 200, 400, 800,
1200, 1600 and 2000 Hz on both axes, reference 0..0.5 A, 2 V vector ceiling.
All had zero saturation. Closed-loop -3 dB interpolation gives **D1.048 kHz**
(800..1200 Hz bracket) and **Q1.226 kHz** (1200..1600 Hz bracket), relative
to each axis's median 20/50 Hz response. These are sparse stationary-axis
estimates, not full MIMO, rotor-angle or high-current validation.

Before a finer sweep started, SWD stopped connecting. CubeProgrammer detected
ST-LINK but could not read the MCU core ID, even under reset. Scott held USER,
confirmed EVM OFF and unplugged/reconnected Nucleo USB; HOTPLUG then recovered.
Cause is unestablished. Pre-reset IRQ/Recorder maxima and CPU fault registers
were not recovered. Each completed test returned normal armed-idle status;
later boot checks cannot replace the missing pre-reset diagnostics.

Scott explicitly authorized saving the new gains in the shared NV journal.
With EVM OFF and outputs LOW, save returned status2; the record at 0x08060018
and its CRC were independently checked. A software reset reloaded 1.0/2500,
NV status1, self-test1, state/reason0, zero drive, Recorder fault0/max512cycles,
CFSR/HFSR0 and ENABLE/INLs/MOE LOW. **Final EVM state: confirmed OFF.**
MCUViewer is STOP on live controls in the matching 24 V project.
The 12 V build is preserved, but its older motion guard requires restoring the
archived 0.8/2000 NV gains before use; both images share the gain journal.

Recovery validation passed: upstream23tests/42checks; both 12/24 V tuning,
actual Recorder, NV and profile host suites under ASan/UBSan; affected 24 V,
12 V and default application targets and the full firmware build pass. Evidence:
`bench/2026-11-10-current-bandwidth-24v-recovery.json`. Raw data:
`bench/current-tuning/24v-recovery-2026-11-10.zip`, with a SHA256 manifest;
all 28 captures were checked and bandwidth reproduced after extraction.
The persistent gain profile is `tools/tuning/current-loop-24v-gains.json`.
Plots: ignored `outputs/tuning/24v-recovery-bandwidth.png` and
`outputs/tuning/24v-recovery-steps.png`. Investigate the SWD interruption before
sustained motion. This checkpoint supersedes the historical hardware state and
saved gains in the sections below. Fresh EVM-OFF confirmation remains
required before any later flash/reset; COM3 remains the user's console.

## Goal and user preferences

Commission the NUCLEO-F411RE + DRV8353RS-EVM + GEMS GM42BLF60-240 motor, tune
current, velocity and position controls, and ultimately run the full example.
Reuse the known working repo code as much as possible. Scott wants efficient
bench iteration with live plots, not replacement controllers or repeated
diagnostics that do not advance the work.

All development belongs locally or on Scott's GitHub account, `scottnortman`.
The original `jnzim` repository is only an upstream source for receiving updates.

## Repository and local folders

- `origin`: https://github.com/scottnortman/stm32-servo-drive.git
- `upstream`: https://github.com/jnzim/stm32-servo-drive.git (fetch only).
- Pushes through `upstream` are locally disabled; default push remote is `origin`.
  A fresh clone does not inherit local Git configuration: verify these settings.
- Active integrated branch: `scott/upstream-integration`.
- Preserved baseline branch: `scott/nucleo-current-tuning`, commit `836b762`.
- Bench baseline tag: `bench/current-pi-2026-11-10`.
- Repeat-results/source checkpoint tag: `checkpoint/current-pi-repeat-2026-11-10`.
  This tags integrated source plus the repeated bench evidence; the tested MCU
  firmware remains the pre-integration baseline, not the integrated build.
- Integration merge: `321aec5`, parents baseline + upstream `eda7136`, incorporating
  179 newer upstream commits. Later documentation commits build on this merge.

On Scott's PC the integrated worktree is
`D:\git\jnzim\stm32-servo-drive-upstream-integration`.
The original folder `D:\git\jnzim\stm32-servo-drive` keeps the baseline and local
builds/checkpoint history. Both are local Git worktrees sharing repository history;
the word `jnzim` in the folder path is only a directory label.

## What has actually worked on hardware

The unloaded motor is correctly connected. A ten-electrical-turn experiment
produced about 2.5 shaft turns, supporting four pole pairs. The earlier V6
reciprocal image worked at nominal 6.25 A, one commanded mechanical revolution
each way, five seconds per leg. It has no rotor feedback; actual trajectory
tracking was observed by the user, not measured by an encoder.

The subsequently flashed pre-integration current-tuning image uses the actual
repo PI/PWM/ADC code plus a bounded experiment and RAM-parameter adapter.
MCUViewer FREE works through ST-LINK: 20 live variables, five Recorder signals,
trace unused. Host live polling is 100 Hz for trends/editing. Actual synchronous
Recorder captures are 20 kHz / 50 us, 1638 samples per five-signal buffer.

Small positive 0.1 V steps verified feedback sign on both stationary electrical
axes. Initial 0.5 A current steps were slow. Tuning both gains gave:

- **Kp = 0.8 V/A; Ki = 2000 V/(A*s)** on both stationary axes.
- Approximately 0.35..0.40 ms 10-90% rise, versus roughly 8..10 ms previously.
- Feedback settled near 0.500 A with no voltage saturation at the 0.75 V ceiling.
- 400/600/800 Hz tones spanning 0..0.5 A passed. Sparse effective SISO estimates:
  open-loop crossover about 459 Hz (D)/520 Hz (Q), phase margin about 68/65 degrees.
- Closed response at 800 Hz: -2.15 dB (D)/-1.35 dB (Q). Exact closed-loop -3 dB
  bandwidth above 800 Hz is not measured. The excitation guard still caps at
  800 Hz; do not report crossover as closed-loop bandwidth.
- Cross-axis response at 800 Hz reached about 13%/8.5%; the sparse SISO estimate
  is not full MIMO robustness validation. Larger-current/rotor-angle validation
  remains outstanding.

The **older pre-integration image** keeps gains in RAM only. Its reset defaults
remain Kp0.2/Ki50. The tested settings are recorded in
`tools/tuning/current-loop-gains.json`; opening the MCUViewer project does not
automatically apply them. The integrated image adds checked flash persistence;
see `docs/LIVE_TUNING_FRONTEND.md` for the explicit save procedure and limitations.

Repeat on 2026-11-10 at 16:53 EST used the same pre-integration image through
MCUViewer, after the user confirmed the bench setup and physically armed USER.
Both bounded 0.5 A steps at Kp0.8/Ki2000 and the 0.75 V ceiling completed with
zero saturation samples. At 20 kHz, using the same three-sample crossing method,
10-90% rise was D0.50 ms/Q0.35 ms; final 10 ms means were D0.50012 A/Q0.49995 A.
The D repeat was slightly slower than the earlier 0.35..0.40 ms result. These
steps do not remeasure tone response or bandwidth. Raw captures are checked in
under `docs/bench/current-tuning/reproduce-20261110T215301Z-*.json`; results and
post-test diagnostics are in `docs/bench/2026-11-10-pi-tuning-repeat.json`.

The integrated `repo-current-tuning` image was subsequently flashed with EVM
power OFF after fresh user confirmation. STM32CubeProgrammer verified its HEX and
erased sectors 0/1 only. Read-only boot checks passed: self-test1, Recorder fault0,
maximum522cycles, no CPU fault, ENABLE/INLs/MOE LOW. Sector7 was empty, so boot
initially selected Kp0.2/Ki50. MCUViewer applied the previously measured
Kp0.8/Ki2000 with state0 and zero output, then explicit `nv_save_seq=1` returned
status2 (saved). The six-word flash record read back with the correct CRC and
commit word. A software reset with EVM power still OFF reloaded Kp0.8/Ki2000,
`nv_status=1`, state0, zero reference/voltage and LOW outputs. Full evidence and
image hashes are in `docs/bench/2026-11-10-nv-current-gains.json`. The exact
flashed ELF/HEX are copied to the ignored local archive
`outputs/checkpoints/nv-current-gains-2026-11-10`.
This validates **flash persistence and disabled-output boot only**. No powered
voltage/current experiment had run on the integrated image at that point.

Powered revalidation subsequently passed on the integrated image: +0.1 V
stationary D/Q steps produced +0.130/+0.133 A selected-current changes. At
0.5 A, conservative Kp0.2/Ki50 rise was D27.15/Q29.75 ms; NV Kp0.8/Ki2000
gave D0.45/Q0.35 ms with no saturation. A bounded rotating D-axis current run
at 3 A, 2.5 V ceiling, 20 s per leg commanded one mechanical revolution out
and back. It tracked 3.000 A mean with 0.017 A mean absolute error, 1.84 V peak
command and no saturation. The MCU finished both half moves, disarmed at the
41 s deadline (reason9), with Recorder fault0, IRQ max3267cycles, CPU fault0,
and ENABLE/INLs/MOE LOW. Scott observed “slightly better but not perfect”
shaft motion. Actual shaft position remains unmeasured; see
`docs/bench/2026-11-10-d-axis-reciprocal-3a.json`.

A subsequent 5 A D-axis run used a 3.2 V vector ceiling, 10 s per leg and a
21 s deadline. The selected current averaged 5.0007 A with 0.0179 A mean
absolute error, 3.068 V peak command and zero saturation samples. The MCU
completed two half moves and auto-disarmed with reason9; Recorder/CPU faults0,
IRQ max3283cycles, ENABLE/INLs/MOE LOW. Scott reported it was better and
smoother than the 3 A run. Shaft travel was not quantified or encoder measured.
See `docs/bench/2026-11-10-d-axis-reciprocal-5a.json`.

The first 6 A continuous run used 5 s per leg. Scott held USER after about
52 s and ten completed half moves; he reported that it worked well. Filtered
MCUViewer points gave 6.000 A mean, 0.0168 A mean absolute error, and zero
sampled saturation. Peak commanded voltage approached the 4 V ceiling.
Post-run: state0/reason1, Recorder fault0, IRQ max3298cycles, CPU fault0,
ENABLE/INLs/MOE LOW. See `docs/bench/2026-11-10-d-axis-reciprocal-6a.json`.
Actual shaft travel remains unmeasured.

Scott then requested twice the speed again, retaining 6 A and continuous USER
stop. The new image used 2.5 s per leg and ran about 46 s, completing 18
commanded half moves before USER stop. Scott said it was working and smooth. Selected
current averaged 6.0002 A with 0.0176 A mean absolute error. Eleven of 3781
downsampled selected points flagged vector saturation near the end; peak D
command reached 4.000 V. HOTPLUG post-stop: state0/reason1, Recorder fault0,
IRQ max3326cycles, CPU fault0, ENABLE/INLs/MOE LOW. Actual shaft travel and
speed were not measured by feedback. See
`docs/bench/2026-11-10-d-axis-reciprocal-6a-2x.json`.

## Hardware and safety state at handoff

The **6 A / 2.5 s-per-leg image** was flashed and verified. After fresh user
EVM-OFF confirmation, CubeProgrammer erased sectors0/1 only and reset; sector7
persisted. Last HOTPLUG boot check: fully disarmed state0/reason0, self-test1,
Recorder fault0/max522cycles, CPU fault0, Kp0.8/Ki2000 loaded from flash,
ENABLE/INLs/MOE LOW. Matching MCUViewer project was regenerated. Powering the
EVM apparently reset the MCU between the first disarmed config apply and USER
arm: the preflight found default step settings while armed at zero drive. A GUI
stop disarmed it, mode4/6 A/duration0/4 V was reapplied with EVM powered,
then Scott rearmed and the fast trial ran. The post-run state was disarmed
state0/reason1 with outputs LOW and no fault; MCUViewer acquisition is STOP.
Scott subsequently confirmed EVM power OFF. A text-only UART boot-message
correction was then flashed with sectors0/1 only and verified; HOTPLUG boot
again showed self-test1, NV 0.8/2000, Recorder fault0, CPU fault0 and
ENABLE/INLs/MOE LOW. This final exact image has not had a powered retest.

Get fresh EVM-power-OFF confirmation before any later programming or reset. USER
physically arms at zero drive; a GUI command starts each
experiment. Stop/fault requires USER rearming. Motor remains secured/unloaded,
Hall leads disconnected. No encoder is currently available. COM3 is the user's
console; do not seize it. Wiring details are in
`docs/STM32F411_DRV8353RS_INTEGRATION.md` and the bench guide.

ST-LINK SN: 066DFF514988525067173049. The exact powered 2.5 s image is archived in
`outputs/checkpoints/d-axis-reciprocal-6a-2x-continuous-2026-11-10`.
The current build and flashed image include the corrected UART boot message
and are archived in `outputs/checkpoints/d-axis-reciprocal-6a-2x-final-2026-11-10`
(HEX SHA256 `3cc3917434aa60384802a76706e406b7647559037a1666930921f2d4a19b2387`).
The 5 s/leg image and its capture are separately archived under
`outputs/checkpoints/d-axis-reciprocal-6a-continuous-2026-11-10` and
`outputs/tuning/d-axis-reciprocal-6a-20261110.json`. Preserve the V6 rollback
ELF/HEX in the original worktree archive.

## What the upstream merge preserved

`fw.elf` retains upstream's 24 V AKM/encoder defaults and newer application,
observer, trajectory, protocol, raw ADC acquisition and host tests.
`REPO_MOTOR_BENCH` preserves the standalone 12 V profile, target gain20, original
both-edge/IIR ADC behavior and sampling override ADC15cycles/CCR4=2489. The
original PWM logical B/C permutation remains; the tuning ADC order PC0/PC2/PC1
matches it. PC9 is wired to INLC and must not become an ISR timing marker in
bench builds. Required diagnostic/disable/fault/register APIs were retained.
See `docs/UPSTREAM_INTEGRATION.md` for resolution details.

## Software validation and reproduction

All 29 firmware targets built with Arm GNU13.2 on Windows. Upstream host suite:
23 tests/42 checks pass. Current-tuning/mailbox/actual vendor Recorder tests pass
under ASan/UBSan; bench/upstream configuration contracts pass. GitHub CI also
passed host suites and all firmware builds with Arm GNU15.2:
https://github.com/scottnortman/stm32-servo-drive/actions/runs/42906131378

For the NV change, upstream 23 tests/42 checks and current-tuning/Recorder/profile
tests passed under ASan/UBSan; new flash-journal tests covered interrupted writes,
corruption fallback and full storage. `repo-current-tuning.elf` and the separate
default `fw.elf` built with Arm GNU13.2. Firmware HEX ends at `0x08004fbb`, below
the reserved `0x08060000` sector. These software checks preceded and are
separate from the powered bench results above.

The rotating D-axis mode uses the saved 0.8/2000 gains and the one-turn cosine
trajectory. The 3 A/5 A profiles have 41 s/21 s deadlines. The prior 6 A
image used 5 s legs until USER. The newly flashed 6 A image uses 2.5 s legs
until physical USER, frontend stop or a protection trip. It uses a 4 V ceiling, 7.5 A
filtered/8 A raw per-phase trips and a 45 us IRQ timing stop. Mode 4 is applied
only while disarmed and requires a physical USER arm plus fresh GUI start.
SWD has no application heartbeat, so viewer disconnection does not stop the
continuous run. Upstream 23/42 and current-tuning/Recorder/profile host checks
pass under ASan/UBSan, including two simulated fast cycles; affected Arm GNU13.2
targets build. The faster image's disabled boot and powered trial passed within
the limits above. Image hashes and results are in
`docs/bench/2026-11-10-d-axis-reciprocal-6a-2x.json`.

Set `ARM_TOOLCHAIN_BIN_DIR` to the installed Arm GNU `bin` directory, then:

```sh
cmake -S . -B build-bringup
cmake --build build-bringup --parallel
sh test/run_tests.sh
bash tools/tuning/test-current-tuning.sh
python tools/tuning/analyze_pi_tuning.py
```

Use WSL/Linux for the host shell tests. Plot analysis needs numpy/matplotlib.
Raw reproducible captures are in `docs/bench/current-tuning`; generated outputs
go to ignored `outputs/tuning`. Firmware/code tests do not imply bench approval.

## Next work, in order

1. The requested 24 V D-axis motion repeat is complete and was observed smoother.
   Keep the archived tested image/capture and separate 12 V/stationary targets.
   Source/evidence checkpoint: `checkpoint/d-axis-reciprocal-24v-6a-2026-11-10`.
2. Continuous motion still touches the 4 V ceiling on some late trend samples.
   Validate temperature, current and angle dependence before extending operation;
   the low-current bandwidth measurement is not a high-current robustness proof.
3. Make long MCUViewer host monitors renew/reconnect MCP sessions on HTTP404;
   keep firmware USER/fault stops independent of host availability.
4. Obtain actual rotor feedback before velocity/position-loop development.
   Fresh EVM-OFF confirmation before any flash/reset; COM3 is the user's console.

## Local recovery notes

The ignored `CONTEXT.md` has current local session notes. The original folder's
`CONTEXT.md` contains longer commissioning history. Full source/firmware/capture
checkpoint archive is referenced by the original folder's
`outputs/checkpoints/latest-pi-tuning.json`; it also preserves the exact flashed
pre-integration tuning image and previous V6 motion image. The integrated NV
image is in this worktree's `outputs/checkpoints/nv-current-gains-2026-11-10`.
These local archives are not in Git.

MCUViewer's local MCP API is `http://localhost:8765/mcp`. On this PC IPv6 localhost
worked while127.0.0.1 timed out. `mcuviewer_project.connect()` clears old session
headers. Its `main()` recreates the GUI project: do not run it just to connect.
`run_current_experiment.py` requires physically armed idle, runs a one-second
test and leaves armed/idle between experiments; explicitly disarm at finish.
New live variables must be added to an active plot/table before relying on
`get_variable_value`: the recovery save view initially returned zeros for newly
added NV scalars until they were placed in the active table. Independent SWD
readback proved the save had already completed; do not repeat a save based only
on such unacquired values. Stop MCUViewer acquisition before CubeProgrammer uses
the same probe to avoid connection contention.
Avoid running build scripts that automatically flash unless programming is
authorized and power-off has been confirmed.
