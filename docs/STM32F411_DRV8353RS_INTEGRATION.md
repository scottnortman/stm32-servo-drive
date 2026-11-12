# STM32F411RE Nucleo to DRV8353RS-EVM integration

This note defines a conservative, staged replacement of the TI ISO-F28027F controller
with a NUCLEO-F411RE. It is based on the active repository source plus the official TI
EVM user guide, TI EVM schematic, DRV8353 datasheet, ST Nucleo manual, and STM32F411
datasheet. It deliberately separates documented board/device facts from engineering
recommendations.

## Scope and safety boundary

TI states that the DRV8353Rx-EVM was designed to be evaluated with the supplied
ISO-F28027F. That controller isolates USB from high-voltage transients; the power EVM
itself is electrically live and not isolated. TI also specifies isolated test equipment,
overvoltage/overcurrent protection, and removal of power connections when the EVM is not
in operation. See the warnings on pages 2-3 of the [DRV8353Rx-EVM User's Guide
(SLVUB79A)](https://www.ti.com/lit/pdf/SLVUB79).

Replacing the ISO board with a USB-connected Nucleo removes that isolation. Even at the
present 12 V no-motor bench voltage, the Nucleo ground, ST-LINK ground, USB cable, hub, and PC
ground become part of the motor-power ground system.

Engineering recommendations for this bench:

- Stop the motor, disable the TI controller, switch the bench supply off, wait for the bus
  capacitors to discharge, disconnect both USB cables, and only then remove the
  ISO-F28027F.
- Keep the motor disconnected for the first logic, SPI, and analog tests.
- Use 12 V and a 0.25 A supply limit for the current no-motor diagnostics.
  Motor-connected limits require a separate reviewed test setup.
- Do not attach a grounded oscilloscope probe to the power stage unless its grounding is
  known to be safe. Prefer an isolated USB link/debugger or other galvanic isolation before
  switching the inverter from a PC-connected Nucleo.
- Do not flash or run the repository's normal motor-control image for the initial test. Its
  `main()` enables the driver and PWM after a button press; use a dedicated logic-only
  image whose PWM pins remain GPIO-low or high-impedance and whose TIM1 main-output
  enable remains clear.

## EVM configuration after removing the ISO-F28027F

There are no user switches on the main DRV8353RS-EVM that must be changed for the
external controller. The ISO board's S1 and S4 positions cease to matter once that board
is physically removed.

Documented default population on the EVM is:

- R19 populated and R18 DNP: split-rail operation; the LM5008A derives the driver's 12 V
  VM rail from VDRAIN.
- R36 populated and R39 DNP: the LP2992 produces the EVM's 3.3 V rail.
- R48, R50, and R52 DNP: the Hall signals are not connected to the one-PWM input path.
- J5 is the DC-bus input and J6 is the three-phase motor output.

These facts are shown in the [official DRV8353RS-EVM schematic, MD016A Rev. A](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/38/4721.MD016A_2800_001_29005F00_Sch.PDF)
and summarized in sections 2.2 and 3.2 of the [EVM user guide](https://www.ti.com/lit/pdf/SLVUB79).
No resistor-population change is needed for the first STM32 logic test.

## Power-domain rules

### Do not connect the two 3.3 V rails

EVM J1-2 is an **output** from the EVM's LP2992 3.3 V regulator. It was intended to power
the original ISO controller. The Nucleo's +3V3 is produced by the Nucleo when it is USB
powered. Do not connect EVM J1-2 to any Nucleo +3V3 pin: doing so parallels two regulators
and creates a back-power path.

Use the rails as follows:

- EVM 3.3 V (J1-2): leave disconnected from the Nucleo. Current three-PWM
  diagnostics drive INLA/B/C using PC6/PC7/PC9; do not add fixed high ties.
- Nucleo 3.3 V (CN7-16 or CN6-4): keep isolated from all EVM-side pull-ups.
- Common ground: mandatory between the boards.

The DRV8353 digital inputs accept a logic high from 1.5 V through 5.5 V, so the Nucleo's
3.3 V GPIO outputs are directly compatible. nFAULT and SDO are open-drain outputs. The
DRV digital pins have a 5.75 V absolute maximum. See the logic-input specifications and
pin descriptions in the [DRV8353 datasheet (SLVSDY6A)](https://www.ti.com/lit/gpn/drv8353).

### Recommended custom-controller power sequence

The sequence below is an engineering recommendation for this non-isolated, separately
powered controller, not TI's original ISO-board sequence:

1. With both sources off, complete and inspect the wiring.
2. Power the Nucleo by USB and run the logic-only image. Confirm ENABLE is low and all
   PWM pins are low or high-impedance.
3. Turn on the current-limited EVM supply. Confirm the EVM 3.3 V LED is green.
4. Let firmware assert ENABLE only under an explicit serial command or button action.
5. For shutdown, drive ENABLE low, turn off the EVM supply, wait for bus discharge, and
   then unplug the Nucleo USB.

This order prevents powered EVM outputs from driving an unpowered STM32 and ensures the
DRV is held asleep while power stabilizes. The final installation should use deliberate
power-domain isolation or sequencing rather than relying on this bench procedure.

## Connector reference: documented EVM functions

TI's connector tables are on pages 9-10 of the [EVM user guide](https://www.ti.com/lit/pdf/SLVUB79).
Directions below are relative to the EVM.

### J1

| J1 pin | EVM signal | Direction | Use in this project |
|---:|---|---|---|
| 2 | 3.3 V | output | EVM-local ties only; **never connect to Nucleo 3.3 V** |
| 3 | GND | power reference | common ground |
| 4 | VSENVM | analog output | optional DC-bus ADC |
| 10 | ENABLE | digital input | driver sleep/wake |
| 11 | ISENC | analog output | phase-C current |
| 13 | ISENB | analog output | phase-B current |
| 14 | SCLK | digital input | SPI clock |
| 15 | ISENA | analog output | phase-A current |
| 16 | nFAULT | open-drain output | active-low fault |

### J2

| J2 pin | EVM signal | Direction | Use in this project |
|---:|---|---|---|
| 1 | GND | power reference | optional second ground return |
| 2 | INHA | digital input | phase-A three-PWM input |
| 4 | INLA | digital input | tie high locally for three-PWM mode |
| 6 | INHB | digital input | phase-B three-PWM input |
| 8 | INLB | digital input | tie high locally for three-PWM mode |
| 10 | INHC | digital input | phase-C three-PWM input |
| 11 | SDI | digital input | SPI controller-out / DRV-in |
| 12 | INLC | digital input | tie high locally for three-PWM mode |
| 13 | SDO | open-drain output | SPI DRV-out / controller-in |
| 17 | nSCS | digital input | active-low SPI chip select |

The other connector pins are not required for the first integration. Do not connect J1-17
(IDRIVE) or J1-19 (VDS); those are hardware-interface controls and are not evaluation
inputs for the RS SPI variant.

## Stage 1 wiring: logic and SPI only

Remove the ISO-F28027F. Leave J6/motor disconnected. Do not connect any INHx, INLx,
ISENx, or VSENVM signal yet.

The following is the active source-code mapping, verified against Table 29 of ST's
[STM32 Nucleo-64 board manual, UM1724](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf):

| Nucleo signal | Nucleo header | Direction | EVM signal | EVM header |
|---|---:|---|---|---:|
| GND | CN7-20 (or another GND) | common | GND | J1-3 |
| PB0 | CN7-34 | STM32 -> EVM | ENABLE | J1-10 |
| PB1 | CN10-24 | EVM -> STM32 | nFAULT | J1-16 |
| PA5 / SPI1_SCK | CN10-11 | STM32 -> EVM | SCLK | J1-14 |
| PA7 / SPI1_MOSI | CN10-15 | STM32 -> EVM | SDI | J2-11 |
| PA6 / SPI1_MISO | CN10-13 | EVM -> STM32 | SDO | J2-13 |
| PC8 / GPIO | CN10-2 | STM32 -> EVM | nSCS | J2-17 |

Do **not** add an SDO pull-up to the Nucleo rail. The DRV datasheet states that SDO is
open-drain and requires an external pull-up; the EVM supplies it with R25 = 1.0 kOhm to
the EVM 3.3 V rail (after the R24 zero-ohm series link). A second pull-up to Nucleo 3.3 V
would unnecessarily couple the two separately regulated rails through the two resistors.
Disable the weak STM32 internal pull-up on PA6 and use the EVM's installed R25.

nFAULT is also open-drain. The EVM includes R21 = 10 kOhm and its nFAULT LED/R20 path
to the EVM's 3.3 V rail. Do not add another nFAULT pull-up; verify that J1-16 measures
near 3.3 V when no fault is present. Disable the source's redundant PB1 internal pull-up to
avoid a second pull-up domain on the same open-drain line.

For jumper-wire construction, keep SCLK, SDI, SDO, and nSCS short. A 33-100 ohm source
series resistor on SCLK, SDI, and nSCS is a useful engineering provision if ringing is seen.

## Stage 1 firmware acceptance test

The logic-only image should implement this sequence:

1. Configure PWM pins as GPIO-low or analog/high-impedance. Keep TIM1 MOE clear.
2. Configure nSCS high, SCLK low, SDI low, ENABLE low, and nFAULT as an input.
3. Wait for EVM power and an explicit operator action.
4. Drive ENABLE high and wait at least 1 ms (`tWAKE` is 1 ms in the datasheet).
5. Read registers 0x00 through 0x06 without writing any configuration.
6. Report every register value and the nFAULT pin over the existing 115200-baud UART.
7. Run a reversible write/read/restore test on one writable control field while all PWM
   inputs remain inactive.
8. Drive ENABLE low on any SPI or fault failure.

The DRV SPI frame is 16 bits, MSB first. SCLK must be low at both nSCS transitions;
nSCS must remain high for at least 400 ns between words. Data is captured by the DRV on
SCLK falling edges and propagated on rising edges, corresponding to the repository's
CPOL=0, CPHA=1 configuration. The reply is returned in the same transaction, not in a
pipelined subsequent transaction. See section 8.5.1 of the
[DRV8353 datasheet](https://www.ti.com/lit/gpn/drv8353).

### Bench-confirmed STM32 SPI receive workaround (November 6, 2026)

With APB2=100 MHz and PA5/SCK at the lowest GPIO output-speed setting, hardware SPI
repeatedly read R3 as 0x3FE while slow GPIO reads returned the reset value 0x3FF.
Comparison v2 reproduced this baseline, then changed APB2 to 25 MHz and the SPI
divider from /256 to /64, preserving the nominal 390625 Hz wire clock. All seven
registers then matched both GPIO sampling paths in two passes, with R0/R1 zero and
nFAULT HIGH. This strongly supports the last-bit SCK-feedback issue in
[ST ES0287 Rev 6, section 2.11.4](https://www.st.com/resource/en/errata_sheet/es0287-stm32f411xcxe-device-errata-stmicroelectronics.pdf).
ST lists 25 MHz maximum APB for low-speed SCK GPIO at 30 pF load. Slowing only the
SPI wire clock does not address that erratum. The validated clock change is confined
to the diagnostics; APB2 also affects TIM1 and ADC timing and must be reviewed before
adoption in motor firmware. SPI writes and the power stage are not proven by this
read-only comparison. The next separate diagnostic is `drv8353-spi-write-test.hex`.

The subsequent `drv8353-spi-write-test.hex` bench test passed at APB2=25 MHz / SPI
/64: CSA R6 read twice as 0x283, changed and read twice as 0x2C3, then restored and
read twice as 0x283. R0/R1 stayed zero and nFAULT stayed HIGH before and after;
firmware completed with ENABLE LOW. This validates a reversible gain-field write,
not every writable register or analog-current calibration. Analog sensing remains
the next stage; the reset CSA gain is 20 V/V, while the motor configuration intends
40 V/V.

Expected evidence of success is not a single hard-coded register image, because status can
vary with power and faults. It is:

- register reads are neither all `0x000` nor all `0x7FF`;
- two consecutive reads of each register agree;
- status registers 0x00 and 0x01 report no active faults;
- nFAULT is high;
- a control-register bit can be changed, read back, and restored exactly;
- ENABLE low returns the device to sleep and makes SPI inactive after the 1 ms sleep time.

## Stage 2 wiring: current and bus sensing

Only after Stage 1 passes, add the analog signals with the motor still disconnected:

| EVM output | EVM header | STM32 ADC | Nucleo header |
|---|---:|---|---:|
| ISENA | J1-15 | PC0 / ADC1_IN10 | CN7-38 |
| ISENB | J1-13 | PC1 / ADC1_IN11 | CN7-36 |
| ISENC | J1-11 | PC2 / ADC1_IN12 | CN7-35 |
| VSENVM (optional) | J1-4 | PA4 / ADC1_IN4 | CN7-32 |

On MB1136 boards, CN7-36 and CN7-38 can be routed either to PC1/PC0 (ADC) or PB9/PB8
(I2C), depending on solder bridges. UM1724 specifies SB56/SB51 on and SB46/SB52 off for
the PC1/PC0 ADC route. Verify those bridges or check continuity before trusting current
measurements.

Scott confirmed SB51/SB56 closed and SB46/SB52 open on November 6, 2026. The
independent `drv8353-analog.hex` diagnostic is built for the four signals above.
It initializes no PWM, writes no DRV registers, uses the validated APB2=25 MHz SPI
setting and software-triggered ADC at 6.25 MHz with 480-cycle acquisition. After
blue USER it reports 32-sample raw mean/min/max and approximate voltages once per
second for 30 seconds, then lowers ENABLE. A SysTick countdown also lowers ENABLE
if the main polling path stalls, while the CPU/interrupt remain running. USER,
observed fault, ADC timeout and near-rail readings end the test early. This image
leaves the amplifier at its reset **20 V/V** gain, not the 40 V/V motor setting
used in the scaling example below. It reports voltages, not calibrated amperes;
the estimates assume a 3.3 V ADC reference and need DMM comparison. It has not yet
completed analog validation. At the present 12 V bus, VSENVM should be about 0.298 V.

The first analog v1 run reported means 2042/2030/2030 for ISENA/B/C and 365 for
VSENVM (estimated 1.646/1.636/1.636/0.294 V; bus 11.836 V). It stopped after one
report with an ambiguous USER-or-nFAULT message, despite Scott confirming no second
button press. These are plausible initial values, not an analog acceptance pass.
Analog v2 is built with separate stop causes, USER debounce and a pre-sleep R0/R1
capture for observed nFAULT lows. Its hardware run stopped on `USER HIGH for 20ms`
while nFAULT stayed HIGH. The cause was incorrect USER polarity in firmware:
[MB1136 C04 schematic, sheet 3](https://www.st.com/resource/en/schematic_pack/mb1136-default-c04_schematic.pdf)
shows the PC13 pull-up R30 and active-low blue B1. Analog v3 corrects arming to LOW
press / HIGH release and stopping to LOW held 20 ms, retaining debounce. V3 was
flashed/verified and ran through 14 reported acquisitions with no STOP. Report-mean
ranges were A=2043-2046, B=2020-2022, C=2031-2035 and VSENVM=367-369 counts, with
estimated bus 11.901-11.966 V. The supplied sequence does not yet include its final
completion; DMM comparison and final gain-specific calibration remain pending.
The later full capture confirmed all 30 consecutive reports, no STOP in the last
v3 run, normal timeout/completion and ENABLE LOW. Report means ranged A=2042-2046,
B=2020-2024, C=2031-2035 and VSENVM=365-370; estimated bus ranged 11.836-11.998 V.
This passes the no-PWM acquisition-run check; DMM accuracy validation and final-gain
offset/current calibration remain pending. The full [capture](bench/2026-11-06-analog-v3-full-capture.txt)
and [30-report CSV](bench/2026-11-06-analog-v3-reports-1-30.csv) preserve the evidence.

Scott subsequently measured Nucleo AVDD at CN10-7 as 3.31 V; other DMM values are
pending because jumpers obstruct access. Equivalent unoccupied Arduino test sockets
are A5/CN8-6 for ISENA/PC0, A4/CN8-5 for ISENB/PC1 and A2/CN8-3 for VSENVM/PA4
(UM1724 Table 16, with the confirmed ADC solder-bridge routing). CN6-6/7 provide
ground. PC2/ISENC has no corresponding Arduino analog socket; use an accessible
CN7-35 solder-side contact or defer that measurement. Keep wiring intact and take
current-amplifier measurements while ENABLE is HIGH. The measured ADC reference
changes the 3.30 V voltage estimates by about +0.303%; no firmware was reflashed
solely for that reference correction.

Scott then reported the meter was not working. Analog v4 is built and flash-verified
to read internal VREFINT on ADC channel 17 with the chip's factory calibration,
calculate VDDA, and use it to scale the same external ADC inputs. ST's factory
calibration is at 3.3 V / 30 C, address 0x1FFF7A2A; a read-only SWD check on this
board returned 0x05E6 (1510). V4 preserves no-PWM/no-DRV-write behavior and the
30-second enable limit. This provides a scaling cross-check against the internal
reference and bench supply display, while absolute current gain/polarity and dynamic
sampling behavior remain unvalidated.

The v4 hardware run completed all 30 reports with no STOP and ENABLE LOW at the
end. Internal-reference means were 1503 counts, yielding VDDA=3.315-3.316 V;
calculated bus voltage was 11.988-12.053 V. This passes the tested voltage-scaling
plausibility cross-check at the 12 V bench setting. See the [v4 capture](bench/2026-11-06-analog-v4-full-capture.txt)
and [parsed data](bench/2026-11-06-analog-v4-reports-1-30.csv).

The separate `drv8353-current-zero.hex` target is built and flash-verified.
It writes/verifies only CSA R6=0x2C3 (40 V/V), records fresh
256-sample per-channel zero offsets in RAM, and observes compensated nominal
current estimates with PWM and motor disconnected. It retains VREFINT scaling and
the enable time limit. This validates offset compensation/stability when tested,
while actual current gain and polarity still require a known-current test.

Its hardware run completed all 30 reports with no STOP and ENABLE LOW. Fresh zeros
were A=2056.988, B=2037.805 and C=2030.824 counts. Subsequent 32-sample nominal
current averages ranged A=-7..+9, B=-9..+7 and C=-7..+5 mA. This passes the tested
no-current offset/stability check; it does not establish absolute gain/polarity or
instantaneous noise performance. See the [capture](bench/2026-11-06-current-zero-v1-full-capture.txt),
[CSV](bench/2026-11-06-current-zero-v1-reports-1-30.csv) and
[summary](bench/2026-11-06-current-zero-v1-summary.json). Recalibrate offsets at
the configured gain on every startup instead of copying these constants.
Earlier diagnostic arming loops were also
corrected in source; preserved pre-fix HEX files remain available.

The EVM uses 7 milliohm shunts and 56 ohm/2.2 nF output filters. Its DRV VREF is the EVM
3.3 V rail. With the repository setting `VREF_DIV=1` and `CSA_GAIN=40`, zero current should
be approximately 1.65 V, and:

```text
Iphase = (VSOx - Vzero) / (0.007 ohm * 40 V/V)
```

Calibrate each channel's actual zero-current ADC count after ENABLE is high and before any
PWM switching. Do not assume exactly 2048 counts.

The EVM's VSENVM divider is 383 kOhm over 9.76 kOhm, so:

```text
VVSENVM = VBUS * 9.76 / (383 + 9.76)
VBUS    = VVSENVM * 40.2418
```

At 24 V, VSENVM should be about 0.596 V. The 95 V EVM maximum produces about 2.36 V,
within a 3.3 V ADC range. These resistor values are documented in the EVM schematic.
The STM32 analog pins must never be driven while the MCU is unpowered; the STM32F411
datasheet limits pin voltages and specifies ADC pins be configured in analog mode. See the
[STM32F411RE datasheet](https://www.st.com/resource/en/datasheet/stm32f411re.pdf).

## Hall connector reference (before any Hall power)

2026-11-11 identification update: user reports all outputs HIGH and markings
possibly `41F533`. Likely Honeywell SS41F: its4.5 V minimum makes EVM3.3 V
insufficient if identification is correct. Proposed sensor5 V/open-collector
signals pulled to3.3 V, direct STM32 routing and source verification are in
[Hall review](HALL_SENSOR_SS41F_REVIEW.md). Do not feed5 V into J3-1.
This is researched guidance; no successful powered Hall transition test yet.


Power clarification, 2026-11-11: Scott found EVM supply power was disconnected;
after restoring it, he reports Hall 3.3 V works. No Nucleo 3.3 V feed is needed.
TI SLVUB79A section 2.1 (page 6) explicitly assigns Hall power to the EVM LDO.
MD016A Rev A shows U3 LP2992 output through populated R36 to the common 3.3 V
net, feeding J3-1, Hall pull-ups and J1-2. J1-2 supplies the original controller;
it is not an incoming Hall-power feed. Keep the USB-powered Nucleo 3.3 V rail
separate. This resolves the supply question, not motor Hall voltage-rating or
output-switching compatibility; no new Hall transition test was reported.

TI MD016A Rev A schematic shows five-position J3: pin 1=local 3.3 V,
2=HALLA/U, 3=HALLB/V, 4=HALLC/W, 5=GND. Follow physical PCB labels/pin-1
marking instead of assuming left/right orientation. The GEMS diagram identifies
Hall red=+V, black=GND, yellow=U, green=V, blue=W, but does not establish supply
voltage or output type. Compatibility with J3's 3.3 V remains unverified.

R54 is the 50k POT potentiometer, with wiper on J1-12, not a Hall voltage control.
Hall pull-ups are R49/R51/R53=1k to EVM 3.3 V. Their signals reach controller
header J2-3/J2-5/J2-15 respectively; physical Nucleo Hall connections are still
pending. Leave R48/R50/R52 (optional direct-1PWM links) DNP. Do not inject an
external Hall supply into J3-1 or power unknown Hall wiring; determine the motor's
electrical ratings first. Motor phase leads remain disconnected for the Hall test.

The [TI schematic](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/38/DRV8353RH_2D00_EVM-schematic.pdf)
was visually checked; its shared RX drawing is orderable as DRV8353RS-EVM.

Prepared, but not flashed or bench tested: `hall-bringup.elf` uses PB6 (CN10-17),
PB7 (CN7-21), PB8 (CN10-3) for U/V/W, respectively. Connector mapping follows
[ST UM1724 Table 29](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf).
Its separate source keeps PB0 ENABLE low throughout, leaves PA8/9/10 GPIO low,
and links no DRV, PWM, ADC or motor-control implementation. It observes states
for 120 seconds after a debounced USER press/release, or until a second press.
The 5ms stable-state filter is for slow manual shaft turns, not speed measurement;
multibit changes can indicate missed transitions. `000` and `111` are flagged
under the stated 120-degree Hall assumption, not a universal sensor fault rule.
This build does not resolve Hall supply/output compatibility. Establish that
before powering sensors or making these additional wires with both boards off.

## Stage 3 wiring: three-PWM, motor still disconnected

### Completed bench step: Nucleo-only scope test, November 6, 2026

Scott reports all three PWM outputs are correct. This records a user-reported
pass for the Nucleo frequency/duty/pin mapping; exact measured values and the
USER-stop result have not yet been recorded.

Hall diagnosis is deferred at Scott's request. Hall red supply was measured about
3.3 V, but three outputs stayed low while turning; no Hall functionality has
been established. All five Hall and three motor phase leads were disconnected
with DRV power off before the next image was programmed.

`pwm-bringup.elf` is a separate diagnostic, built and flash verified. Keep DRV
power **off**, motor/Hall leads disconnected, and PA8/9/10-to-DRV wires disconnected
for this first scope test. PB0 ENABLE stays low throughout; no driver configuration,
ADC, Hall, encoder or motor-control code is linked. Existing normal PWM source
and its phase mapping have not been changed by this diagnostic.

The current v2 image waits for active-low USER press/release, then generates three
distinct center-aligned waveforms continuously, with no timeout (requested to
allow more time for scoping). A second USER press stable for 20 ms stops them.
SysTick clears timer output/counter enables and forces PA8/9/10 GPIO low on USER
stop independently of UART polling. Reset also returns to the waiting state.
These software stops are
not a demonstrated power-stage fault protection path.

| Phase | Nucleo pin | Timer output | Nominal duty | Nominal high time |
|---|---|---|---|---|
| A | PA8 / CN10-23 | TIM1_CH1 | 20% | 10 us |
| B | PA9 / CN10-21 | TIM1_CH2 | 40% | 20 us |
| C | PA10 / CN10-33 | TIM1_CH3 | 60% | 30 us |

All three have a nominal 50 us period / 20 kHz frequency. The SPI-workaround
APB2 domain is retained at 25 MHz, TIMPRE=0 gives TIM1=50 MHz, PSC=0 and
center-aligned ARR=1250 gives 2*ARR ticks per full period. See
[ST RM0383](https://www.st.com/resource/en/reference_manual/rm0383-stm32f411xce-advanced-armbased-32bit-mcus-stmicroelectronics.pdf),
RCC_DCKCFGR and TIM1 center-aligned operation. This avoids importing the normal
PWM source's 100 MHz timer assumption into the diagnostic.

Expected before/after: all three GPIO low and ENABLE low. During acquisition,
scope against Nucleo ground (e.g. CN6-6), use DC coupling and matched probe
attenuation. Record frequency and duty for each signal and the USER
stop result. No powered bridge switching or motor motion has occurred yet.

### Completed bench step: tracing PWM at EVM inputs, ENABLE permanently low

Scott reports "ok confirmed on j2": PWM input frequency/duty/assignment passed
by user observation. Exact readings and USER stop/fault timing were not supplied.

The v3 logic scope image is built, flashed and flash verified. Its only changes
from v2 are banner/instruction text: timing, duties, indefinite duration, USER stop,
and permanent ENABLE low are unchanged. COM3 was occupied; v3 startup text has
not been captured. MCU reset leaves PWM low until USER press/release.

With both boards off, connect the three PWM wires using the table below.
Keep all INLA/B/C high ties absent and motor/Hall leads disconnected. Power USB,
verify EVM J1-10 ENABLE about 0 V relative to common GND, then power EVM at 12 V
with a 0.25 A supply limit. Press/release USER and scope J2-2/6/10 versus J2-1 GND:
expect the same 20/40/60% at 20 kHz, with ENABLE staying low. USER should stop all
three inputs low; report supply current too. This checks wiring/loading only.
No SPI, ADC or bridge switching occurs in this image. TI datasheet 8.4.1.1 specifies
all external MOSFETs disabled in sleep with ENABLE low.

Scott confirms the three PWM wires are connected; measurements at the EVM input
contacts are pending. The visually verified MD016A Rev A schematic provides
TP17/TP18 for common ground, TP4 for the J5 supply bus (about 12 V here), and TP5
for VSENVM (about 0.298 V at 12 V, from 383k/9.76k divider). No dedicated labeled
test points are on INHA/B/C; use J2-2/6/10 contacts or their solder pads.
Low-side gate points TP13=GLA, TP12=GLB and TP9=GLC should remain approximately
0 V without PWM while ENABLE is low. They do not validate the incoming PWM
waveform in sleep mode. DSO ground clips go only to common GND. High-side
gate-to-source measurements and power-stage switching remain later tests.

### Skipped diagnostic: low-side-only gate scope, six-PWM mode

**Superseded at Scott's request:** skip this temporary rewire/test. Keep original
PA8/9/10 connections on INHA/B/C J2-2/6/10. The known PWM logic scope v3 image
was restored and flash verified; ENABLE permanently low. Input PWM checks already
passed by user report. The low-side image described below is retained as an
unused diagnostic and is not the active firmware. Its powered gate test was
never run. Future gate testing should preserve the original input connections
with a separately reviewed three-PWM setup. Do not perform the wire moves below.

After Scott confirmed DRV off, motor/Halls and INL ties disconnected, USB on,
the separate `drv8353-gate.elf` image was built, flashed and verified. It does
not link the normal motor-control, encoder, Hall, PWM or DRV driver code.
Its startup software-triggered EXTI1 shutdown check passed on the MCU, verified
through SWD (`g_gate_shutdown_self_test=1`); ENABLE read low. COM3 was occupied,
so UART boot capture and a physical nFAULT-triggered shutdown are not yet verified.

Power both boards off, then **move** the three existing PWM wires:

| Nucleo signal | Remove from EVM | Connect to EVM |
|---|---|---|
| PA8 / CN10-23 | J2-2 INHA | J2-4 INLA |
| PA9 / CN10-21 | J2-6 INHB | J2-8 INLB |
| PA10 / CN10-33 | J2-10 INHC | J2-12 INLC |

J2-2/6/10 must now stay disconnected, using driver input pulldowns. No INL high
ties, motor leads or Halls. This temporary six-PWM configuration tests only the
low-side gates; it is not the future conventional three-PWM motor wiring.
See [TI datasheet Table 1](https://www.ti.com/lit/gpn/drv8353): with INHx=0,
GLx follows INLx while GHx stays low.

The image holds ENABLE low until explicit USER press/release and a VREFINT-scaled
10..14 V bus check. At wake all inputs remain low; after a clean default-register
check it writes/reads twice COAST R2=0x484, HS R3=0x300, LS R4=0x300 and OCP
R5=0x315; R6 remains default0x283. These select6PWM, all-phase linked OCP,
OTW reporting,50/100mA gate drive,4usTDRIVE,400nsdeadtime,latchedOCP,
2usdeglitch and0.2VVDS threshold. GDF, GDUV and sense OCP remain enabled.
Start PWM under COAST, then clear COAST and verify R2=0x480. Register encodings
come from [TI datasheet Tables 14–18](https://www.ti.com/lit/gpn/drv8353).

All SPI waits are bounded; a readback error/timeout, bad bus, nFAULT or USER stop
removes ENABLE and forces timer/GPIO outputs low. PB1 falling EXTI1 is highest
priority; SysTick supplies a redundant level check,20ms USER debounce and a
five-minute maximum gate-enable window. This bounded powered test is separate
from the indefinite logic-only scope image. Stop paths latch, with no automatic
restart; fault registers reset on sleep. Software-triggered IRQ testing does not
prove physical fault wiring/response latency under switching.

With EVM12V,0.25A supply limit and scope groundTP17/18, use10x probes on GLA TP13,
GLB TP12,GLC TP9. Expect20kHz and20/40/60% high duty respectively, gate high near
the VGLS rail (roughly10V here). Record config/startup output, supply current,
gate waveforms and USER-stop result. Phase-current calibration under switching,
motorR/L/current rating, high-side gate behavior and motor control remain future
work. Wire moves and powered results are not yet confirmed.

### Current step: three-PWM bridge scope, original PWM wiring retained

The `drv8353-bridge.elf` target is built and flashed, using `DRV_GATE_3PWM` in
the protected gate source. It keeps PA8/9/10 on INHA/B/C J2-2/6/10 and adds
software-controlled per-phase enables. **No existing PWM wire moves.**
The following INL connections are part of the complete harness Scott confirmed
rewired on November 9, 2026. Make any wiring changes with both boards off:

| New Nucleo control output | EVM input |
|---|---|
| PC6 / CN10-4 | J2-4 / INLA |
| PC7 / CN10-19 | J2-8 / INLB |
| PC9 / CN10-1 | J2-12 / INLC |

These pin locations follow [ST UM1724 Table29](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
and [TI EVM Table2](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf). Keep INL
fixed3.3V ties absent, motor/Halls disconnected. INL controls are low through
wake/configuration, keeping the bridges disabled; after checked configuration,
start PWM underCOAST, set INLs high, verify MCU pad levels, and clearCOAST.
Every stop drives ENABLE and allINLs low before disabling TIM1.

Run R2 is0x4A0 (3PWM+linkedOCP+OTWreport), configuration COAST R2=0x4A4;
R3/R4=0x300, R5=0x315, R6 remains0x283. Other guards match the protected
low-side diagnostic:12Vbus window, boundedSPI, double readbacks,100ms register
checks,1s bus check,PB1EXTI fault shutdown,20ms USER stop and5min maximum.
The new boot shutdown self-test passed on MCU via SWD (COM3occupied): with
ENABLELOW and CCR=0, pulse INL controls high and use actual software EXTI vector
to verify INL/timer/GPIO low afterward. The full post-rewire procedure, including
unpowered wiring checks, is now user-reported PASS. Physical nFAULT-triggered
shutdown/latency still needs bench validation.

After adding wires with both boards off, use EVM12V/0.25A limit and confirm the
`THREE-PWM bridge scope v1 (NO MOTOR)` banner/self-test PASS before USER start.
GLA TP13,GLB TP12,GLC TP9 should now have roughly80/60/40% high duty at20kHz,
complementary to INHA/B/C20/40/60%, with deadtime/edge differences. Gate high is
nearVGLS (~10V here). Use10x probes and groundTP17/18. On November 9 Scott
reported all checks passed after the complete rewire: INL HIGH, gate PWM on all
three phases, and USER stop returning gates/ENABLE LOW. These are user-reported
results, not recorded exact waveform measurements. Exact frequency/duty/amplitude,
idle/running supply current and the final STOP log have not been supplied. Earlier
ACTIVE logs showed 11.760..11.896V bus, nFAULT=1 and R2=0x4A0. Motor phase
outputs TP3/2/1 can be scoped vsGND later; never put a ground clip on a phase.
MotorR/L/current ratings and actual phase-current protection are not yet resolved.

### Next check: current-sense outputs during switching, no motor

Use the same passed `drv8353-bridge.hex`; no wiring change or reflash is required.
Keep the motor and Hall leads disconnected. With fresh ACTIVE reports, measure
each current-sense output against EVM TP17/TP18 common ground:

| EVM output | Probe point | Expected zero-current DC level |
|---|---|---|
| ISENA | J1-15 | Approximately 1.65 V |
| ISENB | J1-13 | Approximately 1.65 V |
| ISENC | J1-11 | Approximately 1.65 V |

R6=0x283 selects 20 V/V gain and VREF/2 bias. The midpoint follows the actual
EVM VREF voltage and can have offset. See [TI datasheet sections 8.3.4.1 and
8.6.7](https://www.ti.com/lit/ds/symlink/drv8353.pdf). Earlier no-PWM zero-offset
measurements at 40 V/V are a different configuration, not calibration for this run.

Use 10x probes with matching scope attenuation, DC coupling, about 500 mV/div
and 10 us/div initially. Record mean and peak-to-peak voltage for A/B/C and
bench supply current. Use a short ground connection when inspecting switching
spikes; do not confuse pickup in a long probe-ground loop with amplifier output.
A near-midpoint output with small transients differs from a rail-clamped output.
No numerical noise acceptance threshold has been established; report the readings
rather than treating an arbitrary noise number as a current-protection threshold.
USER stops the bridge; the existing five-minute limit still applies. Scott
subsequently reported "all pass" for this scope check. Record this as a user-
reported pass of output bias and qualitative switching-noise inspection. Exact
DC means, peak-to-peak noise and supply current have not been supplied. This
scope check does not validate ADC sample timing or known-current gain/polarity.
Those remain later checks before connected-motor operation.

### Current diagnostic: current ADC observation during switching

`drv8353-bridge-adc.elf` compiles the protected gate source with DRV_GATE_3PWM
and DRV_GATE_ADC_CHECK. Its banner is `THREE-PWM current ADC v1 (NO MOTOR)`.
No new wires are needed. Use the existing complete harness, keeping motor and
Hall leads disconnected. The existing bridge-scope HEX remains unchanged.

After USER press/release, the same bus and register checks run. While the driver
is awake with R2=0x4A4 COAST, all INLs LOW and TIM1 outputs off, collect 256 fresh
samples/channel and print ZERO_COAST raw mean/min/max and estimated millivolts.
Then start the same protected three-PWM output sequence. Once per second collect
128 readings each from ADC10/11/12 and print ADC_BLOCK timing, SWITCHING mean/
min/max/mV, and delta_raw from the fresh zero. R6 remains 0x283, gain20. VDDA is
estimated using this MCU's factory VREFINT calibration, as in the bus guard.

The ADC uses sequential software starts, 480-cycle acquisition at 6.25MHz, with
one discarded conversion after each mux change. This diagnostic observes offsets
and sampled noise during switching. It neither proves PWM-synchronous sample
timing nor provides known-current calibration or an ampere shutdown threshold.
The gross rail check (raw<16 or raw>4079) and baseline midscale plausibility
window (mean1800..2300) are wiring checks, not motor-current limits. Reason8
means current-ADC timeout, rail or baseline plausibility failure. All existing
bus/SPI/register/fault/USER/deadline shutdown paths remain in use.

Builds pass without warnings for all three gate-source variants. Existing gate
and bridge HEX hashes are unchanged. After Scott confirmed EVM off, motor/Halls
disconnected and USB on, the ADC image was flashed, download-verified7.73KB at
3.26V under-reset950kHz, and MCU reset. Hotplug SWD at the ELF-resolved self-test
variable0x20000014 returned1=PASS. Evidence is in
`bench/2026-11-09-bridge-adc-v1-flash.txt` and
`bench/2026-11-09-bridge-adc-v1-self-test-swd.txt`. No serial capture was attempted.
The supplied [powered capture](bench/2026-11-09-bridge-adc-v1-full-capture.txt)
contains10 complete ADC_BLOCK report groups, 128 retained samples per phase in
each61..62ms block, and a USER STOP reason1 at34045ms followed by ENABLELOW /
PWMGPIOLOW. Every active report has nFAULT1, R0/R1=0, R2=0x4A0. VDDA3315mV;
bus11695..11770mV. Parsing results are in the matching reports CSV and summary JSON.

| Phase | COAST mean (mV) | Switching mean range (mV) | Largest mean shift (mV) | Largest sampled within-block span (mV) |
|---|---:|---:|---:|---:|
| A | 1649 | 1648-1649 | 0.740 | 95.5 |
| B | 1642 | 1640-1641 | 1.258 | 97.1 |
| C | 1640 | 1638-1640 | 1.767 | 89.0 |

This passes the observed no-motor ADC connectivity/readout and USER-stop check.
The small mean shifts coexist with larger individual excursions. The span is
from retained asynchronous ADC samples, not an oscilloscope full-bandwidth noise
measurement or RMS value. No motor-current limit follows from it. Next validate
PWM-synchronous sampling and freshness before using these inputs in current
control, then known-current scaling/polarity and actual protection shutdown.

### Current diagnostic: PWM-synchronous ADC timing

`drv8353-bridge-sync.elf` uses DRV_GATE_SYNC with the protected three-PWM gate
source. Scott confirmed EVM off, motor/Halls disconnected and USB on. Sync was
programmed under-reset950kHz at3.26V, download verified9.66KB and MCU reset.
The ELF-resolved startup self-test variable0x20000094 returned1=PASS via hotplug
SWD. Evidence is in `bench/2026-11-09-bridge-sync-v1-flash.txt` and
`bench/2026-11-09-bridge-sync-v1-self-test-swd.txt`. Powered acquisition and
scope timing remain pending. Existing gate, bridge and asynchronous
ADC HEX hashes are unchanged. The banner is
`THREE-PWM synchronous current ADC v1 (NO MOTOR)`. Motor/Halls stay disconnected,
same12V/0.25A bench setup and all existing harness connections retained.

TIM1_CH4 is PWM2 with CCR4=850 and OC4REF routed to TRGO. A rising trigger starts
one injected sequence per20kHz PWM period. ADC1 uses JEXTSEL1 (TIM1_TRGO), rising
edge only, and a three-rank sequence10/11/12 in JSQ2/3/4, read from JDR1/2/3.
The ADC clock is12.5MHz. Each channel acquires for56cycles and converts for12,
giving a nominal16.32us for the three sequential samples. These are not simultaneous
samples. With unchanged duties, the common low-side-ON window is15..35us from
counterbottom. The trigger is17us, nominal conversion end33.32us, before the
next phase-C edge35us. Compile-time bounds reject a sequence outside that window.

Regular VREFINT and PA4 bus reads acquire144cycles (11.52us, satisfying the
F411 VREFINT minimum10us). Each12.48us regular conversion launches during the
descendingCNT600..100 window (38..48us), finishing before the next injection67us.
Injected and regular status flags are preserved independently. ADC statistics use
two buffers, so only a bank-index swap briefly masks interrupts. No UART work is
done in the ADC IRQ. Current zero capture remains256 fresh COAST samples/channel.

The trigger mapping follows [ST's F4 ADC LL definitions](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Inc/stm32f4xx_ll_adc.h)
and [RM0383](https://www.st.com/resource/en/reference_manual/rm0383-stm32f411xce-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).
The [F411 datasheet](https://www.st.com/resource/en/datasheet/stm32f411re.pdf)
specifies VREFINT acquisition requirements. Physical header contacts follow
[UM1724 Table29](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf#page=59).

| Scope signal | Nucleo contact | Meaning |
|---|---|---|
| PA11 / TIM1_CH4 | CN10-14 | Rising edge is the hardware trigger reference. HIGH lasts about16us. |
| PC4 | CN10-34 | Pulse during completion IRQ, after all three conversions; not sample aperture. |
| Common ground | EVM TP17/TP18 | All ordinary scope ground clips stay here. |

No additional inter-board wires are needed. Use10x probes and1V/div for the
3.3V logic markers, about10us/div. PA11 should repeat at20kHz; PC4's leading
edge should occur roughly16.5..17us after PA11 rises. Check actual alignment
against INHC J2-10: trigger~2us after its falling edge; completion marker begins
before its next rising edge. PC4 pulse width includes IRQ processing and does
not represent the acquisition interval. Probe tips alone touch marker pins.

SYNC_BLOCK reports give n (~20000 per one-second report), cumulative total,
period CPUticks min/max (~5000 at100MHz), descending completion CNT bounds,
VDDA and gain. Three SYNC lines give fresh stats/mV and delta from COAST zero.
This validates timing/freshness under no load, not amperes or current regulation.
Wider sample counts and shorter acquisitions differ from the prior asynchronous
test; compare those conditions when assessing extrema/noise.

STOP9 is the new sync guard: failure1=configuration,2=completion phase outside
775<CNT<1000 on descending count,3=interval outside4500..5500CPUcycles,
4=no fresh sequence for2ms,5=unreported buffer reaches60000samples,
6=empty report. SYNC_STOP prints failure, total, lastCNT/interval and raw values.
STOP8 retains ADC rail / zero plausibility checks. Fault/USER/deadline shutdown
drops ENABLE and INLs first and disables injected triggers/IRQ and both markers.
The EXTI boot self-test also requires both marker pads LOW after shutdown.

All four variants compile without warnings. Deliberately invalid timing
configurations fail compilation as expected; evidence is in
`bench/2026-11-09-bridge-sync-v1-build-checks.txt`. Sync hardware/marker timing
and physical fault behavior remain pending. Confirm banner/self-test,
power12V/0.25A, press/release USER, capture at least three complete SYNC reports
plus scope timing, and press USER to capture STOP1 / SYNC_STOP failure0.

### Motor parameters accepted for subsequent work

Scott explicitly chose the published motor values instead of manual R/L tests
and confirmed that this motor already worked in the earlier TI setup. Preserve
that baseline. The [GEMS GM42BLF60-240 product table](https://gemsmotor.com/nema-17-square-brushless-dc-motor)
lists 24V, 4000rpm, 6.5A rated, eight poles (four pole pairs), and star/WYE
windings. It does not publish winding resistance or inductance. Leave R/L
unspecified; do not adopt AKM11E reference values or similar motors' numbers.
The motor's rated-current entry does not establish the new controller's phase-
current trip threshold. Current-sampling and protection validation still apply.

### Future step: three-PWM motor operation

The repository configures `PWM_MODE=01`, the DRV's three-PWM mode. In that mode INHx
selects the high/low state and INLx is a per-phase enable: INLx low makes that half bridge
Hi-Z, while INLx high enables it. TI explicitly directs tying all INLx high when Hi-Z control
is not required. See section 8.3.1.1.2 and its truth table in the
[DRV8353 datasheet](https://www.ti.com/lit/gpn/drv8353).

For this bench, retain the new MCU-controlled INL enables above instead of fixed
high ties. This holds all FETs off through wake/configuration and disables them
directly on stop. The earlier fixed-high proposal is superseded. Leave these
enables low during earlier SPI-only checks so every half bridge stays Hi-Z.

The intended conventional mapping is:

| Logical phase | STM32 timer output | Nucleo header | EVM input | EVM header |
|---|---|---:|---|---:|
| A | PA8 / TIM1_CH1 | CN10-23 | INHA | J2-2 |
| B | PA9 / TIM1_CH2 | CN10-21 | INHB | J2-6 |
| C | PA10 / TIM1_CH3 | CN10-33 | INHC | J2-10 |

Before enabling the EVM, scope these three signals at the EVM header with ENABLE low. Check
frequency, duty, phase assignment, and idle state. Then use the DRV `COAST` bit or keep
ENABLE low while validating firmware transitions. The motor remains disconnected until
SPI, faults, ADC offsets, bus voltage, PWM frequency, and phase mapping have all passed.

## Repository review and discrepancies to resolve before PWM

The active firmware is substantially closer to the desired EVM configuration than the
README, but it is not safe to run unchanged for first integration.

### Active source mapping versus README

- [`Include/board_f411.h`](../Include/board_f411.h) and
  [`src/drv8353.c`](../src/drv8353.c) use PA5/PA6/PA7 plus PC8 for DRV SPI, PB0 for
  ENABLE, PB1 for nFAULT, and PC0/PC1/PC2 for current ADC.
- The README instead documents PB5/PB6 for SPI, PB8/PC6 for enable/fault, PA4/PC1/PC4
  current inputs, and six complementary PWM inputs. That README table is stale and must
  not be used to wire the present source.
- The active source is explicitly three-PWM, not six-PWM.

### PWM phase mismatch inside active source

[`src/pwm.c`](../src/pwm.c) comments state the conventional PA8=A, PA9=B, PA10=C map, but
`pwm_set_duty()` currently sends logical phase B to CCR3/PA10 and logical phase C to
CCR2/PA9. In addition, the `PIN_PWM_*` macro names/comments in
[`Include/board_f411.h`](../Include/board_f411.h) do not agree with their numeric GPIO
values.

Do not connect PWM using an ambiguous interpretation. Recommended resolution before Stage
3 is to normalize the code to:

```text
phase A -> CCR1 -> PA8  -> J2-2  INHA
phase B -> CCR2 -> PA9  -> J2-6  INHB
phase C -> CCR3 -> PA10 -> J2-10 INHC
```

and then make `board_f411.h`, `pwm.c`, and the README agree.

### Unsafe initial execution flow

[`src/main.c`](../src/main.c) waits for a PC3 action and then immediately initializes the
DRV, writes registers, initializes TIM1 and ADC, raises ENABLE, calibrates ADC, and calls
`pwm_enable()`. It does not require SPI self-test success, register-write success, or an
nFAULT-high check before PWM. It is therefore a motor-test image, not a first-contact
bring-up image.

### Configuration values to review

- `0x333`/`0x733` select 150 mA source and 300 mA sink gate current and a 4000 ns TDRIVE,
  matching the old EVM guide's mandated default IDRIVE setting.
- `DRV_CFG_OCP_CONTROL=0x119` is described incorrectly in the source comment. Per the
  DRV8353 register table, it selects 100 ns dead time, latched OCP, 2 us OCP deglitch, and
  0.6 V VDS threshold—not 4 us and 0.75 V.
- `V_BUS` is compile-time 12 V in [`Include/config.h`](../Include/config.h), while this bench
  is at 24 V. Do not run voltage-normalized PWM control until this is changed or, preferably,
  replaced by a validated VSENVM measurement with range checking.
- The current scale correctly uses the EVM's 7 milliohm shunts and configured 40 V/V CSA
  gain, but offset calibration must be demonstrated on this particular wiring.
- nFAULT is polled only; it is not configured as an immediate STM32 hardware-break input.
  Before motor operation, route fault handling to an EXTI/TIM1 break or an equivalently
  immediate PWM shutdown path in addition to the DRV's own protection.

## First-integration pass criteria

Do not proceed to a connected motor until all of the following are recorded:

- no connection between the Nucleo 3.3 V and EVM J1-2;
- common ground continuity confirmed with power off;
- ENABLE is low from reset through explicit operator action;
- PA8/PA9/PA10 remain inactive during the entire logic-only test;
- EVM 3.3 V and 24 V bus measure correctly;
- nFAULT remains high and status registers 0x00/0x01 are zero;
- all writable DRV configuration registers read back exactly;
- SDO waveform has a clean high level and exactly 16 clock periods per nSCS-low frame;
- ISENA/B/C zero-current voltages are plausible, stable, and individually calibrated;
- VSENVM agrees with the bench supply within divider/ADC tolerance;
- the normalized A/B/C PWM code mapping has been corrected and scope-verified;
- firmware disables PWM and ENABLE on any SPI, ADC-range, bus-range, or fault failure.

Only then should the motor be reconnected for a low-current, open-loop phase-order test.
