#!/usr/bin/env python3
"""
serial_dashboard.py — Live Serial Dashboard for Project 004
============================================================
Autonomous Line-Following Robot with Obstacle Avoidance and PID Control

Connects to the Arduino over serial, parses JSON telemetry packets, and
renders a live 2×2 matplotlib dashboard:
  ┌─────────────────────┬─────────────────────┐
  │  PID Error Trend    │  Motor Speeds        │
  ├─────────────────────┼─────────────────────┤
  │  Obstacle Distance  │  Sensor Bar (S0–S4)  │
  └─────────────────────┴─────────────────────┘
A state label is shown in the figure title.

Usage
-----
    pip install pyserial matplotlib
    python serial_dashboard.py --port /dev/ttyUSB0 --baud 9600

    # Windows example:
    python serial_dashboard.py --port COM3 --baud 9600
"""

import argparse
import json
import threading
import time
from collections import deque

import matplotlib
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# ── pyserial import ─────────────────────────────────────────
try:
    import serial
except ImportError:
    raise SystemExit("pyserial not found. Run: pip install pyserial")

# ── Constants ────────────────────────────────────────────────
HISTORY_LEN = 100  # Number of data points kept in rolling buffers

# ── Rolling Data Buffers ─────────────────────────────────────
timestamps  = deque(maxlen=HISTORY_LEN)
pid_error   = deque(maxlen=HISTORY_LEN)
left_spd    = deque(maxlen=HISTORY_LEN)
right_spd   = deque(maxlen=HISTORY_LEN)
distance    = deque(maxlen=HISTORY_LEN)

# Latest packet values (shared with animation thread)
_lock          = threading.Lock()
_latest        = {
    "sensors":   [0, 0, 0, 0, 0],
    "state":     "—",
    "dist":      999.0,
    "error":     0.0,
    "pid_out":   0.0,
    "left_spd":  0,
    "right_spd": 0,
}
_serial_running = True

# ── State colour map ─────────────────────────────────────────
STATE_COLORS = {
    "LINE_FOLLOW":       "#58a6ff",
    "OBSTACLE_DETECTED": "#f85149",
    "AVOID_LEFT":        "#ffa657",
    "AVOID_RIGHT":       "#ffa657",
    "SEARCH":            "#e3b341",
}


# ============================================================
#  serial_reader(port, baud)
#  Background thread: reads JSON lines from serial port and
#  updates the global deques and _latest dict.
# ============================================================
def serial_reader(port: str, baud: int) -> None:
    global _serial_running
    try:
        ser = serial.Serial(port, baud, timeout=2)
        print(f"[serial] Connected to {port} @ {baud} baud")
    except serial.SerialException as exc:
        print(f"[serial] ERROR: {exc}")
        _serial_running = False
        return

    t0 = time.time()
    while _serial_running:
        try:
            raw = ser.readline().decode("utf-8", errors="replace").strip()
            if not raw.startswith("{"):
                continue
            packet = json.loads(raw)
        except (json.JSONDecodeError, serial.SerialException):
            continue

        t = time.time() - t0
        err = float(packet.get("error", 0))

        with _lock:
            timestamps.append(t)
            pid_error.append(err)
            left_spd.append(int(packet.get("left_spd", 0)))
            right_spd.append(int(packet.get("right_spd", 0)))
            distance.append(float(packet.get("dist", 999)))

            _latest["sensors"]   = packet.get("sensors",   [0, 0, 0, 0, 0])
            _latest["state"]     = packet.get("state",     "—")
            _latest["dist"]      = float(packet.get("dist",      999))
            _latest["error"]     = err
            _latest["pid_out"]   = float(packet.get("pid_out",   0))
            _latest["left_spd"]  = int(packet.get("left_spd",    0))
            _latest["right_spd"] = int(packet.get("right_spd",   0))

    ser.close()


# ============================================================
#  Build Figure
# ============================================================
fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(
    2, 2, figsize=(12, 7),
    facecolor="#0d1117",
)
fig.patch.set_facecolor("#0d1117")

for ax in (ax1, ax2, ax3, ax4):
    ax.set_facecolor("#161b22")
    ax.tick_params(colors="#c9d1d9", labelsize=8)
    for spine in ax.spines.values():
        spine.set_edgecolor("#30363d")

# ── ax1: PID Error ──────────────────────────────────────────
(line_err,) = ax1.plot([], [], color="#58a6ff", linewidth=1.5)
ax1.axhline(0, color="#3fb950", linewidth=0.8, linestyle="--")
ax1.set_title("PID Error", color="#c9d1d9", fontsize=10)
ax1.set_ylabel("Error", color="#8b949e", fontsize=8)
ax1.set_xlabel("Time (s)", color="#8b949e", fontsize=8)

# ── ax2: Motor Speeds ────────────────────────────────────────
(line_left,)  = ax2.plot([], [], color="#58a6ff", linewidth=1.5, label="Left")
(line_right,) = ax2.plot([], [], color="#f85149", linewidth=1.5, label="Right")
ax2.set_title("Motor Speeds", color="#c9d1d9", fontsize=10)
ax2.set_ylabel("PWM (0–200)", color="#8b949e", fontsize=8)
ax2.set_xlabel("Time (s)", color="#8b949e", fontsize=8)
ax2.set_ylim(-10, 220)
ax2.legend(
    handles=[
        mpatches.Patch(color="#58a6ff", label="Left"),
        mpatches.Patch(color="#f85149", label="Right"),
    ],
    facecolor="#161b22", labelcolor="#c9d1d9", fontsize=8,
)

# ── ax3: Obstacle Distance ───────────────────────────────────
(line_dist,) = ax3.plot([], [], color="#3fb950", linewidth=1.5)
ax3.axhline(15, color="#f85149", linewidth=0.8, linestyle="--", label="Threshold 15 cm")
ax3.set_title("Obstacle Distance", color="#c9d1d9", fontsize=10)
ax3.set_ylabel("Distance (cm)", color="#8b949e", fontsize=8)
ax3.set_xlabel("Time (s)", color="#8b949e", fontsize=8)
ax3.set_ylim(0, 120)
ax3.legend(facecolor="#161b22", labelcolor="#c9d1d9", fontsize=8)

# ── ax4: Sensor Bar ──────────────────────────────────────────
ax4.set_title("IR Sensors (S0 – S4)", color="#c9d1d9", fontsize=10)
ax4.set_xlim(-0.5, 4.5)
ax4.set_ylim(0, 1.5)
ax4.set_xticks(range(5))
ax4.set_xticklabels(["S0", "S1", "S2", "S3", "S4"], color="#c9d1d9", fontsize=9)
ax4.set_yticks([])

sensor_rects = []
for i in range(5):
    rect = mpatches.FancyBboxPatch(
        (i - 0.35, 0.1), 0.7, 0.9,
        boxstyle="round,pad=0.05",
        facecolor="#3fb950", edgecolor="#30363d", linewidth=1.5,
    )
    ax4.add_patch(rect)
    sensor_rects.append(rect)

# State label on ax4
state_text = ax4.text(
    2.0, 1.3, "State: —",
    ha="center", va="center", fontsize=10,
    color="#e3b341",
    fontfamily="monospace",
    fontweight="bold",
)

fig.suptitle(
    "Project 004 — Line Following Robot  |  Live Dashboard",
    color="#c9d1d9", fontsize=12, fontfamily="monospace",
)
plt.tight_layout(rect=[0, 0, 1, 0.95])


# ============================================================
#  animate(frame)
#  Called by FuncAnimation every interval_ms milliseconds.
# ============================================================
def animate(_frame):
    with _lock:
        ts    = list(timestamps)
        err   = list(pid_error)
        lspd  = list(left_spd)
        rspd  = list(right_spd)
        dist  = list(distance)
        sensors = list(_latest["sensors"])
        state   = _latest["state"]

    if not ts:
        return

    # ── ax1 ─────────────────────────────────────────────────
    line_err.set_data(ts, err)
    ax1.relim()
    ax1.autoscale_view()

    # ── ax2 ─────────────────────────────────────────────────
    line_left.set_data(ts, lspd)
    line_right.set_data(ts, rspd)
    ax2.relim()
    ax2.autoscale_view(scaley=False)

    # ── ax3 ─────────────────────────────────────────────────
    capped_dist = [min(d, 110) for d in dist]
    line_dist.set_data(ts, capped_dist)
    ax3.relim()
    ax3.autoscale_view(scaley=False)

    # ── ax4 ─────────────────────────────────────────────────
    for i, rect in enumerate(sensor_rects):
        active = (len(sensors) > i) and sensors[i]
        rect.set_facecolor("#3fb950" if active else "#21262d")

    color = STATE_COLORS.get(state, "#c9d1d9")
    state_text.set_text(f"State: {state}")
    state_text.set_color(color)


# ============================================================
#  main()
# ============================================================
def main() -> None:
    parser = argparse.ArgumentParser(
        description="Live serial dashboard for Project 004 — Line Following Robot",
    )
    parser.add_argument(
        "--port", default="/dev/ttyUSB0",
        help="Serial port (default: /dev/ttyUSB0)",
    )
    parser.add_argument(
        "--baud", type=int, default=9600,
        help="Baud rate (default: 9600)",
    )
    args = parser.parse_args()

    # Start background serial reader thread
    reader_thread = threading.Thread(
        target=serial_reader,
        args=(args.port, args.baud),
        daemon=True,
    )
    reader_thread.start()

    # Start animation (updates every 100 ms)
    _anim = FuncAnimation(fig, animate, interval=100, cache_frame_data=False)

    plt.show()


if __name__ == "__main__":
    main()
