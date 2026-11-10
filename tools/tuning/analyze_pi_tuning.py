"""Analyze existing raw 20kHz captures; never connects to the target.

The tone algebra H=I/Iref and L=H/(1-H) follows the companion repository's
closed_current_bode_plot.py. Here fixed tones use least-squares sin/cos/DC fits
instead of chirp Welch estimates. Cross coupling/nonlinearity limit SISO margins.
"""
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


def load(name):
    path = OUT / name
    data = json.loads((path if path.exists() else DATA / name).read_text())
    return data.get("signals", data)


def scalar(data, name):
    return np.asarray(data["g_tune_" + name]["values"])


steps, tones, margins = [], [], []
fig, axs = plt.subplots(2, 2, figsize=(11, 7))
for col, axis in enumerate("DQ"):
    for name, kp, ki, color in [
        (f"current-step-{axis}-05A-kp02-ki200.json", .2, 200, "#aa7017"),
        (f"tune-{axis}-kp08-ki2000.json", .8, 2000, "#1762b5"),
    ]:
        d = load(name)
        t = np.asarray(d["g_tune_measured"]["times"])
        r, y = scalar(d, "reference"), scalar(d, "measured")
        edge = np.flatnonzero(np.diff(r) > .25)[0] + 1
        t = t - t[edge]
        assert np.allclose(np.diff(t), 50e-6, atol=1e-7)
        # Only for noise-tolerant STEP crossing estimates, not tone/Bode analysis.
        smooth = np.convolve(np.pad(y, (1, 1), mode="edge"), np.ones(3)/3, mode="valid")
        valid = np.arange(len(y)) >= edge
        t10 = t[np.flatnonzero(valid & (smooth >= .05))[0]]
        t90 = t[np.flatnonzero(valid & (smooth >= .45))[0]]
        row = dict(axis=axis, kp=kp, ki=ki, rise_10_90_ms=float((t90-t10)*1000),
                   crossing_average_us=150, sample_us=50,
                   final_10ms_mean_A=float(y[-200:].mean()),
                   raw_peak_A=float(y.max()),
                   first_5ms_three_sample_peak_A=float(smooth[(t >= 0) & (t < .005)].max()),
                   saturation_samples=int(np.count_nonzero(scalar(d, "saturated"))))
        steps.append(row)
        label = f"Kp={kp:g}, Ki={ki}: {row['rise_10_90_ms']:.2f} ms rise"
        for ax in axs[:, col]:
            ax.plot(t*1000, y, color=color, lw=.7, alpha=.65, label=label)
    for ax in axs[:, col]:
        ax.step(t*1000, r, where="post", color="#303030", ls="--", lw=1, label="0.5 A reference")
        ax.grid(alpha=.2); ax.set_ylabel("Current (A)")
    axs[0, col].set(title=f"Stationary {axis}: full response", xlim=(-1, 25), ylim=(-.07, .62))
    axs[1, col].set(title=f"Stationary {axis}: first 3 ms", xlim=(-.25, 3), ylim=(-.07, .62))
    axs[1, col].set_xlabel("Time from reference step (ms)")
    axs[0, col].legend(loc="lower right", fontsize=8)

    for frequency in [400, 600, 800]:
        d = load(f"tone-{axis}-{frequency}Hz-kp08-ki2000.json")
        tt = np.asarray(d["g_tune_reference"]["times"])
        use = (tt >= tt[0]+.015) & (tt <= tt[-1]-.005)
        X = np.column_stack([np.ones(use.sum()), np.sin(2*np.pi*frequency*tt[use]),
                             np.cos(2*np.pi*frequency*tt[use])])
        coefficients = {k: np.linalg.lstsq(X, scalar(d, k)[use], rcond=None)[0]
                        for k in ["reference", "measured", "cross_current"]}
        phasor = lambda c: c[2] - 1j*c[1]
        H = phasor(coefficients["measured"])/phasor(coefficients["reference"])
        L = H/(1-H)
        reference_error = scalar(d, "reference")[use] - X@coefficients["reference"]
        assert np.std(reference_error) < 1e-4, "wrong frequency or incomplete tone"
        tones.append(dict(axis=axis, f_Hz=frequency, H_mag=float(abs(H)),
                          H_dB=float(20*np.log10(abs(H))), H_phase_deg=float(np.angle(H, deg=True)),
                          L_mag=float(abs(L)), L_phase_deg=float(np.angle(L, deg=True)),
                          cross_ratio=float(abs(phasor(coefficients["cross_current"])/phasor(coefficients["reference"]))),
                          saturation_samples=int(np.count_nonzero(scalar(d, "saturated")))))
    a, b = [row for row in tones if row["axis"] == axis and row["f_Hz"] in (400, 600)]
    assert a["L_mag"] > 1 > b["L_mag"]
    fraction = -np.log(a["L_mag"])/(np.log(b["L_mag"])-np.log(a["L_mag"]))
    margins.append(dict(axis=axis, interpolation_bracket_Hz=[400, 600],
                        estimated_crossover_Hz=float(np.exp(np.log(400)+fraction*np.log(600/400))),
                        estimated_phase_margin_deg=float(180+a["L_phase_deg"]+fraction*(b["L_phase_deg"]-a["L_phase_deg"]))))

fig.suptitle("PI tuning on the motor — raw 20 kHz Recorder captures", fontsize=14)
fig.text(.5, .01, "0.5 A test; unchanged 0.75 V vector ceiling. No saturation. Step crossing estimates use 3 samples (150 us); tones use raw data.", ha="center", fontsize=8)
fig.tight_layout(rect=(0, .035, 1, .95))
fig.savefig(OUT / "pi-tuned-response.png", dpi=180)
results = dict(steps=steps, tones=tones, margins=margins,
               limits="0.5A test,0.75V ceiling; sparse SISO tone estimates,not full robustness certification; exact -3dB crossing not measured above800Hz")
(ROOT / "docs/bench/2026-11-10-pi-tuning-results.json").write_text(json.dumps(results, indent=2))
print(json.dumps(dict(steps=steps, margins=margins), indent=2))
