# MCUViewer FREE current-loop tuning

## Hall observation with 24 V D-axis motion (2026-11-11)

Current `repo-current-motion-24v.elf` uses interface0x1000E and enables
internal3.3 V pull-ups on PB6/PB7/PB8. The manual `hall-bringup.elf` diagnostic
also enables these pulls. Hall wiring and sensor5 V power must be checked
before connection; see `HALL_SENSOR_SS41F_REVIEW.md`. Do not use5 V signal
pull-ups with this configuration.

Interface E is the user-requested8 µs internal OCP deglitch experiment:
R5=`0x139`, first frame retained and two subsequent reads verified during zero-output arming and exposed through
`g_repo_ocp_control`. Other profiles retain2 µs. It does not filter nFAULT-line
noise; the prior already-HIGH IRQ policy is unchanged for this comparison.
Matching ELF is under `outputs/checkpoints/ocp-deglitch-8us-24v-5a-2026-11-11`.
See the latest handoff for current powered-test status; the completed ramp
below used interface B with2 µs OCP qualification.

Prepare the20-slot Hall view using
`python tools/tuning/mcuviewer_project.py --hall24 --elf <matching flashed ELF>`.
It saves `tools/tuning/current-loop-free-hall-24v.mcuv`, using
`g_tune_hall_state` and `g_tune_command_mech_rev` in its two observation slots.
Time traces show both signals, and an XY plot uses commanded revolutions on X
and Hall code on Y. Actual shaft angle is not measured independently. Hall
transition count is available through SWD. Voltage/saturation are in the live
controls table; the current Recorder keeps its original five signals.
State is the binary UVW code: PB6=U/4, PB7=V/2, PB8=W/1. For example state5
means U=HIGH,V=LOW,W=HIGH. The100 Hz plot is a trend, not an edge timestamp.

The observer samples at20 kHz even while disarmed and qualifies a state after
three consecutive equal samples (100..150 us delay). Boot-lifetime diagnostics
available through SWD include `g_tune_hall_raw`, `g_tune_hall_seen` (bitmask of
qualified codes), `g_tune_hall_invalid` (entries into000/111, including initial
state), `g_tune_hall_multibit` (qualified transitions changing multiple bits),
and `g_tune_hall_period_ticks` (50 us units between qualified transitions;
zero until the second transition). Hall values never steer or stop the drive.
The five-signal current Recorder and all motor guards retain their roles;
Hall sampling is included in the running45 us IRQ budget.

Interface E additionally permits5 A duration0 continuous scope motion at3.2 V.
1..4 A remain bounded only. Hold USER to stop; a viewer disconnect is not a stop.
Matching E image is in the8us archive's `continuous-5a` subdirectory. See the
latest handoff for live scope-session state.

### Completed 1–5 A ramp with fixed fault handler

All five bounded cycles passed at fixed24 V/3 A supply input and saved
Kp1.0/Ki2500, six seconds per cycle with2.5 s legs. Every level completed
two half moves and48 valid Hall changes; invalid/multibit counters stayed0.
User reported normal motion and confirmed EVM OFF; outputs verified LOW.
Viewer is STOP on `Hall and rotation capture`, a focused copy of the live
group containing commanded-rotation time, Hall-state time and XY plots.
The original controls and current Recorder groups remain available. The5 A
curves are retained in memory; restarting acquisition can replace them.
The project stores the view, while archived JSON/ZIP files preserve the data.

Interface B accepts disarmed mode4/axis0/integer1..5 A/duration6 with voltage
ceilings1/1.6/2.2/2.8/3.2 V, exact saved gains and fresh physical USER arming
for each cycle. Filtered trips1.25*amplitude+0.25 A, raw trips another0.5 A.
Already-inactive nFAULT edges are counted rather than treated as active faults;
active-LOW and independent level checks remain. Counts per1..5 A cycle were
0/38/27/1538/7300. Physical pulse origin still needs electrical diagnosis.
See `bench/2026-11-11-current-ramp-fault-fix.md`, matching JSON, comparison PNG
and raw ZIP under `bench/current-tuning/current-ramp-fixed-*`. Matching ELF:
`outputs/checkpoints/current-ramp-fault-fix-24v-2026-11-11/repo-current-motion-24v.elf`.

### Historical attempts before interface B

First verify outputs LOW, GPIO input modes and pull-ups, saved1.0/2500 gains,
then observe manual rotation while disarmed. With motor fully reassembled,
secured and unloaded, repeat the prior fixed24 V/2 A input-limit setup, apply
mode4/axis0/6 A/duration6/4 V ceiling after power settles while disarmed,
then obtain physical USER arming for one bounded six-second cycle. This
observation build is flashed and its zero-output boot/pull-up/NV checks passed.
Manual test saw all six codes,30 transitions, invalid0/multibit0. The powered
6 A attempt stopped on nFAULT/reason2 about1.515 s after start, adding six
valid transitions. It did not finish the bounded cycle; reversal is unverified.
Outputs were confirmed LOW and user switched EVM OFF. No retry/reset occurred;
investigate nFAULT before repeating. See `bench/2026-11-11-hall-observation-24v.json`.
The cleanup STOP changed the later displayed reason to9; do not mistake that
for successful bounded completion. Earlier
disconnected-Hall results below describe the previous interface0x10006 image.

The user-requested5 A retry requires interface0x10008. Apply mode4/axis0/5 A/
duration6/voltage limit3.2 V/Kp1/Ki2500 while disarmed. It has the same2.5 s legs
and rejects continuous5 A; filtered/raw trips6.5/7 A. Build/host tests and flashed
zero-output checks passed. At24 V/2 A input the5 A run stopped on nFAULT at
1.546 s/0.081064 commanded rev, before reversal. The captured XY plot is set
to commanded revolutions on X/Hall code on Y. The user requested a supply-limit
comparison; after24 V/3 A setup and fresh USER arm, the same5 A settings stopped
on nFAULT again at1.500 s/0.082528 commanded rev. Both selected-current means
were approximately5 A without sampled saturation, and each run added six valid
Hall transitions. The full cycle remains incomplete. Captures and report saved;
viewer STOP with the3 A-input Hall-vs-command curve visible, EVM confirmed OFF.
Increasing the supply limit did not prevent the fault; diagnose nFAULT before
further powered cycles. See the latest handoff and5 A comparison report.

## 24 V D-axis motion repeat

The previously validated new-gain motion repeat used `repo-current-motion-24v.elf`, interface0x10006,
with `python tools/tuning/mcuviewer_project.py --motion24 --elf <matching ELF>`.
GUI: `tools/tuning/current-loop-free-motion-24v.mcuv`. Do not use the stationary
24 V image for motion: its validator intentionally rejects mode4.

Apply mode4, axis0, amplitude6 A, Kp1.0/Ki2500, voltage limit4 V while disarmed.
Duration6 gives one firmware-bounded cycle; duration0 repeats until physical
USER/GUI stop/fault. Both use 1 s alignment then 2.5 s legs, one commanded turn
each way. Exact gains must have loaded from NV; power-up may reset RAM settings,
so apply after EVM power settles and before USER arming. The powered trial used
fixed24 V/2 A input limit, secured unloaded motor and disconnected Halls.

Both bounded and continuous trials passed within the documented limits. User
reported smoother motion; no encoder measured shaft travel/speed. Continuous
trace: about46.76 s/18 half moves, mean6.00027 A, three sampled saturation flags.
See `bench/2026-11-10-d-axis-reciprocal-24v.json`. Final EVM OFF, outputs LOW,
viewer STOP. The other 24 V sections below preserve the earlier low-current tune.

A long monitor hit MCP HTTP404 around32 s. `connect()` restored the session and
preserved live curves; do not call `main()` just to reconnect. Physical USER stop
remains necessary when the host cannot command stop; SWD loss is not a heartbeat.

## Current 24 V checkpoint (2026-11-10)

The dedicated 24 V image is flashed; powered low-current steps and tones are
complete. Use `tools/tuning/current-loop-free-24v.mcuv`, matching
`outputs/checkpoints/current-bw-24v-recovery-2026-11-10/repo-current-tuning-24v.elf`.
Kp1.0/Ki2500 are saved in sector 7 and reload after reset. The 0.5 A step rise
was D0.40/Q0.25 ms; unsaturated raw 20 kHz tones bracket closed-loop -3 dB
bandwidth at D800..1200/Q1200..1600 Hz, interpolated D1.048/Q1.226 kHz.
Evidence and limits: `bench/2026-11-10-current-bandwidth-24v-recovery.json`.

The 24 V image permits stationary tests only: 0.5 A reference, 0.1 V voltage
steps, 2 V vector ceiling, 2 kHz tones, 1/1.5 A filtered/raw phase trips and
22..26 V arming window. Interface version: 0x10005. Both images share the NV
journal; restore archived 0.8/2000 before using the older 12 V motion profile.
New settings are in `current-loop-24v-gains.json`; the original gain file is
preserved as 12 V history.

Final state: EVM confirmed OFF, disarmed, ENABLE/INLs/MOE LOW; viewer STOP on
live controls. SWD failed after the completed tones and recovered only after
Nucleo USB was power-cycled with EVM OFF. Cause is unresolved; pre-reset timing
and CPU registers were unavailable. Disabled-output boot and NV reload passed.
Fresh OFF confirmation is required before any later flash/reset.

Reproduce bandwidth without hardware (numpy/matplotlib required):

```sh
python -m zipfile -e docs/bench/current-tuning/24v-recovery-2026-11-10.zip outputs/tuning/24v-recovery
python tools/tuning/analyze_bandwidth_tones.py outputs/tuning/24v-recovery --output outputs/tuning/24v-bandwidth.json --plot outputs/tuning/24v-bandwidth.png
```

The sections below preserve earlier 12 V sessions unless explicitly stated.

Integration note: the first tuning captures below preceded the upstream merge.
The integrated image has since passed powered steps and 3 A/5 A rotating-current
trials. See
[integration notes](UPSTREAM_INTEGRATION.md) for the build-profile checks.

Scott selected MCUViewer with ST-LINK and Recorder, free edition: **20 live
variables, 5 Recorder variables, 5 trace channels**, evaluation only, no expiry,
community support. Trace is unused. No paid feature is required by this setup.

The confirmed V6 reciprocal image remains the motion rollback baseline. On the
earlier pre-integration tuning image, small voltage-step polarity checks and
0.5 A current steps were captured on both stationary axes. Live parameter
editing and five-signal Recorder worked. The integrated tuning image has since
passed disabled-output boot and NV-gain reload with EVM power OFF.

This establishes the tuning interface and initial low-gain tracking, not a
finished 1 kHz current loop. 100 Hz host polling is only for live trends/edits;
all step captures used the MCU Recorder at 20 kHz (50 us), giving 20 samples per
cycle at 1 kHz. Use raw samples for bandwidth/phase analysis. The 1.05 ms averaging
used in the initial step-report metrics must NOT be used for 1 kHz Bode analysis.

Initial comparison at Kp=0.2 V/A, Ki=50 -> 200 V/(A*s): D rise 34.5 -> 10.15 ms,
Q rise 30.9 -> 8.15 ms. Final 10 ms means at Ki=200 were D=0.5006 A, Q=0.4996 A,
with zero saturation samples. Raw noise/peaks and overshoot remain to assess at
higher bandwidth. On that earlier image, runtime Ki=200 was RAM-only and reset
restored Ki=50.
See `docs/bench/2026-11-10-current-step-results.json` and
`outputs/tuning/first-current-steps.png`; full raw JSON and CSV are in outputs/tuning.

The 12 V image caps chirps/tones at 800 Hz. The separate 24 V GEMS candidate
allows fixed tones through 2 kHz; its hardware measurements are pending. Use
f_start=f_end and a separate 20 kHz capture per frequency, avoiding the 32 KiB
window limit without lowering the sampling rate. Do not infer the -3 dB
bandwidth from the older 400/600/800 Hz captures.

## 24 V GEMS current-loop bandwidth candidate

Build `repo-current-tuning-24v.elf` and use `--bench24` with
`mcuviewer_project.py` after the matching image is flashed. Its interface
version is `0x00010005`; the 12 V image and GUI remain separate. The 24 V
firmware requires 22..26 V at USER arming, normalizes PWM to 24 V, limits
reference to 0.5 A and vector voltage to 2 V, and rejects rotating motion.
Filtered/raw phase trips are 1/1.5 A. Saved gains Kp0.8/Ki2000 load at boot,
but must be rechecked at 24 V before any bandwidth claim. The candidate and
software-only validation are recorded in
`docs/bench/2026-11-10-current-bandwidth-24v-candidate.json`.

After fresh EVM-OFF confirmation, flash, verify disabled-output boot and saved
gain reload, then use fixed 24 V with an initial 0.25 A **supply input** limit,
unloaded secured motor, and Halls disconnected. USER press/release arms at zero
output. Start with signed 0.1 V voltage steps on D/Q, followed by bounded
0.5 A D/Q current steps at a 0.75 V vector ceiling. The helper
`run_current_experiment.py --bench24` performs one-second tests and leaves
the MCU armed/idle. For frequency response, run separate unsaturated fixed
tones from low frequency through the -3 dB crossing, using the raw 20 kHz
Recorder samples. The analyzer `analyze_bandwidth_tones.py` fits each tone,
rejects saturated/poor fits for the crossing, and reports a bracketed
closed-loop -3 dB estimate. Explicitly disarm with USER or stop_seq afterward.

## Updated PI tune: Kp=0.8, Ki=2000

Repeat at 16:53 EST on 2026-11-10 confirmed both 0.5 A steps on the same
pre-integration image with Kp0.8/Ki2000 and the 0.75 V ceiling. Three-sample
10-90% rise estimates were D0.50 ms/Q0.35 ms, final means D0.50012 A/Q0.49995 A,
with no saturation or faults. See `bench/2026-11-10-pi-tuning-repeat.json` and its
raw capture paths. The motor was explicitly disarmed afterward (state0/reason9,
apply/start17, stop3), and ENABLE/INLs/MOE were verified LOW through read-only
HOTPLUG. Captures and the comparison plot were saved separately. This was the
last powered endstate before the integrated image was flashed.

The existing repo PI was tuned live from raw 20 kHz captures. Tested on both
stationary axes with 0.5 A steps and 400/600/800 Hz current references spanning
0..0.5 A. Voltage vector ceiling stays 0.75 V; protections and firmware unchanged.

- Recommended initial pair: **Kp=0.8 V/A; Ki=2000 V/(A*s)**.
- 10-90% rise about D0.40 ms/Q0.35 ms, versus roughly8-10 ms previously.
  Crossing estimates use3samples/150us averaging; raw samples remain available.
- Final10ms means D0.49971 A/Q0.49983 A in the first tuned captures. No saturation.
- Fixed-tone H=I/Iref, L=H/(1-H) matches the companion-repo analysis method.
  Log interpolation between400/600Hz gives effective SISO open-loop crossover
  about459Hz(D)/520Hz(Q), estimated phase margin67.8/65.4deg. These sparse-tone
  estimates assume approximately linear behavior; cross-axis response at800Hz
  reached13%(D) /8.5%(Q), so they are not full MIMO robustness certification.
- At800Hz closed-loop response was-2.15dB(D)/-1.35dB(Q); thus the tested response
  has not reached-3dB at800Hz. Exact1kHz bandwidth is still unmeasured because the
  flashed excitation guard caps frequencies at800Hz. Crossover and closed-loop
  bandwidth are different quantities. Do not label459/520Hz as closed-loop BW.

A simple first-order-plus-delay fit of the old0.1V raw voltage steps gave
R_effective0.73..0.80ohm, L_effective0.24..0.33mH, delay50..87us. These include
sampling/scaling/bridge effects and are starting design estimates, not separately
measured motor constants. PI zero placement follows the usual motor RL approach:
[TI current-PI derivation](https://www.ti.com/document-viewer/lit/html/SSZTCG4/GUID-4EBDE243-F8EA-41D6-8930-1E6BFACE8DEE).

Saved profile: tools/tuning/current-loop-gains.json. Those powered results used
RAM-only gains on the earlier image. The integrated image now loads the last
saved pair from flash at boot. Loading a .mcuv project does not change the
applied gains; to edit them, set requests and increment apply_seq as usual.

Evidence: docs/bench/2026-11-10-pi-tuning-results.json and pi-tuning-post.json;
raw captures outputs/tuning/tune-*.json and tone-*.json. Comparison graphic:
outputs/tuning/pi-tuned-response.png. Analysis: python tools/tuning/analyze_pi_tuning.py.
Host helper run_current_experiment.py requires a physically armed idle motor,
limits requests to0.5A/Kp2/Ki6000, and checks post-test status. It never arms,
flashes or resets the MCU. It leaves the motor armed/idle between experiments;
explicit stop_seq/USER is required to disarm when finished.

Historical endstate: final repeat D-step left visible in MCUViewer; stop_seq2, state0,
reason9(intentional host stop), result0. ENABLE/INLs/MOE verified LOW by read-only
HOTPLUG. No Recorder/CPU fault. Maximum Recorder545cycles/5.45us; full control IRQ
with tones2571cycles/25.71us within50us period. The integrated image was later
flashed with EVM power OFF; get fresh OFF confirmation before another flash or
reset.

## Open, build and capture

- Project: `tools/tuning/current-loop-free.mcuv`.
- The flashed 6 A/2.5 s-per-leg final image is archived at
  `outputs/checkpoints/d-axis-reciprocal-6a-2x-final-2026-11-10` (HEX SHA256
  `3cc3917434aa60384802a76706e406b7647559037a1666930921f2d4a19b2387`).
  The `build-bringup-arm` ELF/HEX match it. The powered trial used the
  preceding image, archived separately under `d-axis-reciprocal-6a-2x-continuous-2026-11-10`;
  the final image changed only UART boot text and passed disabled-output boot.
- Build: `cmake --build build-bringup-arm --target repo-current-tuning.elf`.
- Software tests (WSL): `bash tools/tuning/test-current-tuning.sh`.
- Optional project regeneration: `python tools/tuning/mcuviewer_project.py` with
  MCUViewer open. Save other projects first: this deliberately creates a new
  project. It never flashes, acquires, or writes MCU RAM. Uses official localhost
  MCP API; IPv6 localhost works on this PC, while 127.0.0.1 did not.

ST-LINK SN 066DFF514988525067173049, SWD 1000 kHz. Live view requests 100 Hz
for trends and editing. Recorder samples synchronously at **20 kHz / 50 us**.
32 KiB holds 1638 five-signal samples, about **81.9 ms**. Default rising trigger
is 0.25 with 200 pretrigger samples (10 ms). Downsampling extends the window at
lower time resolution. Completed buffers transfer to the PC; this is repeated
triggered capture, not continuous 20 kHz streaming to the screen. Export CSV in
MCUViewer for subsequent analysis.

## Free-edition budget

| Purpose | Variables (all have `g_tune_` prefix) | Count |
|---|---|---:|
| Editable requests | mode, axis, amplitude, kp, ki, f_start, f_end, duration, voltage_limit | 9 |
| Commands | apply_seq, start_seq, stop_seq | 3 |
| Signals, also used by Recorder | reference, measured, cross_current, voltage, saturated | 5 |
| Status | state, reason, result | 3 |
| **Total live variables** | | **20** |

Recorder uses exactly those five signals. Timing diagnostics stay out of the
default project; temporarily replace a live variable when needed. The same five
slots can serve later loops. Velocity/position tuning requires rotor feedback,
which is currently unavailable.

## Saving current PI gains in MCU flash

The updated `repo-current-tuning.elf` reserves STM32F411 sector 7
(`0x08060000..0x0807ffff`) for an append-only gain record. At boot it loads the
last committed, checksum-checked Kp/Ki pair into the same RAM requests and repo PI
controller. Empty or damaged storage uses Kp0.2/Ki50; mode, amplitude, duration,
and the 0.75 V ceiling keep their existing defaults. Boot remains disarmed.
The integrated image was flashed with EVM power OFF, and Kp0.8/Ki2000 were
saved and reloaded across a software reset. The exact image, record, and
disabled-output checks are in `bench/2026-11-10-nv-current-gains.json`.
Powered current-loop behavior on this image still needs verification.

MCUViewer FREE has 20 live slots. After flashing the matching updated image and
refreshing its symbol addresses, run
`python tools/tuning/mcuviewer_project.py --nv --elf build-bringup-arm/repo-current-tuning.elf`
to create `tools/tuning/current-loop-free-nv.mcuv`. This view replaces only the
two chirp-frequency requests with `g_tune_nv_save_seq` and `g_tune_nv_status`.
The standard view remains available for 400/600/800 Hz tones. Project generation
creates a new project; save other GUI work first. Opening either project does not
change gains already loaded at boot.

To save a validated pair, apply it through the usual `g_tune_kp`, `g_tune_ki`,
`g_tune_apply_seq` sequence. Run and evaluate bounded steps, then disarm with
USER or `g_tune_stop_seq`. Confirm state0 and ENABLE/INLs/MOE LOW. Increment
`g_tune_nv_save_seq` once. The firmware consumes each sequence value once and
accepts a save only while disarmed, inactive, with low outputs and no pending
unapplied gain edit. The saved values are the **applied** gains. Reads on later
boots need no GUI connection. `g_tune_nv_status`: 0 empty/default, 1 loaded,
2 saved, 3 rejected, 4 journal full, 5 flash write/verify error, 6 corrupt
record without a valid predecessor. A failed or interrupted append leaves the
previous committed pair available. The journal does not auto-erase when full.

Normal image flashing must erase only sectors used by the image. **Mass erase
will erase saved gains**; keep the JSON gain profile as a recovery copy.
Flash write and boot reload passed with EVM power OFF. Any later flash/reset
needs fresh EVM-power-OFF confirmation. The sector layout
and 32-bit programming mode follow the
[STM32F411 reference manual](https://www.st.com/resource/en/reference_manual/dm00119316.pdf).

## D-axis reciprocal trials

`repo-current-tuning` mode 4 commands a rotating **D-axis current** vector
using the saved Kp0.8/Ki2000 PI gains. The electrical field angle follows the
earlier cosine trajectory: 1 s at the initial angle for alignment, then one
commanded mechanical revolution out and back. Sine PWM comes from the existing
`pwm_apply_dq` path. The current image accepts the 3 A, 5 A and fast 6 A
profiles; the slower 6 A profile belongs to the archived prior image:

| Profile | Current | Vector ceiling | Time per leg | Stop | Filter/raw phase trips |
|---|---:|---:|---:|---:|---:|
| Verified 3 A | 3 A | 2.5 V | 20 s | 41 s | 3.75 / 4 A |
| Verified 5 A | 5 A | 3.2 V | 10 s | 21 s deadline | 6.5 / 7 A |
| Archived 6 A image, verified | 6 A | 4 V | 5 s | USER, stop command or fault | 7.5 / 8 A |
| Flashed 6 A image | 6 A | 4 V | 2.5 s | USER, stop command or fault | 7.5 / 8 A |

The 3 A and 5 A profiles coast and disarm at their deadlines. The 6 A profiles
repeats until stopped; SWD has no application heartbeat, so a disconnected
viewer does not stop it. USER, nFAULT, ADC freshness, bus
preflight and 45 us control-IRQ timing stop remain. Mode 4 requires the exact
0.8/2000 pair loaded from NV flash at boot, a config apply while disarmed,
physical USER arming and a fresh GUI start sequence.

The prior 2 A voltage-driven image commanded a full revolution per leg; Scott
observed roughly ±0.5 shaft revolution with uneven motion. This new test keeps
the same commanded trajectory but regulates D current. On the 3 A run, the
selected-current mean during motion was 3.000 A, mean absolute error 0.017 A,
peak commanded voltage 1.84 V and no saturation. Firmware completed two half
moves, disarmed without fault, and measured 3267 maximum IRQ cycles. Scott
reported motion was “slightly better but not perfect.” Evidence is in
`bench/2026-11-10-d-axis-reciprocal-3a.json`; full curves and the exact image
are archived locally under `outputs/checkpoints/d-axis-reciprocal-3a-2026-11-10`.
The 5 A run completed both legs and auto-disarmed at 21 s. During motion its
selected current averaged 5.001 A with 0.018 A mean absolute error, 3.068 V peak
command and no saturation; maximum IRQ time was 3283 cycles and all outputs
were LOW afterward. Scott observed better, smoother shaft motion than at 3 A;
travel was not quantified. See `bench/2026-11-10-d-axis-reciprocal-5a.json`.
The first 6 A run completed ten half moves over about 52 s before Scott held
USER; he reported that motion worked well. On 5000 downsampled curve points,
current averaged 6.000 A with 0.017 A mean absolute error, sampled saturation
was zero and peak commanded voltage approached 4 V. See
`bench/2026-11-10-d-axis-reciprocal-6a.json`.
The 2.5 s-per-leg run completed 18 commanded half moves in about 46 s before
USER stopped it; Scott said it was working and smooth. Current averaged 6.000 A with
0.018 A mean absolute error. Eleven of 3781 selected downsampled curve points
flagged vector saturation near the end, and peak commanded D voltage reached
4.000 V. The post-stop fault and output checks passed; see
`bench/2026-11-10-d-axis-reciprocal-6a-2x.json`.
With no rotor sensor, its
`g_tune_command_mech_rev` is a **command**, not measured shaft position. Smoothness
and shaft travel require direct observation; current tracking and saturation can
be captured electronically. These are distinct measures of performance.

After flashing the matching 6 A-capable image, create the 20-slot GUI view with
`python tools/tuning/mcuviewer_project.py --motion --elf build-bringup-arm/repo-current-tuning.elf`.
It swaps the two chirp frequency requests for commanded mechanical revolutions
and completed half moves. For the continuous trial, apply mode4, axis0,
amplitude6 A, duration0 s, voltage limit4 V and the boot-loaded gains
**while disarmed**. Secure the unloaded motor with Halls disconnected; use fixed
12 V and the 3.5 A supply input limit used for the prior 6.25 A V6 trial.
Press/release USER to arm at zero drive, start a live capture, then increment
`g_tune_start_seq`. The current image uses 2.5 s per leg; the archived previous
6 A image used 5 s per leg with the same runtime settings. USER held for 20 ms
stops and disarms the drive. The GUI
stop sequence also disarms. Confirm state0 and LOW outputs after USER stops it,
save the trace, and record observed shaft travel/smoothness separately.

## First powered run on the integrated image

1. With a matching integrated image flashed, check self-test=1 and
   ENABLE, INLs and MOE LOW; inspect `g_tune_recorder_max_cycles` and
   `g_tune_recorder_fault` read-only before energizing. If replacing the image,
   stop motion with USER and get fresh EVM-power-OFF confirmation before flashing
   or resetting; keep USB connected and recheck disabled-output boot afterward.
2. Options -> Acquisition: enable/detect Recorder. Expected: 50,000 ns,
   32,768 bytes, five variables, version 8.3. Never use these RAM addresses with
   V6 firmware. New firmware starts disabled and waits for USER.
3. Secure the unloaded motor, Halls disconnected. Use fixed 12 V and 0.25 A
   **supply input** limit for the first 0.5 A test. Phase current differs from
   supply current. USER press/release calibrates and arms at ZERO output.
4. First verify feedback sign with mode=0 (voltage step), axis=0, amplitude=0.1 V,
   duration=1 s, voltage_limit=0.75 V. Increment apply_seq, confirm result=0.
   Start Recorder with rising reference trigger at 0.05 V, then increment
   start_seq. Selected current should respond positively; inspect cross current.
   Repeat axis=1. Resolve inconsistent signs/coupling before current control.
5. For the first powered test on the integrated image, explicitly set and apply
   mode=1, axis=0, amplitude=0.5 A, kp=0.2 V/A, ki=50 V/(A*s), duration=1 s,
   voltage_limit=0.75 V; boot currently loads the saved Kp0.8/Ki2000 pair.
   Restore the trigger to 0.25 A, start capture, increment start_seq. These
   conservative starting gains precede revalidation on the integrated image.
   Step rises at 20% of test duration, returns to zero at
   70%, and coasts at the deadline. Another test can reuse the armed state.
   Compare rise, settling, overshoot, cross-axis current and saturation.
6. USER hold 20 ms or stop_seq fully disarms. Faults also disarm. A fresh
   physical USER press/release is required before another GUI start.

Edit requests first, increment **apply_seq last**. The firmware validates the
whole configuration. Editing a request alone does not change active gains.
During a test only Kp/Ki may be committed; other edits are rejected. Integrators
are retained, although changing Kp can immediately change voltage. Change one
gain between captures for comparable results.

Modes: 0 voltage step, 1 current step, 2 current chirp, 3 voltage chirp. Axis:
0 stationary-frame D, 1 stationary-frame Q. Reference is A in current modes, V
in voltage modes; measured/cross current are A, voltage is V, saturation is 0/1.
Modes 0..3 use fixed electrical coordinates. Mode 4 uses a commanded rotating
electrical frame; none is a measured rotor-aligned torque axis.
Standard step/tone tests last 0.1..10 seconds; mode 4 lasts 41 or 21 seconds
for the bounded profiles. The 6 A profile uses duration0 to repeat continuously
until USER, stop command or a fault. A viewer disconnect alone cannot stop it.

State bits: 0=armed, 1=active. Result: 0 accepted, 1 invalid settings,
2 busy/incompatible active edit, 3 start rejected/unarmed. Rejected starts are
consumed and cannot replay after arming. Reasons: 1 USER, 2 nFAULT, 3 raw current,
4 filtered current, 5 ADC, 6 bus preflight, 7 config, 8 self-test,
9 frontend stop/Recorder guard, 10 motion IRQ timing stop.

## Reuse and verification limits

The adapter calls the actual repo `pi_init/pi_step`, `current_feedback_get_dq`
and `pwm_apply_dq`. Original `control.c`, `loops.c`, `pwm.c`, `current_feedback.c`
and sysid sources are not rewritten. Signed d/q PI calls follow
`src/sysid/foc_sysid.c`; the adapter supplies excitation and parameter/capture
interfaces.

Original PWM swaps logical B/C to physical C/B. This new target alone orders
injected ADC PC0/PC2/PC1 to match, before the existing transforms. Boot checks
the sequence. Signed polarity still needs the small voltage-step bench check;
V6's open-loop magnitude test did not prove it.

Baseline gain20/scaling, ADC15cycles/CCR4=2489, 7.5 A filtered/8 A sampled raw
trips, USER/nFAULT/ADC guards are retained. Bus voltage is checked before arming;
modulation assumes fixed 12 V during the test. No SPI during energized tests.
USB serial remains the 115200 console. The earlier USB transport prototype
`src/repo_tune_link.c` is not linked into this target.

Vendor Recorder requires no preemption during recorderStep. A short critical
section validates <=5 RAM-only 4-byte sources, records and measures DWT cycles.
An 800-cycle (8 us at 100 MHz) violation inhibits captures and disarms through
the usual guard. Boot measures the five-signal/float-trigger path at zero drive
before allowing USER arming. Software nFAULT service can be deferred by that
short section; IRQ priorities and internal DRV protection remain unchanged.
Measured Recorder maximum was 545 cycles (5.45 us) across the powered tests;
maximum complete control IRQ was 2058 cycles (20.58 us), below the 50 us period.
Boot full capture test initially exceeded the 8 us budget at -Og and correctly
blocked arming. Only the new adapter is now compiled at -O2; original motor
modules retain their existing options. Boot passed at 522 cycles (5.22 us).

Software tests cover actual repo PI against an independent RL plant, both axes,
limits/NaN/saturation/chirp completion, command replay/stop priority, boot Recorder
path and real five-signal buffer copying. They do not prove motor stability.
Build retains a vendor signedness warning and a pre-existing unused static ADC
helper declaration.

Existing companion-repo Bode analysis is preserved in
`tools/tuning/upstream/closed_current_bode_plot.py`, blob
`e33a81ddccc4f42a5b6724cbee13fc00d08f89c0`. Its AKM constants are not GEMS gains.
Adapt exported CSV after polarity and step tests; analysis needs suitable
excitation and unsaturated data.

Official references: [Recorder](https://docs.mcuviewer.com/recorder/),
[Variable Viewer](https://docs.mcuviewer.com/variableviewer/),
[local MCP API](https://docs.mcuviewer.com/mcpserver/),
[free limits](https://mcuviewer.com/), [repo sysid](https://github.com/jnzim/foc-sysid).

Vendor files in `third_party/mcuviewer_recorder` are from official 1.4.0.
The .c/.h are unchanged; defines set 50,000 ns and five variables. Copyright and
license notices are preserved; vendor code is licensed for use with MCUViewer.
