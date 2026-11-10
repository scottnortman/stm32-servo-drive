# DRV8353RS-EVM standalone bring-up before STM32 connection

Reviewed: 2026-08-12

This procedure validates the TI DRV8353RS-EVM in its original TI configuration before any connection to the STM32 Nucleo. It deliberately does **not** connect or run a motor.

## Conclusion

The DRV8353RS-EVM power board has no autonomous commutation or self-test mode. TI's documented kit uses the accompanying ISO-F28027F controller card to generate gate signals and communicate with the PC. Therefore “standalone” has two useful levels:

1. **Bare power-board check:** confirms that J5 input power, the default buck/LDO power tree, and the 3.3-V indicator work. It does not exercise SPI, the DRV8353 gate drivers, the external MOSFETs, or the current-sense amplifiers.
2. **Stock TI functional check:** uses the DRV8353RS-EVM with its supplied ISO-F28027F and TI GUI, but with J6 (motor) and J3 (Hall sensors) disconnected. This can confirm controller/GUI communication, DRV8353 SPI access, and fault status without intentional power-stage switching.

Use level 2 before removing the ISO-F28027F and wiring the Nucleo. TI explicitly says the EVM was designed to be evaluated with the accompanying isolated controller board. The ISO-F28027F is a modified LAUNCHXL-F28027F whose USB side is isolated from the live EVM side; the EVM supplies its target-side 3.3 V. The power board itself is still electrically live and is not an isolation-test platform. [TI DRV8353Rx-EVM User's Guide, pp. 2 and 5](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)

## Required equipment

- DRV8353RS-EVM power board
- Supplied ISO-F28027F controller card
- Current-limited DC bench supply
- Mini-USB cable, connected directly to the PC for this setup
- DMM with insulated probes
- TI DRV8353Rx EVM GUI or the official desktop firmware/GUI package

TI specifies 9–95 V at the EVM power connector; the GUI guide specifies a 12–95 V supply. The EVM is rated as a 15-A continuous/20-A peak drive stage, but those are motor-stage capabilities, not appropriate current-limit settings for this no-load test. [TI EVM product page](https://www.ti.com/tool/DRV8353RS-EVM), [EVM User's Guide, sections 2.1–2.2 and 3.1](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf), [GUI User's Guide, section 1](https://www.ti.com/lit/ug/slvu791b/slvu791b.pdf)

TI publishes no board-level no-load current limit or pass/fail current range for the EVM plus ISO-F28027F. The DRV8353 IC alone is specified at 8.5 mA typical/13 mA maximum on VM plus 1.9 mA typical/4 mA maximum on VDRAIN at 48 V, ENABLE high, and all PWM inputs low. Those figures exclude the buck, LDO, LEDs, ISO-F28027F, and conversion losses, so they must not be used as a board-current acceptance limit. [DRV835x data sheet, section 7.5, p. 12](https://www.ti.com/lit/ds/symlink/drv8353.pdf)

## Before applying power

Facts from TI documentation:

- The stock EVM uses a split supply: J5 feeds VDRAIN, the LM5008A buck derives nominal 12 V for VM, and an LP2992 LDO derives 3.3 V. In the published Rev-A design, R19 is populated and R18 is open. [EVM User's Guide, sections 2.2, 3.1, and 3.2](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)
- J5 has a 30-A-rated fuse intended for large inrush/fault events. This fuse is not a substitute for bench-supply current limiting during bring-up. [EVM User's Guide, section 3.1, step 3](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)
- ISO-F28027F switch S4 must be **OFF** and S1 must be **ON-ON-ON** from left to right. [EVM User's Guide, p. 2](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf), [GUI User's Guide, section 2.2](https://www.ti.com/lit/ug/slvu791b/slvu791b.pdf)
- The controller must be docked J1-to-J1 and J2-to-J2. J1 contains the polarization/key feature. [EVM User's Guide, section 3.1, step 1](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)
- J1 pin 2 is the EVM's **3.3-V output/controller supply**, and J1 pin 3 is ground. Do not feed an external 3.3-V supply into J1-2. [EVM User's Guide, Table 1](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)

Recommended inspection and setup (engineering recommendations, not TI pass/fail specifications):

1. With all power removed, photograph/read the power-board assembly and revision markings. Confirm it is the **RS** (SPI) variant. TI's published design package is Rev. A; component designators can differ on another revision. The published identification resistor is R58 = 6.04 kΩ for RS and 8.06 kΩ for RH. [TI Rev-A design package](https://www.ti.com/tool/DRV8353RS-EVM#design-files), [EVM User's Guide, Table 1](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)
2. Inspect J5, the fuse, electrolytic capacitors, power MOSFETs, and both 40-pin headers for damage, contamination, bent pins, or incorrect alignment.
3. Dock the ISO-F28027F with J1 and J2 correctly aligned. Do not offset either header by one row.
4. Set S4 OFF and S1 ON-ON-ON.
5. Leave these disconnected:
   - STM32 Nucleo and every jumper wire to it
   - Motor connector J6
   - Hall connector J3
   - Any encoder
   - Mini-USB, initially
6. Set the bench supply to **12.0 V** with its output OFF. Verify polarity at the loose J5 leads before inserting them. Connect positive to J5 `+VM`/VDRAIN and negative to J5 `GND` exactly as marked on the PCB.
7. A practical initial current limit is **0.25 A**. This is a conservative bring-up recommendation, not a TI-specified acceptance threshold. If normal startup inrush makes the supply latch or fold back, switch off, allow the board to discharge, raise the limit to at most **0.5 A** for this no-motor check, and retry. A limit that remains active, a collapsing voltage, unexpected heating, odor, smoke, or noise is a stop condition.

## Test A — power-board-only sanity check

This optional test is useful if the ISO-F28027F was shipped separately or the board's history is unknown.

1. Remove the ISO-F28027F. Keep J6, J3, USB, and the Nucleo disconnected.
2. Apply 12 V at J5.
3. Confirm the EVM's `3V3` LED illuminates.
4. With the DMM referenced to EVM ground, confirm approximately 3.3 V at a labeled 3V3 point or J1-2.
5. Observe for 30–60 seconds. The supply must not remain in current limit and nothing should heat rapidly.
6. Turn the supply off and allow the bus to discharge before handling the board.

The 3V3 LED is documented as indicating a correctly sized 3.3-V signal. This test only establishes that the input and auxiliary power path are alive. With ENABLE low or uncontrolled, the DRV8353 may be asleep; sleep disables its gate drivers, charge pump, gate regulator, DVDD, current-sense amplifiers, and SPI, and resets its registers. [EVM User's Guide, Figure 3](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf), [DRV835x data sheet, section 8.4.1.1](https://www.ti.com/lit/ds/symlink/drv8353.pdf)

Do not reject a board merely because the nominal 12-V buck rail is somewhat below 12 V when J5 itself is only 12 V; TI gives the architecture and nominal output but no EVM rail tolerance for this condition. If a regulated 12-V-rail measurement is important, repeat later at 15 V input while continuing to use current limiting and no motor.

## Test B — stock TI no-motor functional check

### Power-up

TI's two guides conflict on power order: the hardware guide's prominent safety instruction says to complete connections except USB, apply J5 power, wait for the 3V3 LED, then connect USB; the GUI guide lists USB before DC power. Follow the hardware guide's explicit safety sequence:

1. With the bench output and USB off/disconnected, dock the ISO-F28027F and recheck S4 OFF/S1 ON-ON-ON.
2. Confirm J6, J3, and the Nucleo remain disconnected.
3. Apply the current-limited 12-V supply at J5.
4. Confirm the EVM `3V3` LED turns on.
5. Connect Mini-USB from the ISO-F28027F directly to the PC.
6. With the correct controller firmware loaded, `LP_LED` should remain asserted with an orange emission. [EVM User's Guide, safety instructions on p. 2 and section 3.1](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)

The board figure also identifies an `nFAULT` LED. The DRV8353 nFAULT output is open-drain and pulls low on a fault. From that logic and the published EVM LED circuit, the normal expectation is that the fault LED is off when healthy and on when nFAULT is asserted; this LED interpretation is an inference, not an explicit LED truth table in the user guide. [EVM User's Guide, Figure 3](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf), [DRV835x data sheet, pin-function tables](https://www.ti.com/lit/ds/symlink/drv8353.pdf)

### GUI and SPI check

1. Start the official DRV8353Rx EVM GUI and select **DRV8353RS**, not RH.
2. Confirm the connection pane reports `Hardware connected. Connected to Target` and the home page reports `Device Connected`. [GUI User's Guide, section 2.5](https://www.ti.com/lit/ug/slvu791b/slvu791b.pdf)
3. Do **not** click **Start Motor**.
4. If register access is needed, toggle **System Enable**, open the RS register page, and keep Start Motor untouched. TI documents that System Enable also enables the RS register page and that the page can read/write every register and field. [GUI User's Guide, sections 2.6.1 and 2.7](https://www.ti.com/lit/ug/slvu791b/slvu791b.pdf)
5. Read and record all registers before changing anything, especially:
   - Fault Status 1, address `0x00`
   - VGS Status 2, address `0x01`
   - Driver Control, address `0x02`
   - Gate Drive HS, address `0x03`
   - Gate Drive LS, address `0x04`
   - OCP Control, address `0x05`
   - CSA Control, address `0x06`
6. If either status register is nonzero, decode the bits using the data sheet. A startup UVLO can remain latched in the SPI status after the supply has recovered. Clear faults with `CLR_FLT`, wait, and read both status registers again. A clean result is both status registers zero, nFAULT released, and no fault LED. A fault that immediately returns is a failure requiring diagnosis, not permission to start the motor. [DRV835x data sheet, sections 8.3.6, 8.6.1, and 8.6.2](https://www.ti.com/lit/ds/symlink/drv8353.pdf)

TI's official Lab05b firmware initially calls `HAL_disablePwm()` and does not call `HAL_enablePwm()` until the controller enters its run/identification states. This supports treating GUI connection/System Enable plus register access—without Start Motor—as a non-switching check. This remains an inference from the official source rather than a safety guarantee, which is why J6 must stay disconnected. [TI DRV8353RS/H-EVM firmware package SLVC736A, `proj_lab05b.c`](https://www.ti.com/tool/download/SLVC736)

## Critical IDRIVE requirement

Do not run a motor at the silicon reset settings.

There is a significant age/revision conflict in TI's material:

- The 2018 EVM guides say to operate the board at 150-mA source/300-mA sink IDRIVE. The TI firmware package also forces field value 3 for source and sink, which maps to 150 mA/300 mA in the data sheet.
- The DRV8353RS silicon reset value for both source fields is `1111b` (1 A) and for both sink fields is `1111b` (2 A).
- TI's newer EVM-specific support guidance says the EVM's CSD19532Q5B MOSFETs have low gate charge and recommends the **lowest** IDRIVE settings: **50 mA source/100 mA sink**, on both high- and low-side registers. It says to make this change after System Enable and before spinning, and to repeat it whenever reset returns the registers to defaults.

For this project, adopt the newer, more conservative EVM-specific setting before any later powered switching: set bits 7:4 and 3:0 to zero in both Gate Drive HS (`0x03`) and Gate Drive LS (`0x04`), then read them back. Do not rely on what the GUI happens to display at launch. [TI EVM IDRIVE safety FAQ](https://e2e.ti.com/support/motor-drivers-group/motor-drivers/f/motor-drivers-forum/1236302/faq-drv8353rs-evm-how-to-avoid-damaging-your-drv8353rx-evm-with-too-high-idrive), [DRV835x data sheet, sections 8.6.2.2–8.6.2.3](https://www.ti.com/lit/ds/symlink/drv8353.pdf), [EVM User's Guide, cover note](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)

The no-motor validation does not need PWM or a spinning motor. Motor identification deliberately energizes and spins the motor; it is a later commissioning step after the external MCU interface, PWM safe states, fault response, gate-drive settings, current scaling, bus voltage, motor data, mechanical restraint, and emergency power removal have all been checked.

## Pass/fail record

Record these observations rather than relying on “the LEDs looked normal”:

| Check | Pass criterion | Observed |
|---|---|---|
| Board identity/revision | DRV8353RS; revision recorded | |
| Visual inspection | No damage, debris, bent/offset headers | |
| J5 polarity before connection | Matches PCB markings | |
| Supply behavior | Reaches set voltage; not continuously current-limited | |
| Input current | Record only; TI publishes no board-level pass band | |
| 3V3 LED | On after J5 power | |
| Measured 3.3 V | Approximately 3.3 V relative to EVM ground | |
| LP_LED | Steady orange after USB/valid firmware | |
| GUI target | Correct DRV8353RS target connected | |
| Fault LED/nFAULT | Released after clearing any startup latch | |
| Status register `0x00` | `0x000` after clear and reread | |
| Status register `0x01` | `0x000` after clear and reread | |
| HS/LS IDRIVE | Record; set/read back 50-mA source/100-mA sink before any future switching | |
| Temperature/odor/noise | No rapid heating, odor, smoke, or unexpected sound | |

Do not declare the complete three-phase power stage proven by this check. Passing establishes auxiliary power, the stock controller link, DRV8353 SPI communication, and a no-fault idle state. MOSFET switching, phase outputs, current feedback, and loaded operation still require staged tests later.

## Shutdown

TI says not to leave the EVM powered unattended and to remove power/interface connections when it is not in use. It does not specify a detailed shutdown ordering. The following is conservative engineering practice:

1. Leave Start Motor stopped and disable the system in the GUI.
2. Close/disconnect the GUI and unplug Mini-USB.
3. Turn off the J5 bench supply.
4. Wait for the input bus and LEDs to decay; verify the J5 voltage is near zero before moving headers or wires. Never short the bus capacitor to discharge it.
5. Disconnect J5 when the EVM is unattended or being rewired.

[TI EVM safety instructions, pp. 2–3 and section 2.2](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)

## Sources and revision limits

- [DRV8353RS-EVM product page and Rev-A design-file listing](https://www.ti.com/tool/DRV8353RS-EVM)
- [DRV8353Rx-EVM User's Guide, SLVUB79A (November 2018)](https://www.ti.com/lit/ug/slvub79a/slvub79a.pdf)
- [DRV8353Rx-EVM GUI User's Guide, SLVU791B (November 2018)](https://www.ti.com/lit/ug/slvu791b/slvu791b.pdf)
- [DRV835x data sheet, SLVSDY6A (June 2019)](https://www.ti.com/lit/ds/symlink/drv8353.pdf)
- [Official DRV8353RS/H-EVM firmware, SLVC736A (December 2018)](https://www.ti.com/tool/download/SLVC736)
- [TI E2E EVM-specific IDRIVE safety FAQ (June 2023)](https://e2e.ti.com/support/motor-drivers-group/motor-drivers/f/motor-drivers-forum/1236302/faq-drv8353rs-evm-how-to-avoid-damaging-your-drv8353rx-evm-with-too-high-idrive)

The EVM hardware/GUI guides and firmware date from 2018, while TI's safer IDRIVE guidance is newer. The current TI product page still exposes the Rev-A design package; no later public hardware design-package revision was identified during this review. Check the actual board markings and do not blindly apply Rev-A resistor/test-point designators to a different assembly.
