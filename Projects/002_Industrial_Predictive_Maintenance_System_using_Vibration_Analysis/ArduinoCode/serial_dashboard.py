#!/usr/bin/env python3
"""
Predictive Maintenance System — Python Serial Dashboard
========================================================
Reads JSON from Arduino Serial port and displays a live
matplotlib dashboard with X/Y/Z acceleration trends, RMS
vibration history, bearing temperature, and a colour-coded
machine-state status panel.

Usage:
    pip install pyserial matplotlib
    python serial_dashboard.py --port COM3          (Windows)
    python serial_dashboard.py --port /dev/ttyUSB0  (Linux/Mac)
"""

import argparse
import json
import sys
import threading
import time
from collections import deque
from datetime import datetime

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.gridspec import GridSpec

# ── Config ────────────────────────────────────────────────────
BAUD_RATE   = 9600
HISTORY_LEN = 200   # 200 samples × ~500 ms window each ≈ ~100 s of history

# State → colour mapping (matplotlib colour strings)
STATE_COLOURS = {
    "NORMAL":   "#3fb950",
    "WARNING":  "#e3b341",
    "CRITICAL": "#f85149",
}

# ── Data buffers ──────────────────────────────────────────────
ax_buf    = deque(maxlen=HISTORY_LEN)
ay_buf    = deque(maxlen=HISTORY_LEN)
az_buf    = deque(maxlen=HISTORY_LEN)
rms_buf   = deque(maxlen=HISTORY_LEN)
temp_buf  = deque(maxlen=HISTORY_LEN)
latest    = {}
lock      = threading.Lock()


def serial_reader(port: str) -> None:
    """Background thread: reads JSON lines from Arduino and updates buffers."""
    global latest
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=2)
        print(f"[✓] Connected to {port} @ {BAUD_RATE} baud")
    except serial.SerialException as e:
        print(f"[✗] Cannot open {port}: {e}")
        sys.exit(1)

    while True:
        try:
            raw = ser.readline().decode("utf-8", errors="ignore").strip()
        except serial.SerialException as e:
            print(f"[✗] Serial read error: {e}")
            time.sleep(1)
            continue

        if not raw.startswith("{"):
            continue  # skip comment lines (start with #)

        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            continue

        with lock:
            latest = data
            ax_buf.append(data.get("ax", 0.0))
            ay_buf.append(data.get("ay", 0.0))
            az_buf.append(data.get("az", 0.0))
            rms_buf.append(data.get("rms", 0.0))
            temp_buf.append(data.get("temp", 25.0))

        state      = data.get("state", "NORMAL")
        state_sym  = {"NORMAL": "●", "WARNING": "▲", "CRITICAL": "✖"}.get(state, "?")
        ts         = datetime.now().strftime("%H:%M:%S")
        print(
            f"[{ts}] {state_sym} {state:<8}  "
            f"RMS:{data.get('rms', 0):.3f} m/s²  "
            f"ax:{data.get('ax', 0):+.2f}  "
            f"ay:{data.get('ay', 0):+.2f}  "
            f"az:{data.get('az', 0):+.2f}  "
            f"T:{data.get('temp', 0):.1f}°C"
        )


def build_dashboard(port: str) -> None:
    """Build and animate the live matplotlib dashboard."""
    fig = plt.figure(figsize=(15, 8), facecolor="#0d1117")
    fig.suptitle(
        "Project 002 — Industrial Predictive Maintenance Dashboard",
        color="#58a6ff", fontsize=15, fontweight="bold",
    )

    gs = GridSpec(3, 3, figure=fig, hspace=0.55, wspace=0.35)
    ax_acc   = fig.add_subplot(gs[0, :2])   # X/Y/Z acceleration
    ax_rms   = fig.add_subplot(gs[1, :2])   # RMS trend
    ax_temp  = fig.add_subplot(gs[2, :2])   # Bearing temperature
    ax_gauge = fig.add_subplot(gs[0, 2])    # RMS bar gauge
    ax_state = fig.add_subplot(gs[1:, 2])   # Machine state panel

    for ax in [ax_acc, ax_rms, ax_temp, ax_gauge, ax_state]:
        ax.set_facecolor("#161b22")
        for spine in ax.spines.values():
            spine.set_edgecolor("#30363d")

    def animate(_frame: int) -> None:
        with lock:
            xs       = list(range(len(rms_buf)))
            _ax      = list(ax_buf)
            _ay      = list(ay_buf)
            _az      = list(az_buf)
            _rms     = list(rms_buf)
            _temp    = list(temp_buf)
            snap     = dict(latest)

        state       = snap.get("state", "NORMAL")
        state_col   = STATE_COLOURS.get(state, "#8b949e")
        cur_rms     = snap.get("rms", 0.0)
        cur_temp    = snap.get("temp", 25.0)

        # ── X/Y/Z Acceleration trend ─────────────────────────
        ax_acc.cla()
        ax_acc.set_facecolor("#161b22")
        ax_acc.set_title("Acceleration X / Y / Z  (m/s²)", color="#8b949e", fontsize=10)
        if xs:
            ax_acc.plot(xs, _ax, color="#79c0ff", linewidth=1.5, label="X")
            ax_acc.plot(xs, _ay, color="#3fb950", linewidth=1.5, label="Y")
            ax_acc.plot(xs, _az, color="#ffa657", linewidth=1.5, label="Z")
        ax_acc.axhline(0, color="#30363d", linewidth=0.8)
        ax_acc.tick_params(colors="#8b949e", labelsize=8)
        ax_acc.legend(facecolor="#161b22", labelcolor="#8b949e",
                      fontsize=8, loc="upper left")
        for spine in ax_acc.spines.values():
            spine.set_edgecolor("#30363d")

        # ── RMS trend with threshold bands ───────────────────
        ax_rms.cla()
        ax_rms.set_facecolor("#161b22")
        ax_rms.set_title("Vibration RMS  (m/s²)", color="#8b949e", fontsize=10)
        ax_rms.axhspan(0,          3.0,  alpha=0.12, color="#3fb950")   # NORMAL zone
        ax_rms.axhspan(3.0,        7.0,  alpha=0.10, color="#e3b341")   # WARNING zone
        ax_rms.axhspan(7.0,        15.0, alpha=0.10, color="#f85149")   # CRITICAL zone
        ax_rms.axhline(3.0, color="#e3b341", linewidth=1.0, linestyle="--")
        ax_rms.axhline(7.0, color="#f85149", linewidth=1.0, linestyle="--")
        if xs:
            rms_colors = [STATE_COLOURS.get(
                "CRITICAL" if v >= 7.0 else "WARNING" if v >= 3.0 else "NORMAL",
                "#3fb950") for v in _rms]
            ax_rms.plot(xs, _rms, color=state_col, linewidth=2, label="RMS")
            ax_rms.fill_between(xs, _rms, alpha=0.2, color=state_col)
        ax_rms.set_ylim(0, max(15.0, max(_rms) * 1.2) if _rms else 15.0)
        ax_rms.tick_params(colors="#8b949e", labelsize=8)
        ax_rms.legend(facecolor="#161b22", labelcolor="#8b949e",
                      fontsize=8, loc="upper left")
        for spine in ax_rms.spines.values():
            spine.set_edgecolor("#30363d")

        # ── Bearing temperature trend ─────────────────────────
        ax_temp.cla()
        ax_temp.set_facecolor("#161b22")
        ax_temp.set_title("Bearing Temperature  (°C)", color="#8b949e", fontsize=10)
        ax_temp.axhline(80.0,  color="#e3b341", linewidth=1.0,
                        linestyle="--", label="Warning 80°C")
        ax_temp.axhline(100.0, color="#f85149", linewidth=1.0,
                        linestyle="--", label="Critical 100°C")
        if xs:
            ax_temp.plot(xs, _temp, color="#ff7b72", linewidth=2, label="Temp °C")
            ax_temp.fill_between(xs, _temp, alpha=0.15, color="#ff7b72")
        ax_temp.tick_params(colors="#8b949e", labelsize=8)
        ax_temp.legend(facecolor="#161b22", labelcolor="#8b949e",
                       fontsize=8, loc="upper left")
        for spine in ax_temp.spines.values():
            spine.set_edgecolor("#30363d")

        # ── RMS Gauge bar ─────────────────────────────────────
        ax_gauge.cla()
        ax_gauge.set_facecolor("#161b22")
        ax_gauge.set_title("Current RMS", color="#8b949e", fontsize=10)
        ax_gauge.barh(0, 15.0,   color="#21262d", height=0.5)
        ax_gauge.barh(0, cur_rms, color=state_col, height=0.5)
        ax_gauge.set_xlim(0, 15.0)
        ax_gauge.set_yticks([])
        ax_gauge.text(
            7.5, 0, f"{cur_rms:.3f} m/s²",
            ha="center", va="center",
            color="white", fontsize=13, fontweight="bold",
        )
        ax_gauge.tick_params(colors="#8b949e", labelsize=8)
        for spine in ax_gauge.spines.values():
            spine.set_edgecolor("#30363d")

        # ── Machine State panel ───────────────────────────────
        ax_state.cla()
        ax_state.set_facecolor("#161b22")
        ax_state.axis("off")
        for spine in ax_state.spines.values():
            spine.set_edgecolor("#30363d")

        state_icons = {"NORMAL": "✔", "WARNING": "⚠", "CRITICAL": "✖"}
        icon = state_icons.get(state, "?")

        ax_state.text(0.5, 0.90, "MACHINE STATE",
                      ha="center", color="#8b949e", fontsize=10,
                      fontweight="bold", transform=ax_state.transAxes)
        ax_state.text(0.5, 0.70, f"{icon}  {state}",
                      ha="center", color=state_col, fontsize=18,
                      fontweight="bold", transform=ax_state.transAxes)
        ax_state.text(0.5, 0.55, f"RMS: {cur_rms:.3f} m/s²",
                      ha="center", color="#c9d1d9", fontsize=10,
                      transform=ax_state.transAxes)
        ax_state.text(0.5, 0.43, f"Temp: {cur_temp:.1f} °C",
                      ha="center", color="#c9d1d9", fontsize=10,
                      transform=ax_state.transAxes)

        # Threshold reference
        ax_state.text(0.5, 0.28, "Thresholds",
                      ha="center", color="#6e7681", fontsize=9,
                      transform=ax_state.transAxes)
        ax_state.text(0.5, 0.20, "Normal  < 3.0 m/s²",
                      ha="center", color="#3fb950", fontsize=8,
                      transform=ax_state.transAxes)
        ax_state.text(0.5, 0.13, "Warning  3–7 m/s²",
                      ha="center", color="#e3b341", fontsize=8,
                      transform=ax_state.transAxes)
        ax_state.text(0.5, 0.06, "Critical ≥ 7.0 m/s²",
                      ha="center", color="#f85149", fontsize=8,
                      transform=ax_state.transAxes)

    ani = animation.FuncAnimation(fig, animate, interval=600, cache_frame_data=False)

    # Start serial reader in background thread
    t = threading.Thread(target=serial_reader, args=(port,), daemon=True)
    t.start()

    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Predictive Maintenance System — Live Serial Dashboard"
    )
    parser.add_argument(
        "--port", default="COM3",
        help="Serial port (e.g. COM3  or  /dev/ttyUSB0)",
    )
    args = parser.parse_args()
    build_dashboard(args.port)
