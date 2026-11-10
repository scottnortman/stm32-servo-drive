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
voltage/current experiment has run on the integrated image.

## Hardware and safety state at handoff

The integrated tuning image is now on the MCU. Last observed after the NV-save
reset: fully disarmed, ENABLE/INLs/MOE LOW, self-test1, Recorder/CPU faults0,
Kp0.8/Ki2000 loaded from flash, `nv_status=1`, apply/start/stop/save sequences0.
MCUViewer is stopped on the matching integrated project. The EVM was user-confirmed
OFF before programming and remained OFF through this verification. These describe
the last observation, not a guarantee of the current hardware state.

Get fresh EVM-power-OFF confirmation before any later programming or reset. USER
physically arms at zero drive; a GUI command starts each bounded
experiment. Stop/fault requires USER rearming. Motor remains secured/unloaded,
Hall leads disconnected. No encoder is currently available. COM3 is the user's
console; do not seize it. Wiring details are in
`docs/STM32F411_DRV8353RS_INTEGRATION.md` and the bench guide.

ST-LINK SN: 066DFF514988525067173049. MCUViewer now uses the matching ELF at
`build-bringup-arm/repo-current-tuning.elf`. Both the standard 20-variable project
and the 20-variable NV-save view were regenerated with its addresses. Preserve
the exact rollback baseline ELF/HEX in the original worktree archive.

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
the reserved `0x08060000` sector. These software checks are separate from the
powered bench work still pending.

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

1. With EVM 12 V back under the user's control and the motor secured, verify
   small signed voltage steps on both axes, then bounded 0.5 A current steps
   on the integrated image. Before the first powered check, explicitly apply
   conservative Kp0.2/Ki50 in RAM; flash will otherwise load Kp0.8/Ki2000.
2. Recheck Kp0.8/Ki2000 on the integrated image after polarity/current guards
   pass. The flash journal's empty, save and reload paths are verified; corruption
   fallback is host-tested only. Preserve rollback baseline and exact new image.
3. Extend/verify excitation beyond800Hz for full bandwidth measurements using
   raw20kHz data. Avoid the earlier1ms plotting average for1kHz phase analysis.
4. Validate higher currents and cross coupling/angle dependence. Actual rotor
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
