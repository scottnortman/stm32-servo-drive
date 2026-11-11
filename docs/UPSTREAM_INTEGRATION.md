# Upstream integration, 2026-11-10

## Branches and ownership

- `origin`: https://github.com/scottnortman/stm32-servo-drive.git
- `upstream`: https://github.com/jnzim/stm32-servo-drive.git
- `scott/nucleo-current-tuning`: committed commissioning/tuning baseline, `836b762`.
- `bench/current-pi-2026-11-10`: tag at that baseline.
- `scott/upstream-integration`: merges upstream `eda7136` (179 newer commits).

The baseline checkout is kept separately. Integration was performed in a Git
worktree; no reset, rebase, force push or history rewrite was used. The local
checkpoint archive also preserves the exact previously flashed HEX/ELF and V6
motion image. The gains in `tools/tuning/current-loop-gains.json` and captures in
`docs/bench/current-tuning` refer to that **pre-integration** image.

## Conflict resolution and compatibility

Three textual conflicts occurred: `.gitignore`, `Include/config.h`, and
`src/drv8353.c`. Semantic review also found removed bench APIs and changed ADC
and voltage defaults that Git merged without a conflict.

| Area | Resolution |
|---|---|
| Application | Keep upstream trajectory, observer, protocol, tests and SVPWM helper changes. No SVPWM behavior is enabled beyond upstream's own use. |
| Configuration | Keep upstream AKM/encoder defaults and gains. `REPO_MOTOR_BENCH` selects fixed 12 V unless the dedicated GEMS current-tuning target also sets `REPO_TUNING_24V`; gain20 remains a target override. Default `fw.elf` stays 24 V, three pole pairs, gain40. |
| PWM and ADC | Default application keeps upstream single falling-edge, raw ADC acquisition and centered sample offset. Bench builds retain original both-edge/IIR acquisition and offset; current/V6 board entry still selects ADC15cycles and CCR4=2489. |
| Timing marker | Bench ADC keeps PC4 marker and its own DWT timing. It does not initialize upstream's PC9 ISR marker because PC9 is wired to INLC. |
| DRV driver | Preserve the verified EVM pull-up/reset and SPI adaptations, plus public disable/fault/register and diagnostic APIs still used by commissioning targets. Upstream had removed these as unused by its application. |
| Open-loop adapter | Restore the original small `open_loop_step` under `REPO_MOTOR_BENCH` only. Upstream velocity/trajectory behavior remains in the common module. |
| Legacy delay | Retain `delay_ms` for the communication diagnostic that still calls it; unused sections are discarded from other images. |
| Scripts | Use LF shell files and the correct `Include` directory case for Linux. |

This is one shared source tree with explicit build profiles, not a second copy
of the controller. The bench current PI still calls `src/control.c`.

The later `repo-current-tuning-24v.elf` target uses the standalone GEMS bench
modules and 24 V bus normalization. It reserves the same NV sector 7 as the
12 V tuning target, but has a separate MCUViewer interface version and limits
tests to stationary 0.5 A current references, 0.1 V voltage steps and a 2 V
vector command. The existing `repo-current-tuning.elf` remains the 12 V motion
and tuning profile. The 24 V target was software validated; hardware results
are tracked separately in `bench/2026-11-10-current-bandwidth-24v-candidate.json`.

The separate `repo-current-motion-24v.elf` adds `REPO_MOTION_24V` on the same
24 V bus/bench modules. It requires persisted Kp1.0/Ki2500 for 6 A mode4,
with a 4 V ceiling, 2.5 s legs and either a 6 s bounded check or continuous
USER stop. Stationary limits remain unchanged; the original stationary24 V
and12 V images retain their validators and gain gates. Interface0x10006 and a
separate `--motion24` GUI keep image/profile selection explicit. Bench results
are in `bench/2026-11-10-d-axis-reciprocal-24v.json`.

## Validation

- All **29 firmware images** build with Arm GNU 13.2.1 on Windows.
- Upstream host suite: **23 tests / 42 checks pass**.
- Current tuning and actual vendor Recorder/mailbox suite: **ASan/UBSan pass**.
- Both bench and upstream configuration contracts pass, including bus voltage,
  gain/scaling, timing and trip thresholds.
- Preprocessed ADC paths checked for both initialization and calibration edge
  selection, preserved bench IIR, and isolation of ISR timing instrumentation.
- Checked-in raw captures reproduce both PI comparison plots from a fresh worktree.
- CI runs host suites and firmware builds on `jz-dev`, `scott/**`, and pull requests.

Build/check commands (no hardware access):

```sh
cmake -S . -B build-bringup
cmake --build build-bringup --parallel
sh test/run_tests.sh
bash tools/tuning/test-current-tuning.sh
python tools/tuning/analyze_pi_tuning.py
```

Set `ARM_TOOLCHAIN_BIN_DIR` to the Arm GNU `bin` directory before configuring.
Machine-readable validation/image hashes are in
`docs/bench/2026-11-10-upstream-integration.json`.

The integrated `repo-current-tuning` firmware has now been flashed, with fresh
user EVM-power-OFF confirmation. Its disabled-output boot and explicit NV gain
save/reload passed with the EVM OFF; powered current-loop tests remain pending.
See `bench/2026-11-10-nv-current-gains.json`. Fresh EVM-power-OFF confirmation
is required before any later programming/reset. The default upstream `fw.elf` is
an AKM/encoder application and is not the standalone GEMS commissioning image.
Next bench verification should begin with small signed-response/current steps.
The older pre-integration image has RAM-only gains. The flashed integrated
`repo-current-tuning` image loaded an explicitly saved Kp0.8/Ki2000 pair from
reserved flash sector 7 after reset, with Kp0.2/Ki50 as the empty/corrupt
fallback. See `LIVE_TUNING_FRONTEND.md`; powered loop behavior remains untested.

## Subsequent upstream updates

Commit local work first. Fetch upstream, create a new integration branch from the
current development branch, merge `upstream/jz-dev`, resolve with these profiles
in mind, and run the checks above. Push the resulting branch to `origin`; keep a
tag for each hardware-verified baseline. Do not merge upstream directly into a
dirty bench checkout or force-push over the baseline tag.
