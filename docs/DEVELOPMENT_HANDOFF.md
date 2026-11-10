# Development handoff

Last engineering state: 2026-11-10. This is a curated project handoff, not the
full chat transcript. Verify live hardware and Git state before acting.

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
commanded half moves before USER stop. Scott said it was working. Selected
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
then Scott rearmed and the fast trial ran. The latest observed post-run state
is disarmed state0/reason1 with outputs LOW and no fault; MCUViewer acquisition
is STOP. EVM power was last used at 12 V/3.5 A input limit; its present power
state is not yet confirmed.

Get fresh EVM-power-OFF confirmation before any later programming or reset. USER
physically arms at zero drive; a GUI command starts each
experiment. Stop/fault requires USER rearming. Motor remains secured/unloaded,
Hall leads disconnected. No encoder is currently available. COM3 is the user's
console; do not seize it. Wiring details are in
`docs/STM32F411_DRV8353RS_INTEGRATION.md` and the bench guide.

ST-LINK SN: 066DFF514988525067173049. The build path
The exact powered 2.5 s image is archived in
`outputs/checkpoints/d-axis-reciprocal-6a-2x-continuous-2026-11-10`.
The current build also corrects a UART boot message that otherwise implied
all tests have a deadline; this text-only update has not been flashed yet.
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

1. Record the user's final qualitative smoothness/travel observation if
   available; the 6 A/2.5 s trial electrically tracked but reached the 4 V
   ceiling on some late samples. Do not infer actual rotor trajectory from
   the commanded angle. Obtain fresh EVM-OFF confirmation before any later
   flash or reset. If the UART text-only image is flashed, verify disabled
   output/NV load; the powered results above belong to the archived image.
2. Extend/verify excitation beyond800Hz for full bandwidth measurements using
   raw20kHz data. Avoid the earlier1ms plotting average for1kHz phase analysis.
3. Validate higher currents and cross coupling/angle dependence. Actual rotor
   feedback is needed before developing and tuning velocity/position loops.

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
Avoid running build scripts that automatically flash unless programming is
authorized and power-off has been confirmed.
