# Hall sensor identification and interface review, 2026-11-11

Scott reports all three Hall outputs remain HIGH and can partly read a sensor
marking as Honeywell `41F533` after opening the motor. No photograph has been
reviewed. This is consistent with SS41F: Honeywell drawing Figure4 marks the
package `41F` over `XXX`. The `533` suffix is likely traceability/date information,
but its interpretation and the installed part identity are not confirmed.

## Electrical finding

Honeywell SS41F/SS41G datasheet32312814 IssueA Table1 specifies 4.5..24 V operating
supply and an open-collector sinking output. The 3.3 V EVM Hall supply is below
that rating. Undervoltage is therefore a leading explanation for non-switching,
not proof of sensor failure. The page2 startup notice mentioning3 V does not
supersede the electrical operating-range table. At rated power, bipolar sensors
respond to alternating magnetic poles; moving/removing the Hall board from its
normal rotor position can also prevent the expected shaft-turn transitions.
Figure4, marked face toward viewer/leads down: left=Vs, middle=GND, right=OUT.
Do not infer installed PCB wiring just from lead positions or wire colors.

## Who uses the signals

Our driver firmware configures DRV8353 register2=0x0020, 3xPWM. Here the MCU
commands the phase PWM/enables; the DRV does not use Hall feedback for commutation.
EVM J3 routes Hall outputs to controller header J2-3/-5/-15, with R49/R51/R53=1k
pull-ups to the EVM3.3 V rail. R48/R50/R52 are optional DNP links for direct Hall
commutation in the separate1xPWM mode. Leave them DNP for our setup.
Hall signals may bypass the EVM and connect directly to STM32 digital inputs.
The currently flashed motion image still commands angle open loop; connecting
Halls does not automatically enable feedback. The separate hall-bringup image
already has an observation-only reader for PB6/PB7/PB8, but has not been flashed.

## Proposed direct-MCU wiring, not yet performed

Assuming identification/wiring checks confirm SS41F and the5 V rail is on the
NUCLEO-F411RE, use its +5 V output CN6-5 or CN7-18 for motor Hall V+.
Disconnect all five Hall wires from EVM J3 for this direct-wiring option.
In particular motor Hall V+ must not remain tied to J3-1: feeding5 V there would
backfeed the EVM3.3 V rail. Keep Nucleo/EVM3.3 V regulators separate.

| Function | Nucleo connection |
|---|---|
| Motor Hall V+ | +5 V, CN6-5 or CN7-18 |
| Motor Hall ground | GND, e.g. CN7-20; existing EVM common ground retained |
| Hall A/U | PB6, CN10-17 |
| Hall B/V | PB7, CN7-21 |
| Hall C/W | PB8, CN10-3 |

The requested first test uses STM32 internal pull-ups to3.3 V on all three
inputs (30..50k, typically40k). These are enabled in the manual Hall diagnostic
and the24 V D-axis motion image (interface0x10007). For stronger noise immunity,
external4.7k pull-ups to Nucleo3.3 V (CN6-4) remain an option after observation.
SS41F open-collector outputs permit
5 V sensor power with3.3 V signal pull-ups. STM32F411 DS10314 Table8 labels
PB6/PB7/PB8 FT digital pins;3.3 V pull-ups avoid relying on5 V tolerance and
simplify power sequencing. Check for additional motor-PCB pull-ups to5 V and
verify output high levels before connecting the MCU inputs.

Make wiring changes with both boards unpowered. For the initial manual Hall
check keep motor phase leads disconnected, MCU configured as digital inputs,
and verify5 V at sensor Vs-to-GND and alternating LOW/HIGH outputs while
turning the intact rotor or applying opposite magnet poles. No power/wiring,
MCU reset/flash or powered Hall test was performed by the agent in this review.
NUCLEO +5 V is a documented extension-board supply output when USB/VIN/E5V
powered; available total current must respect the selected board power source.

## Primary references

- [Honeywell SS41F/SS41G datasheet32312814 IssueA, manufacturer-authored distributor copy](https://www.mouser.com/catalog/specsheets/hwsc-s-a0001295895-1.pdf), pp2..4.
- [Honeywell SS41 family product page, including SS41F](https://automation.honeywell.com/us/en/products/sensing-solutions/sensors/magnetic-sensors/ss41-l-t2-t3-s-sp-ss51t-bipolar-hall-effect-digital-sensor-ic).
- [TI MD016A RevA schematic](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/38/DRV8353RH_2D00_EVM-schematic.pdf), connectors/interface and regulator sections.
- [DRV8353 datasheet](https://www.ti.com/lit/ds/symlink/drv8353.pdf), sections8.3.1.1.2 and8.3.1.1.3, 3x versus1xPWM.
- [STM32F411 DS10314](https://www.st.com/resource/en/datasheet/stm32f411re.pdf), Table8 pp46..47.
- [Nucleo UM1724](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf), section7.5.4 and Table29.
