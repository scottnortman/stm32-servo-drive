"""Run one bounded experiment through the existing MCUViewer RAM interface.

Requires prior physical USER arming; never arms, flashes or resets the MCU.
Preserves firmware protections. Use only with the matching tuning firmware.
"""
import argparse
import json
import time
from pathlib import Path
import mcuviewer_project as viewer


def values(names):
    response = viewer.rpc("tools/call", {"name": "get_variable_value",
                          "arguments": {"variables": json.dumps(names)}})
    if response.get("isError"):
        raise RuntimeError(response)
    data = json.loads(response["content"][0]["text"])
    if not all(item["ok"] for item in data):
        raise RuntimeError("Live values are stale or unavailable")
    return {item["n"]: item["v"] for item in data}


def run(axis, kp, ki, amplitude=0.5, frequency=None, label=None,
        bench24=False, voltage_limit=0.75, voltage_step=False):
    viewer.connect()
    viewer.tool("stop_acquisition")
    viewer.tool("set_active_group", name="Live current and controls")
    viewer.tool("start_acquisition", wait_for_connection=0)
    time.sleep(0.1)
    before = values(["g_tune_state", "g_tune_reason", "g_tune_apply_seq",
                     "g_tune_start_seq", "g_tune_duration", "g_tune_voltage_limit"])
    if before["g_tune_state"] != 1 or before["g_tune_reason"]:
        raise RuntimeError("Requires USER-armed idle state with no fault")
    max_voltage=2.0 if bench24 else 0.75
    max_frequency=2000 if bench24 else 800
    if before["g_tune_voltage_limit"] > max_voltage+0.001:
        raise RuntimeError(f"Existing voltage ceiling exceeds {max_voltage}V host limit")
    if not 0 < voltage_limit <= max_voltage:
        raise ValueError(f"Requested voltage ceiling must be at most {max_voltage}V")
    if voltage_step and (not bench24 or frequency is not None):
        raise ValueError("Voltage-step precheck is available only on the 24V profile")
    if amplitude > (0.1 if voltage_step else 0.5) or not 0 <= kp <= 2 or not 0 <= ki <= 6000:
        raise ValueError("Host limits: 0.1V voltage step or 0.5A current, Kp2/Ki6000")
    config = {"mode": 0 if voltage_step else 1 if frequency is None else 2, "axis": axis,
              "kp": kp, "ki": ki, "amplitude": amplitude, "duration": 1,
              "voltage_limit": voltage_limit}
    if frequency is not None:
        if not 1 <= frequency <= max_frequency:
            raise ValueError(f"This host profile permits tones only through {max_frequency}Hz")
        config.update(f_start=frequency, f_end=frequency)
    for key, value in config.items():
        viewer.tool("set_variable_value", name="g_tune_" + key, value=value)
    seq = int(before["g_tune_apply_seq"]) + 1
    viewer.tool("set_variable_value", name="g_tune_apply_seq", value=seq)
    time.sleep(0.1)
    status = values(["g_tune_state", "g_tune_reason", "g_tune_result"])
    if status != {"g_tune_state": 1.0, "g_tune_reason": 0.0, "g_tune_result": 0.0}:
        raise RuntimeError(status)
    viewer.tool("stop_acquisition")
    viewer.tool("set_active_group", name="20 kHz current step - five signals")
    viewer.tool("set_recorder_mode", mode="normal")
    viewer.tool("set_recorder_trigger", source_variable="g_tune_reference",
                value=amplitude/2, edge="rising", pre_trigger_samples=200, enabled=1)
    viewer.tool("start_acquisition", wait_for_connection=0)
    viewer.tool("set_variable_value", name="g_tune_start_seq",
                value=int(before["g_tune_start_seq"])+1)
    time.sleep(1.3)  # excitation deadline is one second; no unbounded motion
    data = {}
    for name in viewer.SIGNALS:
        response = viewer.rpc("tools/call", {"name": "get_curve_data",
                              "arguments": {"name": name, "max_points": 2000}})
        if response.get("isError"):
            raise RuntimeError(response)
        data[name] = json.loads(response["content"][0]["text"])
    path = viewer.ROOT / "outputs/tuning" / ((label or f"tune-{axis}-{seq}") + ".json")
    path.write_text(json.dumps({"config": config, "signals": data}, indent=2))
    viewer.tool("stop_acquisition")
    viewer.tool("set_active_group", name="Live current and controls")
    viewer.tool("start_acquisition", wait_for_connection=0)
    time.sleep(0.1)
    after = values(["g_tune_state", "g_tune_reason", "g_tune_result"])
    if after != {"g_tune_state": 1.0, "g_tune_reason": 0.0, "g_tune_result": 0.0}:
        raise RuntimeError(f"Experiment stopped abnormally: {after}; capture: {path}")
    print("CAPTURE", path)
    return data


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--axis", type=int, choices=[0, 1], required=True)
    p.add_argument("--kp", type=float, required=True)
    p.add_argument("--ki", type=float, required=True)
    p.add_argument("--amplitude", type=float, default=0.5)
    p.add_argument("--frequency", type=float)
    p.add_argument("--bench24", action="store_true",
                   help="24V GEMS tuning image, allowing 2kHz tones and <=2V ceiling")
    p.add_argument("--voltage-limit", type=float, default=0.75)
    p.add_argument("--voltage-step", action="store_true",
                   help="0.1V signed-response precheck on the 24V profile")
    p.add_argument("--label", required=True)
    a = p.parse_args()
    run(a.axis, a.kp, a.ki, a.amplitude, a.frequency, a.label,
        a.bench24, a.voltage_limit, a.voltage_step)
