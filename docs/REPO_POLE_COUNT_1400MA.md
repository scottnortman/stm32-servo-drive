# Ten-turn test at1.4A

Scott reported some motion and explicitly requested doubling the drive current.
No completed mechanical revolution count or pole count has been confirmed.

Separate target: `repo-pole-count-1400ma.elf/.hex`. It retains the original repo
PI/open_loop_step/PWM/current-feedback functions and the ten-angle-wrap counter.
The original700mA counted and continuous images remain byte-for-byte unchanged.

| Board profile |700mA image|Double-current image|
|---|---:|---:|
| Nominal phase-current amplitude reference |0.7A|1.4A|
| Phase-voltage ceiling |0.55V|1.1V|
| Each filtered phase-current trip |1A|2A|
| Sampled raw-current watchdog |2A|4A|
| Current ADC sample setting |84cycles|56cycles|

The original current ADC implementation is unmodified. The board adapter selects
the shorter sample time after current_feedback_init, and checks it at boot.
The existing calibration preserves this setting. This configuration change is
necessary to accommodate the wider PWM swing; there is no new acquisition ISR
or IRQ-entry timing predicate.

Timing at the existing100MHz TIM1/25MHz ADC: ARR2499,CCR42449,maxphaseCCR1477
at1.1V, earliest next phase edge35.21us. Even using the later OC4 trigger edge
at25.49us, three56+12cycle conversions finish at33.65us, leaving1.56us before
that edge. The earlier trigger has more margin. Raw4A offset thresholds also
fit inside the ADC rails across the accepted calibration-offset range.

No electrical frequency, alignment, current-ramp duration or PI gain changes.
One-second alignment, one-second speed ramp, nominal0.1Hz electrical and exactly
ten electrical turns, then final field held until USER/fault. No assumed motor
pole count is used. Pole inference still requires no jumps, reversals or stalls.

Keep the bench supply fixed at12V/0.25A. As in the700mA counted image, bus voltage
is validated before drive with outputs disabled; no regular bus/VREF reads are
added while running. Current,nFAULT,USER and ADC freshness protections remain.
All current numbers are nominal using the existing reference gain40/7mOhm/3.3V
conversion. Sampled raw trips are not a guaranteed bound on instantaneous peaks.

Build and existing-counter ASan/UBSan regression pass. ELF inspection confirms
the actual original helper/PWM/ADC chains and correct ADC protection vector.
Core implementation/config/board files remain unchanged. The known pre-existing
unused static declaration warning in current_feedback.h remains.

HEX SHA256:
`B24E90832214BAD34DFE7535D24A87514F431F3DD8F179E75D834A7E8C2DCC65`.
Boot self-test0x200000A8; electrical-turn count0x20000068; control ticks0x200000A4.
ADC current sample bits expected `SMPR1 &0x1FF ==0xDB`.
Build/link/geometry/counter evidence: `bench/2026-11-10-repo-pole-count-1400ma-*`.

Not flashed yet. Current MCU image is the700mA counted test. Hold USER to disable
the present run/hold, turn EVM12V OFF, then program. After disabled-output boot
verification, apply12V/0.25A and USER start. Observe/count after alignment, USER
stop after COUNT COMPLETE, and report shaft travel and RUN/STOP lines.

Deployment: after Scott confirmed EVM12V OFF, hash-guarded flash/readback/reset
succeeded. HOTPLUG verified boot self-test1, turns0, controlticks0, current ADC
sample bits0xDB, TIMARR2499/CCR42449, ADC prescaler/4, PA4 analog and all drive
outputs disabled. See matching `-flash.txt` and `-self-test-swd.txt`. Powered
1.4A behavior, shaft travel and pole-count inference remain pending.

## Observed result

Scott reports2.5clockwise shaft revolutions for ten electrical turns. Motion
had smooth portions with pole-to-pole jumps. This strongly supports4polepairs/
8poles as the working setting and agrees with the motor specification; it does
not demonstrate uniform rotor following or identify why the jumps occur.

Read-only HOTPLUG retained state: electricalturns10,fieldangle0,reason0,nFAULT1,
nominalcurrentmagnitude1.400589A,phasecurrents-0.004732/-1.215515/+1.210362A,
voltageamplitude0.870882V below1.1V ceiling,maxcontrolIRQ14.62us,preflightbus11755mV.
Driver was still enabled holding the final field at the read. USER disables it.
Evidence:`bench/2026-11-10-repo-pole-count-1400ma-result-swd.txt` and-summary.json.

No further firmware/current changes made. Retain4polepairs. Next establish
available rotor feedback: original encoder-based closed-loop FOC is available;
Hall compatibility/output function was not established in the earlier test.
Question pending: quadrature encoder available, or only built-in Halls?
