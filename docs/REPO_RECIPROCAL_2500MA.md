# Reciprocal commissioning trial at 2.5 A

Scott reports the 2 A reciprocal image produces +/-0.5 rev motion but remains
uneven. He notes that open-loop commissioning is normally much smoother and
explicitly requests a current increase. Lack of rotor feedback alone does not
establish the cause of rough motion. No cause has yet been proven.

Separate target: `repo-reciprocal-2500ma.elf/.hex`.

- Phase-current magnitude reference: 2.5 A, increased 25% from 2 A.
- Same 1 s alignment, 0.5 s current ramp, 20 s per commanded revolution,
  cosine forward/back trajectory and continuous repetition.
- Voltage ceiling 1.6 V, increased from 1.3 V to give current headroom.
- Filtered per-phase trip 3.3 A; sampled raw trip remains 4.5 A.
- Supply 12 V / 0.75 A input current limit. Input current differs from phase current.
- USER, nFAULT, ADC freshness/rail/offset protections retained.
- Bus preflight only, fixed 12 V assumption during run, as prior image.

The original repo open_loop_step, PWM, ADC acquisition, PI and trajectory
implementations are unchanged. Only a separate profile/target and identifying
console text were added. The verified 2 A HEX is preserved unchanged.

Build passes with the existing unused static declaration warning. ELF checks
confirm ADC vector, original call chain, and FPU enable before C startup.
Geometry: max phase CCR1582, next edge34.16us; latest possible three-channel
ADC completion33.65us, margin0.51us. ADC remains56cycles at25MHz. Raw threshold
1563counts fits accepted1800..2300 offsets without underflow/rail overflow.
Trajectory tests were already passed for the identical trajectory source;
actual mechanical smoothness remains a bench observation, not a software test.

HEX SHA256: D20559C3C0EE5CF652C0BFDBD0CB6D70758E483422EDFBB07190C810DAD78238
Evidence: `bench/2026-11-10-repo-reciprocal-2500ma-*`.
Deployment complete after fresh EVM OFF confirmation: hash-guarded flash, verify
and reset succeeded. HOTPLUG readback confirms self-test PASS, zero motion counters,
ENABLE/INLs/MOE LOW, original timer/sample configuration and no core fault.
Ready: power12V/0.75A, press/release USER; observe smoothness at same travel/speed.
