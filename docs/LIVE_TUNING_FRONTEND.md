# MCUViewer FREE current-loop tuning

Integration note: the powered captures and tuned gains below were measured
before the latest upstream merge. The integrated tuning image is now flashed,
but its powered loop response has not been remeasured. See
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

Next bandwidth work needs a sweep extending above the desired crossover. The
initial excitation guard currently caps chirps/tones at 800 Hz; extend and verify
it before claiming a measured >=1 kHz bandwidth. Fixed-frequency tests can use
f_start=f_end and a separate 20 kHz capture per frequency, avoiding the 32 KiB
window limit without lowering the sampling rate.

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
- Matching flashed firmware on this PC: `build-bringup-arm/repo-current-tuning.elf`
  and `.hex` (HEX SHA256 `f00cd902bc01702359b97b69529b6cc0d82cac9af83cbcffea0960f47ee69823`).
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

## First powered run on the integrated image

1. The matching integrated image is already flashed. Check self-test=1 and
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
These are fixed electrical coordinates, not measured rotor-aligned torque axes.
Each test lasts 0.1..10 seconds and removes drive at its deadline, including if
the viewer disconnects.

State bits: 0=armed, 1=active. Result: 0 accepted, 1 invalid settings,
2 busy/incompatible active edit, 3 start rejected/unarmed. Rejected starts are
consumed and cannot replay after arming. Reasons: 1 USER, 2 nFAULT, 3 raw current,
4 filtered current, 5 ADC, 6 bus preflight, 7 config, 8 self-test,
9 frontend stop/Recorder guard.

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
