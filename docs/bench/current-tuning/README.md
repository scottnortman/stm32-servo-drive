# Stationary current-loop captures, 2026-11-10

Raw five-signal MCUViewer FREE Recorder data at 20 kHz/50 us. Motor: GEMS
GM42BLF60-240; NUCLEO-F411RE and DRV8353RS-EVM. Gain20, nominal7mOhm scaling.
0.5A steps and DC-biased0..0.5A tones,0.75V vector ceiling,12V bench supply.

Tested PI:Kp0.8V/A,Ki2000V/(A*s). Not full-current or rotor-feedback validation.
These captures were made with the pre-upstream-integration firmware; historical
firmware SHA256 is in tools/tuning/current-loop-gains.json.

From the repository root run:

```sh
python tools/tuning/analyze_pi_tuning.py
python tools/tuning/summarize-captures.py
```

Requires numpy and matplotlib. Both scripts use these checked-in captures if
newer local outputs/tuning captures do not exist, and write generated plots
under outputs/tuning. Tone margins use a sparse SISO estimate, not full MIMO
robustness certification. No recorded frequency exceeds800Hz.
