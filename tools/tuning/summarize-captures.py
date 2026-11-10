"""Export the first MCUViewer current steps; no target connection or control."""
import csv
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "outputs/tuning"
OUT.mkdir(parents=True, exist_ok=True)
DATA = ROOT / "docs/bench/current-tuning"
SIGNALS = ["reference", "measured", "cross_current", "voltage", "saturated"]
summary = []
fig, axes = plt.subplots(1, 2, figsize=(11, 4), sharey=True)
for ax, axis in zip(axes, ["D", "Q"]):
    for ki, color in [(50, "#bd701a"), (200, "#1762b5")]:
        stem = f"current-step-{axis}-05A-kp02-ki{ki}"
        path = OUT / (stem + ".json")
        data = json.loads((path if path.exists() else DATA / (stem + ".json")).read_text())
        columns = [np.asarray(data["g_tune_" + name]["values"]) for name in SIGNALS]
        reference, measured, cross, voltage, saturated = columns
        times = np.asarray(data["g_tune_measured"]["times"])
        edge = np.flatnonzero(np.diff(reference) > 0.25)[0] + 1
        relative = times - times[edge]
        # Centred 21-point (1.05ms) averaging is only for repeatable noise-tolerant
        # step metrics. Keep/export all raw 50us samples; not for 1kHz Bode work.
        filtered = np.asarray([measured[max(0, i-10):min(len(measured), i+11)].mean()
                               for i in range(len(measured))])
        start = np.arange(len(measured)) >= edge
        t10 = relative[np.flatnonzero(start & (filtered >= 0.05))[0]]
        t90 = relative[np.flatnonzero(start & (filtered >= 0.45))[0]]
        row = dict(axis=axis, kp=0.2, ki=ki, sample_rate_hz=20000,
                   rise_10_90_ms=float((t90-t10)*1000),
                   final_10ms_mean_A=float(measured[-200:].mean()),
                   raw_peak_A=float(measured.max()),
                   averaged_peak_A=float(filtered.max()),
                   final_cross_mean_A=float(cross[-200:].mean()),
                   saturation_samples=int(np.count_nonzero(saturated)))
        summary.append(row)
        with (OUT / (stem + ".csv")).open("w", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(["t_s", "i_ref_A", "i_meas_A", "i_cross_A", "v_V", "saturated"])
            writer.writerows(zip(relative, *columns))
        ax.plot(relative*1000, measured, color=color, alpha=0.15, lw=0.5)
        ax.plot(relative*1000, filtered, color=color, lw=1.6,
                label=f"Ki={ki}: rise {row['rise_10_90_ms']:.1f} ms")
    ax.step(relative*1000, reference, where="post", color="#303030", ls="--", lw=1, label="0.5 A command")
    ax.set(title=f"Stationary {axis} axis", xlabel="Time from step (ms)", xlim=(-5, 70), ylim=(-0.06, 0.59))
    ax.grid(alpha=0.2); ax.legend(loc="lower right", fontsize=8)
axes[0].set_ylabel("Current (A)")
fig.suptitle("First current-loop steps through MCUViewer FREE — Kp=0.2 V/A", fontsize=12)
fig.text(0.5, 0.01, "20 kHz raw captures (faint); 1.05 ms averaging (solid). Initial low-gain tests; 1 kHz bandwidth not yet established.", ha="center", fontsize=8)
fig.tight_layout(rect=(0, 0.04, 1, 0.95))
fig.savefig(OUT / "first-current-steps.png", dpi=180)
(ROOT / "docs/bench/2026-11-10-current-step-results.json").write_text(json.dumps(summary, indent=2))
print(json.dumps(summary, indent=2))
