# Slow 700 mA torque trial v3

Scott clarified that he wants more actual shaft travel while staying slow,
not faster rotation. The abandoned 10->100 rpm experiment has been removed
from active source/build targets and was never flashed. Its files are retained
under `outputs/abandoned-10x-speed-experiment` for history only.

The 350 mA smooth v2 command already requests continuous full field turns.
Scott reports better motion but only a few degrees. Live, non-atomic HOTPLUG
reads show active drive, phase progression, about350 mA signed currents and
no motor/sync/bus error. Small motion is not a programmed travel limit.
Torque margin remains a hypothesis, not a proved root cause.

## Trial changes

Separate target: `drv8353-motor-torque-700ma.elf/.hex`.

| Setting | Smooth v2 | Slow torque v3 |
|---|---:|---:|
| Nominal peak phase current command |350mA|700mA|
| Filtered current trip, each phase |500mA|1000mA|
| Sampled raw current trip, each phase |1000mA|2000mA|
| Maximum compare span |56/1250=4.48%|100/1250=8%|
| Individual leg duties |47.76..52.24%|46..54%|
| Initial alignment |0.5s|0.5s|
| Commanded mechanical field turn |6s (~10rpm)|6s (~10rpm)|
| Scheduled stop or travel limit |none|none|

More current requires voltage headroom: the 350mA runs used about50..54 of56
compare counts. The 8% ceiling corresponds to at most0.96V average line voltage
at12V before losses, while keeping long complementary pulses and the existing
ADC quiet window. The ADC timing's conservative bounds cover compares up to750;
this trial's maximum is675. Physical instantaneous peaks and current calibration
are not established by sampled-current limits.

[GEMS specifications](https://gemsmotor.com/nema-17-square-brushless-dc-motor)
list GM42BLF60-240 as24V,6.5A rated,8poles,3phases and star/WYE. Those ratings
support a cautious700mA trial, but do not prove the actual motor's torque margin,
R/L, or current calibration. Actual shaft displacement cannot be multiplied by
a known factor without rotor feedback. This remains an unloaded observation run.

## Reuse of working repo code

This target compiles the existing, unmodified `src/control.c` and calls its actual
`pi_init`/`pi_step` once per phase. It preserves v2's conservative integral gain:
Kp=0,Ki=9.765625 timer counts/(mA s),dt=50us, equivalent to32Q16 counts per mA
per sample. It does not import the AKM fitted gains or normal3V alignment.
Applied-vector back-calculation and common-mode removal retain the bounded PWM.
Thus the main behavior change is the torque command and voltage/trip envelope.

As in the working `src/main.c`, the FPU is enabled explicitly before float use.
An exact shared-PI/FPU self-test runs while ENABLE and all INLs are LOW. Startup
prints PASS before the regular shutdown self-test; a failure stays disabled.
Disassembly confirms CPACR setup precedes float instructions and the controller
calls the linked `pi_step` symbol.

The field generator and signed phase regulation remain bench-specific. This is
not the full encoder-based FOC loop in `src/sysid/foc_trajectory.c`; no rotor
angle or actual speed is measured. Centered PWM and verified physical mapping
are retained; the normal firmware's B/C permutation is not copied.

All existing USER,nFAULT,ADC timing/freshness,rail,bus,progress,current-sum and
missing-response guards remain. No SPI/UART while driven; no automatic retry.
Current-sum allowance remains150mA. Stop feedback includes current references.

## Operation

1. Confirm EVM12V OFF and Nucleo USB connected before programming. Motor may
   remain connected, secured and unloaded; Halls remain disconnected.
2. Check `SLOW TORQUE v3 (700mA, shared repo PI, Hi-Z SPI)` and both self-tests PASS.
3. Apply12V with the existing0.25A supply limit; press/release USER.
4. Observe actual shaft travel. The command remains slow and repeats full turns
   indefinitely. Hold USER to stop; current/other protections can also stop it.
   If it only oscillates, stop and send the feedback rather than extending the run.
5. UART is quiet during operation; feedback prints after stopping. Supply input
   current is distinct from phase current. Turn EVM power off before rewiring.

## Validation and deployment

Four20-second synthetic current-plant cases with deadbands and delays pass
ASan/UBSan: all6sectors,350/700mA reference bounds as appropriate, <=2mA startup
reference increments, normal PWM increments<=1count, signed tracking within105mA
after100ms, and raw/filtered/sum/no-response/configuration protections. The
shared PI implementation is linked in these host tests. These are electrical
current models, not actual mechanical rotor-tracking tests.
All baseline host suites pass. All11 diagnostic targets build warning-free;
all earlier HEX hashes, including smooth v2, match saved values.

Final HEX SHA256:
`CDBB164319F2876E45DEF2A486511CAE503998382EF3792A690E43D3BE7699A0`.
ELF shutdown self-test symbol: `0x20003898`.
Build/tests/hashes: `bench/2026-11-10-motor-torque-700ma-*`.
Scott confirmed EVM12V OFF and USB connected, motor remains connected. Flashed
November10 with hash guard, programmer readback verification and MCU reset.
HOTPLUG read shutdown self-test1 at0x20003898, GPIOB ODR0 (ENABLE LOW), and
CPACR0x00F00000 (FPU enabled). Reaching the shutdown self-test also confirms the
earlier shared-PI/FPU self-test passed. Evidence: matching `-flash.txt` and
`-self-test-swd.txt`. Powered700mA operation and actual shaft travel remain pending.
