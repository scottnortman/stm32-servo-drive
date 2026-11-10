# Smooth continuous current-field trial v2

The v1 continuous image ran for 2,891,733 ms (48.20 minutes), advanced the field
11,565 times, and stopped on USER with motor/sync/bus failure codes all zero.
All six retained phase pairs had the intended current signs, approximately
300..350 mA. Scott reports slight shaft oscillation rather than continuous
rotation. This supports electrical commutation but does not prove rotor following.
The leading incomplete run/reset fragments are not used to infer a reset cause.
See `bench/2026-11-10-motor-continuous-v1-user-stop-summary.json`.

Ranked explanations are discrete 60-degree field jumps, insufficient torque
margin at 350 mA, and remaining phase/current errors. Their physical causes are
not resolved. This trial changes the current trajectory/controller while keeping
the current target, electrical field speed, voltage ceiling and current trips.
It is an experiment; host simulations cannot verify actual motor rotation.

## Behavior

Build target: `drv8353-motor-smooth.elf`; separate from the preserved v1 target.
After a 0.5 second alignment, blend the signed three-phase current references
between AB/AC/BC/BA/CA/CB vertices on every 50 us sample. Each blend takes 250 ms.
This creates a continuous polygonal rotating current vector, not a sine wave.
One electrical cycle takes 1.5 seconds; with four pole pairs the commanded field
makes a full mechanical revolution every 6 seconds. It repeats indefinitely.
No programmed coast gaps, current-ramp resets, finite travel range or timed stop.

Scott requested visibly larger motion. The earlier command already requested
full revolutions; the small actual displacement was failure to follow, not a
small programmed travel limit. Actual rotation must still be observed on the
next run. A light shaft marker attached with power off can improve visibility.

The new signed per-phase integral controllers use all three current channels.
Common measurement-current offset is rejected from regulation, while the
unmodified channel values still feed raw/filtered current trips. Integrator
common mode is removed and vector anti-windup bounds all three PWM legs.
Current-sum consistency replaces the old pair/unused-phase check because all
three phases now participate: absolute filtered sum above 150 mA trips after
the startup settling interval. Missing target and sustained saturated tracking
error also stop drive. Each sector requires at least 100 samples within the
105 mA per-phase tracking-error allowance. Saturated error above 75 mA for
200 samples (10 ms), after startup, trips no-response.

Nominal peak phase reference remains 350 mA. Filtered trip 500 mA, sampled raw
trip 1 A, maximum PWM span 56/1250=4.48%, individual duties 47.76..52.24%.
Absolute current calibration and instantaneous peak current remain unverified.
USER, nFAULT, ADC rail/timing/freshness, bus and field-progress guards remain.
SPI and UART remain quiet while driven. No automatic restart.

## Operator steps

1. With EVM power OFF and Nucleo USB connected, flash the smooth image.
   Retain motor and existing harness; keep Halls disconnected and motor unloaded.
2. Check `SMOOTH continuous rotation v2 (350mA, Hi-Z SPI)` and self-test PASS.
3. Apply 12 V with the existing 0.25 A supply limit; press/release USER.
4. Observe whether the shaft makes repeated full turns. USER hold stops it;
   protection may also stop it. Serial feedback prints only after shutdown.
5. Report actual motion and the stop log. If it still oscillates or stops early,
   stop manually and send the result rather than extending the observation.

Feedback now includes signed `reference_mA` beside measured/filtered currents.
History is decimated at 10 ms so retained frames span each last full blend,
rather than showing only the tail. `field_advances` excludes alignment completion.

## Validation and deployment

Host ASan/UBSan tests cover four 20-second synthetic three-wire motor-current
plants with deadbands and delays, all six sectors, <=1 mA reference increments,
<=1 count normal PWM increments, signed tracking and PWM bounds, plus raw,
filtered, current-sum, no-response and invalid-configuration failures. The
baseline controller, trace, ADC queue and finite/continuous suites also pass.
These models omit actual mechanical dynamics and do not establish rotor tracking.

All ten diagnostic targets build without warnings. All prior HEX image hashes
match their saved values. New HEX SHA256:
`148F5F43B19F66A39B5A9E6EAD47F85B81A40F0026183FC4BD33996D296893DE`.
ELF software shutdown self-test symbol: `0x20003868`.
Build/tests/hash evidence: `bench/2026-11-10-motor-smooth-v2-*`.
Scott confirmed EVM 12 V OFF, USB connected, motor remains connected. Flashed
November 10 with the hash guard; programmer readback verified and MCU reset.
HOTPLUG read self-test `00000001` at `0x20003868` and GPIOB ODR `00000000`
(ENABLE LOW). Evidence: matching `-flash.txt` and `-self-test-swd.txt`.
Powered smooth-current run and actual shaft rotation remain pending.
