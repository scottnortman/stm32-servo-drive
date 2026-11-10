# Motor run using the existing repo path

Scott explicitly directed us to stop recreating the working motor code. The
parallel sector/current controller and its ADC IRQ-count timing guard are not
linked into this image. The proposed patch for that guard was abandoned.

The last700mA diagnostic stopped before motor drive: SYNC failure2 at down-count
775, period5000CPUticks, samples0,run_ms0,pulse_was_on0,CCR625/625/625.
This was not a700mA torque trial. See the saved v3 stop summary under `bench/`.

## Actual reused implementation

New target `repo-motor.elf/.hex` builds these existing source files directly:

- `src/loops.c`: `open_loop_step` advances electrical angle.
- `src/pwm.c`: existing inverse Park/Clarke, voltage-to-duty conversion, TIM1 setup.
- `src/current_feedback.c`: existing ADC init, calibration, ISR/IIR and amp conversion.
- `src/control.c`: existing `pi_init`/`pi_step`.
- `src/drv8353.c`: existing SPI initialization, configuration and readback.
- Existing clock, system and startup files.

The first four implementation files, motor config and board headers have no
diff from the repo reference. The working SPI driver retains the earlier
communication fixes already present in this workspace. No motor math, PWM setup,
or ADC acquisition implementation was copied into another module.

`src/repo_motor_main.c` is a bench entry point because the normal main requires
the encoder/RPi handshake absent here. It supplies USER PC13, existing INL GPIO
enables, UART, bus supervision and current/fault shutdown. It calls the real
`open_loop_step -> pwm_apply_dq -> pwm_apply_phase_volts` chain at TIM1 updates.
There is no custom six-sector generator or IRQ-entry-count rejection.

The existing PWM B/C permutation is retained. For this non-encoder amplitude
control, the three-phase current magnitude is permutation-invariant; actual
direction is not asserted. The normal AKM encoder profile is not activated.

## Board and motor settings

The GEMS motor has four pole pairs; target field speed remains10rpm. Start with
a1second fixed-field alignment, ramp phase-current magnitude reference0->0.7A
over0.5s, then ramp field speed0->10rpm over1s. Continue indefinitely afterward.
Current magnitude limits voltage amplitude through the existing PI function,
Kp0,Ki4V/(A s),dt50us,out0..0.55V. These are conservative bench settings,
not the normal AKM fitted closed-loop gains.

This is voltage-driven open-loop commutation with current-amplitude regulation.
It is not encoder-based position/velocity FOC. No actual shaft angle or speed
is measured; larger actual travel still requires observation.

The0.55V phase-voltage ceiling corresponds to at most about0.953V line-voltage
amplitude at the configured12V bus. The existing PWM and ADC trigger/sample
settings are used as implemented by the reference modules. Current conversion
also retains the reference's nominal3.3V/gain40/7mOhm scaling and IIR filter.
Current calibration and instantaneous peaks are not independently verified.

Protection adapter: each filtered phase trips above1A; injected ADC watchdog
trips at nominal2A using calibrated offset bounds. Its small interrupt adapter
delegates acquisition to the original ADC_IRQHandler. The startup vector selects
that adapter only with `REPO_MOTOR_BENCH`; all earlier targets retain their vectors.
The original ADC ISR and new adapter are both present and the actual vector is
verified against the ELF. No linker wrapping assumption is used.

USER held20ms and nFAULT EXTI disable ENABLE/INLs/PWM. Math runs with IRQs enabled;
a preempting stop leaves outputs disabled even if a final CCR write completes.
ADC freshness2ms, ADC rail/offset checks, configuration readback and10..14V bus
supervision remain. SPI ends before motor arming. RUN status prints once a second.
The original driver register configuration is used, including its gate-drive
and latched OCP settings; no diagnostic register profile is substituted.

## Verification and use

Build passed. An existing unused static declaration in `current_feedback.h`
produces a compiler warning; its original implementation was left unchanged.
ELF inspection verifies original call chains, ADC vector routing and absence
of all parallel `motor_probe`/`motor_continuous` controller symbols. All previous
diagnostic HEX hashes remain unchanged. See `bench/2026-11-10-repo-motor-v1-link-verification.json`.

The disabled-output boot check executes the real zero-voltage sine/PWM path,
checks original ADC acquisition and TIM1 interrupt progress, and exercises the
software EXTI shutdown vector. It requires ENABLE/INLs/MOE LOW afterward.
This does not prove the physical nFAULT wire or actual rotor tracking.

Program with EVM12V OFF and USB connected; motor may remain connected and unloaded,
Halls disconnected. After boot PASS, apply12V with the existing0.25A supply limit
and press/release USER. Observe after alignment/speed ramp and report actual
shaft motion plus RUN/STOP lines. USER stops; there is no programmed run limit.

Final HEX SHA256:
`E2E18E21786BCEEA3F70F70CAD550087D948BB3F607A6AFDC1C5736FBA51C38E`.
Boot self-test address: `0x200000A4`; timer-events `0x20000098`;
software fault-IRQ count `0x20000094`.
Flashed after Scott confirmed EVM12V OFF and USB connected. Programmer readback
verified the image and reset the MCU. HOTPLUG confirmed boot self-test1,
software fault IRQ count1, timer/ADC acquisition progressing, control ticks0,
ENABLE/INLs/MOE LOW and FPU access enabled. Powered motor operation remains
pending. Evidence: matching `bench/2026-11-10-repo-motor-v1-*` files.


## V2 board-sense correction

V1 stopped before drive with reason6,control_ticks0,VBUS2509/2473mV. Scott's
supply displayed12V/4mA. Live SWD showed PA4 MODER00(digital input), which the
board adapter had omitted configuring for ADC4. With ENABLE,INLs andMOE allLOW,
changed only PA4 to analog and triggered one regular bus conversion: raw74->364,
nominal estimate11804mV. The before/after evidence demonstrates this omission
caused the false bus reading. MCU was not reset and motor remained disabled.
See `bench/2026-11-10-repo-motor-v1-pa4-analog-ab-swd.txt`.

V2 sets PA4 analog/no-pull at board startup and includes those mode bits in the
boot check. Banner identifies REPO MOTOR v2. Motor/PWM/current/PI implementations,
current/voltage settings, ADC acquisition and IRQ routing are unchanged. Link
verification repeated. Existing unused static-declaration warning remains.
New HEX SHA256:7001D6E329A9BBE24F1DCEC77CDCC5700231F4C5A8756EF912C282ECCADB67B1.
Boot self-test address remains0x200000A4. V2 not flashed yet; v1 is stopped and
PA4 is temporarily analog in RAM until the next reset. Fresh EVM-off state is
needed for programming; USB already verified connected by ST-LINK.

V2 deployment: after EVM-off confirmation, hash-guarded programming/readback/reset
succeeded. HOTPLUG verified boot self-test1, software fault count1, motor control
ticks0, PA4 analog/no-pull and ENABLE/INLs/MOE LOW. Powered v2 operation pending.
See matching `bench/2026-11-10-repo-motor-v2-flash.txt` and `-self-test-swd.txt`.
