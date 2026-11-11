"""Fit raw 20 kHz MCUViewer fixed tones and bracket closed current bandwidth.

The reported -3 dB frequency is relative to the low-frequency response. A
crossing is reported only when unsaturated captures bracket it on one axis.
"""
import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def fit_tone(signals, name, frequency, use):
    samples = np.asarray(signals[name]["values"], dtype=float)[use]
    times = np.asarray(signals[name]["times"], dtype=float)[use]
    design = np.column_stack((np.ones(len(times)),
                              np.sin(2 * np.pi * frequency * times),
                              np.cos(2 * np.pi * frequency * times)))
    coefficients = np.linalg.lstsq(design, samples, rcond=None)[0]
    fitted = design @ coefficients
    power = np.sum((samples - samples.mean()) ** 2)
    r2 = 1 - np.sum((samples - fitted) ** 2) / power if power else 0.0
    return coefficients[2] - 1j * coefficients[1], float(r2), samples


def analyze(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    config, signals = data["config"], data["signals"]
    frequency = float(config["f_start"])
    if frequency != float(config["f_end"]) or config["mode"] != 2:
        raise ValueError(f"{path}: expected a fixed current tone")
    times = np.asarray(signals["g_tune_reference"]["times"], dtype=float)
    if len(times) < 500 or not np.allclose(np.diff(times), 50e-6, atol=1e-6):
        raise ValueError(f"{path}: expected at least 500 synchronous 20 kHz samples")
    use = (times >= times[0] + 0.015) & (times <= times[-1] - 0.005)
    if use.sum() < 400:
        raise ValueError(f"{path}: insufficient steady tone samples")
    reference, ref_r2, _ = fit_tone(signals, "g_tune_reference", frequency, use)
    measured, meas_r2, _ = fit_tone(signals, "g_tune_measured", frequency, use)
    cross, _, _ = fit_tone(signals, "g_tune_cross_current", frequency, use)
    if abs(reference) < 0.05 or ref_r2 < 0.99:
        raise ValueError(f"{path}: missing or distorted reference tone")
    transfer = measured / reference
    saturation = int(np.count_nonzero(np.asarray(
        signals["g_tune_saturated"]["values"])[use]))
    voltage = np.asarray(signals["g_tune_voltage"]["values"], dtype=float)[use]
    return {
        "file": path.name, "axis": "D" if config["axis"] == 0 else "Q",
        "frequency_Hz": frequency, "gain_dB": float(20 * np.log10(abs(transfer))),
        "phase_deg": float(np.angle(transfer, deg=True)),
        "cross_axis_ratio": float(abs(cross / reference)),
        "reference_fit_r2": ref_r2, "measured_fit_r2": meas_r2,
        "voltage_peak_abs_V": float(np.max(np.abs(voltage))),
        "saturation_samples": saturation, "used_samples": int(use.sum()),
        "voltage_limit_V": float(config.get("voltage_limit", 0.75)),
    }


def crossing(rows):
    valid = sorted((r for r in rows if r["saturation_samples"] == 0 and
                    r["measured_fit_r2"] >= 0.8),
                   key=lambda r: r["frequency_Hz"])
    if len(valid) < 3:
        return {"status": "insufficient unsaturated high-quality tones"}
    baseline = float(np.median([r["gain_dB"] for r in valid[:2]]))
    target = baseline - 3.0102999566
    for left, right in zip(valid, valid[1:]):
        if left["gain_dB"] >= target > right["gain_dB"]:
            fraction = (target - left["gain_dB"]) / (
                right["gain_dB"] - left["gain_dB"])
            frequency = np.exp(np.log(left["frequency_Hz"]) + fraction *
                               np.log(right["frequency_Hz"] / left["frequency_Hz"]))
            return {"status": "bracketed", "baseline_dB": baseline,
                    "target_dB": target, "bracket_Hz": [left["frequency_Hz"],
                    right["frequency_Hz"]], "estimate_Hz": float(frequency)}
    return {"status": "not bracketed", "baseline_dB": baseline,
            "target_dB": target, "highest_valid_Hz": valid[-1]["frequency_Hz"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_dir", type=Path)
    parser.add_argument("--pattern", default="24v-tone-*.json")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--plot", type=Path, required=True)
    args = parser.parse_args()
    paths = sorted(args.capture_dir.glob(args.pattern))
    if not paths:
        raise SystemExit(f"No captures match {args.pattern} in {args.capture_dir}")
    rows = [analyze(path) for path in paths]
    bandwidth = {axis: crossing([r for r in rows if r["axis"] == axis])
                 for axis in ("D", "Q")}
    result = {"method": "raw 20 kHz fixed-tone least squares; 15 ms initial and 5 ms final exclusion; log-frequency interpolation of -3 dB crossing relative to lowest two valid tones",
              "rows": rows, "closed_loop_bandwidth": bandwidth,
              "limits": "stationary electrical axes; sparse SISO estimates; no rotor feedback or full MIMO robustness proof"}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    fig, plots = plt.subplots(2, 1, figsize=(8, 7), sharex=True)
    for axis, color in (("D", "#1762b5"), ("Q", "#ad6410")):
        subset = sorted((r for r in rows if r["axis"] == axis),
                        key=lambda r: r["frequency_Hz"])
        if not subset:
            continue
        frequencies = [r["frequency_Hz"] for r in subset]
        plots[0].semilogx(frequencies, [r["gain_dB"] for r in subset],
                          "o-", color=color, label=axis)
        plots[1].semilogx(frequencies, [r["phase_deg"] for r in subset],
                          "o-", color=color, label=axis)
        if "target_dB" in bandwidth[axis]:
            plots[0].axhline(bandwidth[axis]["target_dB"], color=color,
                             ls="--", lw=0.8, alpha=0.7)
        if bandwidth[axis]["status"] == "bracketed":
            plots[0].axvline(bandwidth[axis]["estimate_Hz"], color=color,
                             ls=":", alpha=0.7)
    plots[0].set_ylabel("Closed response (dB)")
    plots[1].set_ylabel("Phase (degrees)")
    plots[1].set_xlabel("Tone frequency (Hz)")
    for plot in plots:
        plot.grid(True, which="both", alpha=0.25)
        plot.legend()
    fig.tight_layout()
    args.plot.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.plot, dpi=180)
    print(json.dumps(bandwidth, indent=2))


if __name__ == "__main__":
    main()
