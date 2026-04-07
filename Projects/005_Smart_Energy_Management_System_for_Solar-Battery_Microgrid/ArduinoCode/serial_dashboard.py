#!/usr/bin/env python3
"""
serial_dashboard.py – Live dashboard for Project 005
Smart Energy Management System for Solar-Battery Microgrid

Usage:
    python serial_dashboard.py --port COM3          (Windows)
    python serial_dashboard.py --port /dev/ttyUSB0  (Linux/Mac)

Reads JSON lines from Arduino and renders:
  • Solar power trend (120-point rolling buffer ≈ 20 minutes)
  • Battery SOC gauge with colour coding
  • Load power trend
  • MPPT duty-cycle trend
  • Energy-flow status panel
"""

import argparse
import json
import sys
import threading
import collections
from datetime import datetime

import serial
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.gridspec as gridspec
from matplotlib.animation import FuncAnimation

# ── Argument parsing ──────────────────────────────────────────
parser = argparse.ArgumentParser(
    description="Live dashboard for Solar Microgrid (Project 005)"
)
parser.add_argument(
    "--port", default="/dev/ttyUSB0",
    help="Serial port where the Arduino is connected (default: /dev/ttyUSB0)"
)
parser.add_argument(
    "--baud", type=int, default=115200,
    help="Baud rate (default: 115200)"
)
args = parser.parse_args()

# ── Rolling data buffers (120 × 10 s ≈ 20 min) ───────────────
BUFFER = 120
solar_w   = collections.deque([0.0] * BUFFER, maxlen=BUFFER)
batt_soc  = collections.deque([0.0] * BUFFER, maxlen=BUFFER)
load_w    = collections.deque([0.0] * BUFFER, maxlen=BUFFER)
mppt_duty = collections.deque([0.0] * BUFFER, maxlen=BUFFER)
timestamps = collections.deque([""] * BUFFER, maxlen=BUFFER)

latest = {
    "solar_v": 0.0, "solar_i": 0.0, "solar_w": 0.0,
    "batt_v":  0.0, "batt_soc": 0,
    "batt_i":  0.0, "load_w": 0.0,
    "mppt_duty": 0, "state": "---",
    "loads": [0, 0, 0, 0],
}
lock = threading.Lock()

# ── Serial reader thread ──────────────────────────────────────
def serial_reader():
    """Runs in background; parses JSON from the serial port."""
    try:
        ser = serial.Serial(args.port, args.baud, timeout=2)
    except serial.SerialException as exc:
        print(f"[ERROR] Cannot open {args.port}: {exc}", file=sys.stderr)
        sys.exit(1)

    print(f"[INFO] Connected to {args.port} @ {args.baud} baud")
    while True:
        try:
            raw = ser.readline().decode("utf-8", errors="replace").strip()
            if not raw.startswith("{"):
                continue
            data = json.loads(raw)
            ts = datetime.now().strftime("%H:%M:%S")
            with lock:
                latest.update(data)
                solar_w.append(data.get("solar_w", 0))
                batt_soc.append(data.get("batt_soc", 0))
                load_w.append(data.get("load_w", 0))
                mppt_duty.append(data.get("mppt_duty", 0))
                timestamps.append(ts)
        except (json.JSONDecodeError, UnicodeDecodeError):
            pass
        except serial.SerialException:
            print("[WARN] Serial disconnected.", file=sys.stderr)
            break

reader_thread = threading.Thread(target=serial_reader, daemon=True)
reader_thread.start()

# ── Matplotlib setup ──────────────────────────────────────────
matplotlib.rcParams.update({
    "axes.facecolor":   "#1e1e2e",
    "figure.facecolor": "#0d1117",
    "axes.edgecolor":   "#444",
    "text.color":       "#cdd6f4",
    "axes.labelcolor":  "#cdd6f4",
    "xtick.color":      "#888",
    "ytick.color":      "#888",
    "grid.color":       "#2a2a3e",
    "lines.linewidth":  1.8,
})

fig = plt.figure(figsize=(14, 8))
fig.suptitle("☀  Solar-Battery Microgrid Dashboard  ⚡", fontsize=14,
             color="#cba6f7", fontweight="bold", y=0.98)

gs = gridspec.GridSpec(3, 3, figure=fig, hspace=0.48, wspace=0.35)

ax_solar  = fig.add_subplot(gs[0, :2])   # Solar power trend (wide)
ax_soc    = fig.add_subplot(gs[1, :2])   # Battery SOC trend
ax_load   = fig.add_subplot(gs[2, 0])    # Load power
ax_mppt   = fig.add_subplot(gs[2, 1])    # MPPT duty
ax_status = fig.add_subplot(gs[:, 2])    # Status panel

x_axis = list(range(BUFFER))

def soc_color(soc):
    if soc >= 50: return "#a6e3a1"   # green
    if soc >= 20: return "#f9e2af"   # yellow
    return "#f38ba8"                  # red

def update(frame):
    with lock:
        sw  = list(solar_w)
        bs  = list(batt_soc)
        lw  = list(load_w)
        md  = list(mppt_duty)
        d   = dict(latest)

    cur_soc = d["batt_soc"]
    col     = soc_color(cur_soc)

    # ── Solar power ───────────────────────────────────────────
    ax_solar.cla()
    ax_solar.plot(x_axis, sw, color="#fab387", label="Solar W")
    ax_solar.fill_between(x_axis, sw, alpha=0.2, color="#fab387")
    ax_solar.set_title("Solar Power (W)", fontsize=9)
    ax_solar.set_ylim(0, max(max(sw) * 1.2, 5))
    ax_solar.set_xlim(0, BUFFER - 1)
    ax_solar.set_ylabel("Watts")
    ax_solar.grid(True, linestyle="--", alpha=0.4)

    # ── Battery SOC ───────────────────────────────────────────
    ax_soc.cla()
    ax_soc.plot(x_axis, bs, color=col, label="SOC %")
    ax_soc.fill_between(x_axis, bs, alpha=0.2, color=col)
    ax_soc.axhline(30, color="#f38ba8", linewidth=0.8,
                   linestyle="--", label="30% threshold")
    ax_soc.axhline(15, color="#f38ba8", linewidth=1.2,
                   linestyle=":", label="15% critical")
    ax_soc.set_title(f"Battery SOC (%)  ← now {cur_soc}%", fontsize=9)
    ax_soc.set_ylim(0, 105)
    ax_soc.set_xlim(0, BUFFER - 1)
    ax_soc.set_ylabel("SOC %")
    ax_soc.legend(fontsize=7, loc="lower left")
    ax_soc.grid(True, linestyle="--", alpha=0.4)

    # ── Load power ────────────────────────────────────────────
    ax_load.cla()
    ax_load.plot(x_axis, lw, color="#89dceb")
    ax_load.fill_between(x_axis, lw, alpha=0.2, color="#89dceb")
    ax_load.set_title("Load Power (W)", fontsize=9)
    ax_load.set_ylim(0, max(max(lw) * 1.2, 5))
    ax_load.set_xlim(0, BUFFER - 1)
    ax_load.grid(True, linestyle="--", alpha=0.4)

    # ── MPPT duty ─────────────────────────────────────────────
    ax_mppt.cla()
    ax_mppt.plot(x_axis, md, color="#cba6f7")
    ax_mppt.fill_between(x_axis, md, alpha=0.2, color="#cba6f7")
    ax_mppt.set_title("MPPT Duty (%)", fontsize=9)
    ax_mppt.set_ylim(0, 105)
    ax_mppt.set_xlim(0, BUFFER - 1)
    ax_mppt.grid(True, linestyle="--", alpha=0.4)

    # ── Status panel ─────────────────────────────────────────
    ax_status.cla()
    ax_status.set_facecolor("#1e1e2e")
    ax_status.set_xlim(0, 10)
    ax_status.set_ylim(0, 20)
    ax_status.axis("off")

    loads = d.get("loads", [0, 0, 0, 0])
    load_str = "".join(["■" if l else "□" for l in loads])

    lines = [
        ("⚡ Energy Flow", "#cba6f7", 11, 19.2),
        ("", "#888", 8, 18.5),
        # ASCII energy flow
        ("  ☀ Solar Panel", "#fab387", 8, 17.5),
        ("      │", "#888", 8, 16.9),
        ("      ▼", "#888", 8, 16.5),
        ("  ⚙ MPPT Conv.", "#f9e2af", 8, 15.8),
        ("      │", "#888", 8, 15.2),
        ("      ▼", "#888", 8, 14.8),
        ("  🔋 Battery", "#a6e3a1", 8, 14.1),
        ("      │", "#888", 8, 13.5),
        ("      ▼", "#888", 8, 13.1),
        ("  🔌 Loads", "#89dceb", 8, 12.4),
    ]

    for (text, colour, x, y) in lines:
        ax_status.text(x/10*9, y, text, color=colour,
                       fontsize=8, ha="center", va="center",
                       fontfamily="monospace")

    # Metrics block
    metrics = [
        (f"Solar:  {d['solar_v']:.1f}V  {d['solar_i']:.2f}A  {d['solar_w']:.1f}W", "#fab387"),
        (f"Battery:{d['batt_v']:.2f}V  {d['batt_soc']:3d}%  {d['batt_i']:+.2f}A", col),
        (f"Load:   {d['load_w']:.1f}W", "#89dceb"),
        (f"MPPT duty: {d['mppt_duty']:3d}%", "#cba6f7"),
        (f"State: {d['state']}", "#f9e2af"),
        (f"Loads: {load_str}", "#888"),
    ]
    for idx, (text, colour) in enumerate(metrics):
        ax_status.text(0.5, 10.8 - idx * 1.35, text, color=colour,
                       fontsize=7.5, ha="left", va="center",
                       fontfamily="monospace")

    # SOC indicator bar
    bar_w = 8 * (cur_soc / 100)
    ax_status.add_patch(mpatches.Rectangle((1, 1.2), 8, 0.7,
                        color="#333", zorder=2))
    ax_status.add_patch(mpatches.Rectangle((1, 1.2), bar_w, 0.7,
                        color=col, zorder=3))
    ax_status.text(5, 0.7, f"SOC: {cur_soc}%", color=col,
                   fontsize=9, ha="center", fontweight="bold")

ani = FuncAnimation(fig, update, interval=500, cache_frame_data=False)

plt.tight_layout(rect=[0, 0, 1, 0.96])
plt.show()
