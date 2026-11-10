"""Prepare MCUViewer FREE through its documented localhost MCP server.

Only creates/configures the GUI project. Never flashes, starts acquisition,
writes target RAM, or energizes the motor. Run with MCUViewer already open.
"""
import json
import argparse
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
URL = "http://localhost:8765/mcp"
HEADERS = {"Accept": "application/json, text/event-stream", "Content-Type": "application/json"}
SERIAL = "066DFF514988525067173049"
SIGNALS = ["g_tune_reference", "g_tune_measured", "g_tune_cross_current",
           "g_tune_voltage", "g_tune_saturated"]
PARAMETERS = {
    "mode": (0, 3), "axis": (0, 1), "amplitude": (0, 6.25),
    "kp": (0, 10), "ki": (0, 10000), "f_start": (1, 800),
    "f_end": (1, 800), "duration": (0.1, 10), "voltage_limit": (0.001, 4),
}


def rpc(method, params=None):
    payload = {"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}
    with urllib.request.urlopen(urllib.request.Request(
            URL, data=json.dumps(payload).encode(), headers=HEADERS), timeout=20) as response:
        if "Mcp-Session-Id" in response.headers:
            HEADERS["Mcp-Session-Id"] = response.headers["Mcp-Session-Id"]
        result = json.load(response)
    if "error" in result:
        raise RuntimeError(result["error"])
    return result.get("result", {})


def tool(tool_name, **arguments):
    result = rpc("tools/call", {"name": tool_name, "arguments": arguments})
    if result.get("isError"):
        raise RuntimeError(result)
    print(tool_name, json.dumps(result, ensure_ascii=True)[:1200])
    return result


def connect():
    HEADERS.pop("Mcp-Session-Id", None)
    rpc("initialize", {"protocolVersion": "2025-03-26", "capabilities": {},
                       "clientInfo": {"name": "servo-tuning-setup", "version": "1"}})
    notification = {"jsonrpc": "2.0", "method": "notifications/initialized"}
    with urllib.request.urlopen(urllib.request.Request(
            URL, data=json.dumps(notification).encode(), headers=HEADERS), timeout=10):
        pass


def main(nv=False, elf=None):
    connect()
    # This script intentionally starts from an empty project. Save other work first.
    tool("new_project")
    tool("set_elf_path", path=str(elf or ROOT / "build-bringup/repo-current-tuning.elf"), relative=1)
    tool("set_elf_parser", parser="gdb")
    tool("set_debug_probe", type="stlink", serial_number=SERIAL,
         speed_khz=1000, mode="normal", keep_connection=0)
    tool("set_sampling", sample_frequency_hz=100, viewport_width_ms=2000)
    tool("set_recorder_enabled", enabled=1)
    # FREE has 20 live slots. The save view swaps the two chirp-frequency
    # requests for a save command and its read-only status.
    parameters = {name: limits for name, limits in PARAMETERS.items()
                  if not nv or name not in ("f_start", "f_end")}
    variables = [{"name": "g_tune_" + name, "type": "f32"} for name in parameters]
    variables += [{"name": "g_tune_" + name + "_seq", "type": "u32"}
                  for name in ["apply", "start", "stop"]]
    variables += [{"name": name, "type": "u32" if name.endswith("saturated") else "f32"}
                  for name in SIGNALS]
    variables += [{"name": "g_tune_" + name, "type": "u32"} for name in ["state", "reason", "result"]]
    if nv:
        variables += [{"name": "g_tune_nv_" + name, "type": "u32"}
                      for name in ["save_seq", "status"]]
    assert len(variables) == 20 and len(SIGNALS) == 5
    tool("add_variable", variables=json.dumps(variables))
    for name, limits in parameters.items():
        tool("configure_variable", name="g_tune_" + name,
             write_limits_enabled=1, write_min=limits[0], write_max=limits[1])
    tool("add_group", name="Live current and controls", type="sampling")
    tool("add_plot", group="Live current and controls", name="Current (A)")
    tool("add_variable_to_plot", group="Live current and controls", plot="Current (A)",
         variables=json.dumps(SIGNALS[:3]))
    tool("add_plot", group="Live current and controls", name="Voltage (V) and saturation")
    tool("add_variable_to_plot", group="Live current and controls", plot="Voltage (V) and saturation",
         variables=json.dumps(SIGNALS[3:]))
    tool("add_plot", group="Live current and controls", name="Parameters and commands", type="table")
    tool("add_variable_to_plot", group="Live current and controls", plot="Parameters and commands",
         variables=json.dumps([v["name"] for v in variables if v["name"] not in SIGNALS]))
    tool("add_group", name="20 kHz current step - five signals", type="recorder")
    tool("add_plot", group="20 kHz current step - five signals", name="Step response (A)")
    tool("add_variable_to_plot", group="20 kHz current step - five signals", plot="Step response (A)",
         variables=json.dumps(SIGNALS[:3]))
    tool("add_plot", group="20 kHz current step - five signals", name="Control voltage (V) and saturation")
    tool("add_variable_to_plot", group="20 kHz current step - five signals", plot="Control voltage (V) and saturation",
         variables=json.dumps(SIGNALS[3:]))
    tool("set_active_group", name="20 kHz current step - five signals")
    tool("set_recorder_downsampling", skipped_samples=0)
    tool("set_recorder_trigger", source_variable=SIGNALS[0], edge="rising", value=0.25,
         pre_trigger_samples=200, enabled=1)
    tool("set_recorder_mode", mode="normal")
    tool("refresh_variable_addresses")
    name = "current-loop-free-nv.mcuv" if nv else "current-loop-free.mcuv"
    tool("save_project_as", path=str(ROOT / "tools/tuning" / name))
    tool("list_groups")
    tool("get_recorder_settings")


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nv",action="store_true",
                        help="20-slot gain-save view; use only with the matching new image")
    parser.add_argument("--elf",type=Path,
                        help="ELF matching the flashed image (default: build-bringup/repo-current-tuning.elf)")
    args=parser.parse_args()
    main(args.nv,args.elf.resolve() if args.elf else None)
