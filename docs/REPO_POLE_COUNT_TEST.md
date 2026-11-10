# Ten-electrical-turn pole-count observation

Scott requested ten slow electrical cycles so he can count shaft revolutions.
This target deliberately measures commanded field turns; it does not measure
rotor position. A jump, reversal, stall or missed turn invalidates a pole-count
inference from this test. The previous run moved partway and jumped back, so
its field count cannot establish the pole count.

For a rotor that follows the field, pole pairs = electrical turns / shaft turns.
For ten electrical turns: **poles = 20 / absolute shaft revolutions**.
Examples:2.5shaftturns->8poles;3and1/3->6poles;5->4poles.
The electrical/mechanical relationship is described by
[Microchip's motor-control documentation](https://developerhelp.microchip.com/xwiki/bin/view/applications/motors/control-algorithms/bldc/sensors/).

## Behavior

Separate target `repo-pole-count.elf/.hex`, profile `REPO_COUNT_POLES`.
One-second alignment/current ramp, one-second speed ramp, then nominal0.1Hz
electrical (one electrical revolution every10seconds). Count exactly ten wraps
of the real repo `open_loop_step` electrical angle. Total observation is about
101–102seconds including startup; floating-point/timer rounding changes timing
slightly but not the commanded number of wraps. The last step ends at angle0.

After turn10, hold the final stator field until USER or a protection stops drive.
This is a field hold, not measured closed-loop shaft-position hold. It prevents
a programmed coast at the observation endpoint. This counted test is Scott's
explicit exception to the earlier continuous-motion request; the existing
continuous target is preserved unchanged.

Use a visible shaft mark attached with power off. Ignore the initial alignment
movement; count from the aligned starting position through `COUNT COMPLETE`.
Report total shaft revolutions including the final fraction, direction, and
whether it moved monotonically without jumps. Do not touch a powered shaft.

## Reuse and limits

Actual `loops.c`, `pwm.c`, `current_feedback.c`, `control.c` and working driver
remain unmodified. `repo_pole_count.c` is a small angle-wrap counter around
the existing helper, not another waveform or current controller. The field
frequency uses no assumed pole count. Nominal700mA magnitude limit,550mV phase
voltage ceiling,1A filtered/2A sampled raw trips, USER,nFAULT and2ms current-ADC
freshness checks remain unchanged.

Assumption for this test: bench supply remains fixed at12V/0.25A. Bus voltage is
measured and validated before motor drive. For that preflight, injected triggers
and JEOC interrupts pause while all outputs remain disabled; the original
calibration routine subsequently restores the injected acquisition path.
There are no added regular bus/VREF conversions during drive. The original
current ADC path has exclusive acquisition ownership, as in the reference motor
firmware. RUN output labels the cached result `VBUS_preflight_mV`.
There is no runtime software bus-voltage supervisor in this counted profile;
hardware driver faults/current/USER protection remain active.

This also avoids the unresolved runtime bus-monitor stop in the prior run:
reason6 occurred at12901mV, inside10..14V, but the code also checked inferred
VDDA and regular-conversion completion. The exact failed sub-check was not
retained. It is not evidence of a motor overcurrent or a measured rotor error.
No voltage/current limits were raised for this pole-count test.

## Validation and use

Host ASan/UBSan test compiles the actual existing `src/loops.c` with the counter
and a hardware-output sink. It verifies ten wraps in1,997,080constant-speed
samples, continuous angle increments, final angle0 and continued field hold
without an eleventh wrap. This does not verify rotor following.

MCU build passed with the existing unused-static-declaration warning in the
unchanged current feedback header. ELF verification confirms the counter calls
the original helper/PWM chain and the protection adapter routes to the original
ADC ISR. The continuous repo-motor v2 HEX is unchanged.

Program with EVM12V OFF and USB connected; motor may remain connected, secured,
unloaded and with Halls disconnected. After boot PASS, apply12V/0.25A and press/
release USER. Watch the mark after alignment for the complete count. USER held
20ms disables drive, including after final-field hold.

HEX SHA256:`5F1E276EE439F05EBD0055913F3954E25012BE119436913DE7C2292C832234A3`.
Boot self-test0x200000A8; timer events0x2000009C; software fault IRQ0x20000098;
electrical turns0x20000068. Build/test/link evidence under
`bench/2026-11-10-repo-pole-count-v1-*`.
After Scott confirmed EVM12V OFF, programming/readback verification/reset
succeeded. HOTPLUG verified boot self-test1, software fault IRQ1, electrical
turns0, control ticks0, PA4 analog and ENABLE/INLs/MOE LOW. Evidence: matching
`bench/2026-11-10-repo-pole-count-v1-flash.txt` and `-self-test-swd.txt`.
Actual shaft-turn observation and pole-count inference remain pending.
