#!/usr/bin/env python3
"""
Smart Irrigation System — Python Serial Dashboard
==================================================
Reads JSON from Arduino Serial port and displays
a live terminal dashboard with matplotlib graph.

Usage:
    pip install pyserial matplotlib
    python serial_dashboard.py --port COM3        (Windows)
    python serial_dashboard.py --port /dev/ttyUSB0 (Linux/Mac)
"""

import argparse
import json
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
HISTORY_LEN = 60   # data points to keep (60 × 2s = 2 min window)

# ── Data buffers ──────────────────────────────────────────────
timestamps = deque(maxlen=HISTORY_LEN)
moisture   = deque(maxlen=HISTORY_LEN)
temp       = deque(maxlen=HISTORY_LEN)
humidity   = deque(maxlen=HISTORY_LEN)
pump_state = deque(maxlen=HISTORY_LEN)
latest     = {}

def serial_reader(port: str):
    """Background thread: reads JSON lines from Arduino."""
    global latest
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=2)
        print(f"[✓] Connected to {port} @ {BAUD_RATE} baud")
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if line.startswith("{"):
                try:
                    data = json.loads(line)
                    latest = data
                    now = datetime.now().strftime("%H:%M:%S")
                    timestamps.append(now)
                    moisture.append(data.get("moisture", 0))
                    temp.append(data.get("temp", 0))
                    humidity.append(data.get("humidity", 0))
                    pump_state.append(1 if data.get("pump") else 0)
                    print(f"[{now}] M:{data.get('moisture')}%  "
                          f"T:{data.get('temp')}°C  "
                          f"H:{data.get('humidity')}%  "
                          f"Pump:{'ON ' if data.get('pump') else 'OFF'}  "
                          f"→ {data.get('reason','')}")
                except json.JSONDecodeError:
                    pass
    except serial.SerialException as e:
        print(f"[✗] Serial error: {e}")

def build_dashboard(port: str):
    """Build and animate the live matplotlib dashboard."""
    fig = plt.figure(figsize=(14, 8), facecolor="#0d1117")
    fig.suptitle("Smart Irrigation System — Live Dashboard",
                 color="#58a6ff", fontsize=16, fontweight="bold")

    gs = GridSpec(2, 3, figure=fig, hspace=0.45, wspace=0.35)
    ax_moisture = fig.add_subplot(gs[0, :2])
    ax_temp_hum = fig.add_subplot(gs[1, :2])
    ax_gauge_m  = fig.add_subplot(gs[0, 2])
    ax_status   = fig.add_subplot(gs[1, 2])

    for ax in [ax_moisture, ax_temp_hum, ax_gauge_m, ax_status]:
        ax.set_facecolor("#161b22")
        for spine in ax.spines.values():
            spine.set_edgecolor("#30363d")

    def animate(_frame):
        xs = list(range(len(moisture)))

        # ── Moisture trend ───────────────────────────────────
        ax_moisture.cla()
        ax_moisture.set_facecolor("#161b22")
        ax_moisture.set_title("Soil Moisture (%)", color="#8b949e", fontsize=10)
        ax_moisture.set_ylim(0, 100)
        ax_moisture.axhspan(0,  30, alpha=0.15, color="#f85149")   # critical
        ax_moisture.axhspan(30, 70, alpha=0.10, color="#3fb950")   # optimal
        ax_moisture.axhspan(70,100, alpha=0.10, color="#58a6ff")   # saturated
        if xs:
            ax_moisture.plot(xs, list(moisture), color="#3fb950",
                             linewidth=2, label="Moisture")
            ax_moisture.fill_between(xs, list(moisture),
                                     alpha=0.2, color="#3fb950")
        ax_moisture.tick_params(colors="#8b949e")
        ax_moisture.legend(facecolor="#161b22", labelcolor="#8b949e")

        # ── Temp & Humidity trend ────────────────────────────
        ax_temp_hum.cla()
        ax_temp_hum.set_facecolor("#161b22")
        ax_temp_hum.set_title("Temperature (°C) & Humidity (%)",
                               color="#8b949e", fontsize=10)
        if xs:
            ax_temp_hum.plot(xs, list(temp), color="#f0883e",
                             linewidth=2, label="Temp °C")
            ax_temp_hum.plot(xs, list(humidity), color="#79c0ff",
                             linewidth=2, label="Humidity %")
        ax_temp_hum.tick_params(colors="#8b949e")
        ax_temp_hum.legend(facecolor="#161b22", labelcolor="#8b949e")

        # ── Current moisture gauge ───────────────────────────
        ax_gauge_m.cla()
        ax_gauge_m.set_facecolor("#161b22")
        ax_gauge_m.set_title("Current Moisture", color="#8b949e", fontsize=10)
        val = latest.get("moisture", 0)
        color = ("#f85149" if val < 30 else
                 "#3fb950" if val < 70 else "#58a6ff")
        ax_gauge_m.barh(0, val, color=color, height=0.4)
        ax_gauge_m.barh(0, 100, color="#21262d", height=0.4)
        ax_gauge_m.barh(0, val, color=color, height=0.4)
        ax_gauge_m.set_xlim(0, 100)
        ax_gauge_m.set_yticks([])
        ax_gauge_m.text(50, 0.35, f"{val:.1f}%",
                        ha="center", va="center",
                        color="white", fontsize=18, fontweight="bold")
        ax_gauge_m.tick_params(colors="#8b949e")

        # ── Status panel ─────────────────────────────────────
        ax_status.cla()
        ax_status.set_facecolor("#161b22")
        ax_status.axis("off")
        pump_on = latest.get("pump", False)
        pump_color  = "#3fb950" if pump_on  else "#f85149"
        pump_symbol = "● ON"    if pump_on  else "○ OFF"
        ax_status.text(0.5, 0.80, "PUMP STATUS",
                       ha="center", color="#8b949e", fontsize=10,
                       transform=ax_status.transAxes)
        ax_status.text(0.5, 0.58, pump_symbol,
                       ha="center", color=pump_color, fontsize=22,
                       fontweight="bold", transform=ax_status.transAxes)
        ax_status.text(0.5, 0.35,
                       latest.get("reason", "—"),
                       ha="center", color="#c9d1d9", fontsize=8,
                       wrap=True, transform=ax_status.transAxes)
        ax_status.text(0.5, 0.12,
                       f"T:{latest.get('temp','—')}°C  "
                       f"H:{latest.get('humidity','—')}%",
                       ha="center", color="#8b949e", fontsize=9,
                       transform=ax_status.transAxes)

    ani = animation.FuncAnimation(fig, animate, interval=2000)

    # Start serial in background thread
    t = threading.Thread(target=serial_reader, args=(port,), daemon=True)
    t.start()

    plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Smart Irrigation Dashboard")
    parser.add_argument("--port", default="COM3",
                        help="Serial port (e.g. COM3 or /dev/ttyUSB0)")
    args = parser.parse_args()
    build_dashboard(args.port)
