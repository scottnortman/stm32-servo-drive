# Separate250mA stepped-crawl trial

The100mA electrical sequence completed all reverse/field steps, but Scott
reported **no shaft rotation** and confirmed free hand rotation with EVM OFF.
See `bench/2026-11-09-motor-spin-v1-first-run-analysis.md`. The new trial tests
whether additional low-current torque can overcome detents/friction. That
explanation is not yet established, and movement is not guaranteed.

Image: `build-bringup/drv8353-motor-spin-250ma.hex`

```text
Diagnostic: DRV8353RS reverse-and-stepped-crawl v3 (250mA, Hi-Z SPI)
```

The [original sequence/procedure](MOTOR_STEPPED_CRAWL.md) remains applicable:
one USER press runs B->A,C->B,A->C checks first, then alignment and24 advances
if all three pass. Expect a stepped field turn over about6..7s, not measured
or closed-loop shaft speed. Report actual shaft movement separately from
electrical completion. Keep the shaft unloaded/free and do not hand-start it.

Changes are explicit:

| Parameter |100mA baseline |New trial |
|---|---:|---:|
| Nominal requested current |100mA |250mA |
| Differential compare cap |24/1250=1.92% |40/1250=3.2% |
| Individual leg duty range |49.04..50.96% |48.4..51.6% |
| Nominal line-line average cap at12V |0.2304V |0.384V |
| Target-recognition band |70..130mA |175..325mA |

500mA filtered and1A sampled raw trips are **unchanged**. The20ms current ramp,
250ms independent pulse cutoff,12s total maximum, latched0.2V VDS OCP,150/300mA
gate strength/100ns deadtime, zero ADC preflight, fresh offsets, quiet SPI,
bus/timing/CCR/INL guards and USER/nFAULT shutdown remain. No automatic retries
or further current increase. These currents remain nominal shunt-derived values,
not independently calibrated amperes or continuous peak-current limits.

Use12V/0.25A supply limit as before. After flash/startup-self-test verification,
one USER press starts the combined test. Wait through FEEDBACK END, switch
EVM OFF, then send the full log and whether the shaft rotated/stepped/twitched
or remained still. If it stops early, preserve the stop lines before reset.

The five baseline and three250mA suite invocations pass ASan/UBSan, checking
both target variants, raw/filtered trips, regulation synthetic plants, all PWM
bounds/ADC quiet windows and the closed field sequence. Results:
`bench/2026-11-09-motor-spin-v2-250ma-host-tests.txt`. All seven images build
without warnings and all six previous HEX hashes stay unchanged. No R/L or
AKM motor/encoder gains were imported for this trial.

New image text16899/data4/bss11984. HEX SHA256:
`66FF84642F250A27C01454675A647738A1987A8DC0098CD7642FBA252A66FE9A`.
Shutdown-self-test symbol resolves0x20002EB8; resolve again before programming.
Current status: flashed and readback verified, startup software-triggered
shutdown-self-test PASS (SWD0x20002EB8=00000001). Debug connection and ENABLE
LOW were checked before programming; Scott had just confirmed the EVM-OFF
shaft check. Evidence: `bench/2026-11-09-motor-spin-v2-250ma-preflash-swd.txt`,
`bench/2026-11-09-motor-spin-v2-250ma-flash.txt` and
`bench/2026-11-09-motor-spin-v2-250ma-self-test-swd.txt`.
Scott reported slight movement on v2. Retained SWD state showed a watchdog trip
BEFORE the first pulse: zero controller samples/reverse passes/crawl holds,
matched625 compares and SPI active. Thus250mA torque has not yet been tested.
See `bench/2026-11-09-motor-spin-v2-250ma-first-run-analysis.md`.

The built/host-tested v3 startup fix requires INLsLOW for ALL SPI, including
clear-COAST readbacks. After1ms quiet settling, it arms at a known PWM phase
(up-count200..400,4..8us) with all INHsHIGH and >=9us to the17us ADC trigger.
It enables the raw-current watchdog BEFORE raising INLs. Off-state sampling
requires all INLsLOW; on-state sampling requires all INLsHIGH and watchdogON.
Only after INLs goLOW at cutoff is that current watchdog disabled for Hi-Z SPI.
Driven-pulse current thresholds are unchanged; ADC rail/timing/freshness and
nFAULT checks remain during setup. New SYNC failure7 means arm-window timeout.

V3 image text17407/data4/bss11984, SHA256:
`B736033AEC48890302BF16A8F3A2CBBA5C8F82E9FEDCBF9F86A112F3FA52EF18`.
Regression results: `bench/2026-11-09-motor-spin-v3-hizspi-host-tests.txt`.
Current status: v3 flashed/readback verified on November10,2026 after Scott
confirmed EVM OFF/USB connected. Startup software-triggered shutdown-self-test
PASS read at SWD0x20002EB8=00000001; GPIOB ODR confirmed ENABLE LOW. Evidence:
`bench/2026-11-10-motor-spin-v3-hizspi-flash.txt` and
`bench/2026-11-10-motor-spin-v3-hizspi-self-test-swd.txt`.
The first complete powered v3 electrical run PASSED:3 reverse checks,25 holds,
24 advances, all failure0 and STOP11. Nominal target250mA was recognized on
every step; max active-pulse sampled magnitude335mA. Field elapsed6306ms is
commanded field progress, not measured shaft motion. Actual movement report
is pending. See `bench/2026-11-10-motor-spin-v3-hizspi-first-run-analysis.md` and
matching full log/CSV/JSON. No further firmware or current-limit change yet.
