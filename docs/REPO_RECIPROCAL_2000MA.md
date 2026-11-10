# Repeating one-turn forward/back trial at2A

Scott has neither encoder nor verified Hall feedback available and explicitly
requested more current plus smooth single revolutions back and forth. This is
an open-loop motion-command trial; actual shaft travel/smoothness is unmeasured.
The prior ten-electrical-turn test produced2.5CW mechanical turns, supporting
the working4polepair/8pole assumption, but still had pole-to-pole jumps.

Target `repo-reciprocal-2000ma.elf/.hex` uses the actual original repo PI,
open_loop_step,PWM and current ADC implementations unchanged. The new command
adapter uses the cosine position-command pattern already in run_cine_sweep,
at fixed frequency and scaled/offset to travel0->2pi->0 mechanically. It does
not use or recreate the unavailable encoder-based closed-loop FOC.

## Motion and profile

- Nominal phase-current magnitude reference2.0A, up from1.4A; ramped over0.5s.
-1second initial alignment, then20seconds per commanded mechanical revolution.
- Continuous cosine easing at start/end, zero endpoint velocity and continuous
  acceleration at reversal. Average3rpm per leg, peak about4.71rpm.
- Four electrical turns per leg with the4polepair assumption. Starts in the same
  logical forward direction that previously producedCW; then returns and repeats.
- No timed total-run stop or programmed coast gap. USER/fault disables drive.
-1.3V phase-voltage ceiling;2.8A filtered/4.5A sampled raw trips, each phase.
- Bench supply12V/**0.5A input limit**. This raises available input power for the
  higher phase current; input current and phase current are different quantities.

Actual motion can still stick, slip or fail to reach a full revolution without
position feedback. Do not label the command trace as measured shaft position.

## Reuse and board configuration

`repo_reciprocate.c` computes the motion command only. It calls the original
open_loop_step; the caller canonicalizes negative returned angles because the
existing helper only wraps positive overflow. Its sin/cos output remains periodic.
The actual current/PWM/ADC/control and cine-sweep sources/config headers remain
diff-free. Existing700mA/1.4A/continuous HEX hashes are unchanged.

Board ADC sample time remains56cycles at25MHz. At the1.3V ceiling maxCCR1519,
earliest next switching edge34.79us. The later possible OC4 trigger25.49us plus
the8.16us three-channel sequence ends33.65us, leaving1.14us margin. Raw4.5A
watchdog bounds fit inside ADC rails for the accepted offset interval.
The existing injected acquisition/IRQ implementation is unchanged.

USER20ms,nFAULT,raw/filtered current,ADC freshness and input/offset guards remain.
Bus is checked with outputs disabled before drive; supply is assumed fixed12V
during operation. No additional regular bus/VREF conversions while driving.
RUN reports half_moves and command_mech_mdeg; neither is rotor feedback.

## Verification and deployment

ASan/UBSan tests link actual src/loops.c and exercise four alternating legs:
four electrical turns per leg, exact commanded0/360degree endpoints, bounded
continuous phase increments and zero endpoint velocity through reversals.
This is not a mechanical tracking test. MCU boot additionally exercises forward
and reverse command math at zero voltage before any enable.

Build and ELF/vector/call-chain checks pass. The original unused static-declaration
warning in current_feedback.h remains unchanged. Old image hashes match saved
values; no changes to the original motor implementations were required.

HEX SHA256:`02DCC6FAA1159B10B0C0690CC64AB893D2B1FA7843177B8E32C36806BD555A0A`.
Boot self-test0x200000BC; control ticks0x200000B8; half_moves0x2000006C;
command_mech_mdeg0x20000068; fault IRQ count0x200000AC.
Evidence: `bench/2026-11-10-repo-reciprocal-2000ma-*`.

Deployment complete after Scott confirmed EVM12V OFF. Final image verified and reset.
The first image boot faulted at the compiler-generated FP save before CPACR;
reset assembly now enables FPU before C entry for this profile only. Hardware
readback confirms self-test1, fault status clear, motion counters0 and all drive
enables LOW. See boot SWD log and boot-summary JSON.

Apply12V/0.5A,USER start, observe one forward/back pair (about41seconds including
alignment), then USER stop and report motion/RUN lines. Actual travel is unmeasured.
