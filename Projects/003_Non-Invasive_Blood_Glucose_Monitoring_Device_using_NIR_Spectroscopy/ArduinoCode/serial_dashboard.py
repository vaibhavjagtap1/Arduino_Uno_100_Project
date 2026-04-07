#!/usr/bin/env python3
"""
NIR Glucose Monitor — Python Serial Dashboard
==============================================
Reads JSON telemetry from Arduino Serial port and displays
a live matplotlib dashboard with glucose trend, absorbance
trend, and a status indicator panel.

DISCLAIMER: This dashboard is part of an educational prototype.
            The glucose readings shown are NOT clinically validated
            and MUST NOT be used for medical diagnosis or treatment.

Usage:
    pip install pyserial matplotlib
    python serial_dashboard.py --port COM3           (Windows)
    python serial_dashboard.py --port /dev/ttyUSB0   (Linux/Mac)
    python serial_dashboard.py --port /dev/ttyACM0 --baud 9600
"""

import argparse
import json
import sys
import threading
import time
from collections import deque
from datetime import datetime

# ── Third-party imports (install with pip) ────────────────────
try:
    import serial
except ImportError:
    print("[ERROR] pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

try:
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    import matplotlib.patches as mpatches
    from matplotlib.gridspec import GridSpec
    import matplotlib.ticker as ticker
except ImportError:
    print("[ERROR] matplotlib not installed. Run: pip install matplotlib")
    sys.exit(1)

# ── Configuration ─────────────────────────────────────────────
BAUD_RATE    = 9600
HISTORY_LEN  = 60     # data points displayed (60 samples ≈ ~2 min)
REFRESH_MS   = 500    # animation refresh interval (ms)

# Glucose classification thresholds (mg/dL)
GLUCOSE_LOW        = 70.0
GLUCOSE_NORMAL_HI  = 140.0
GLUCOSE_HIGH_HI    = 200.0

# Colour scheme
COLOR_BG        = "#0d1117"
COLOR_PANEL     = "#161b22"
COLOR_TEXT      = "#c9d1d9"
COLOR_GRID      = "#21262d"
COLOR_HYPO      = "#ff4d6d"    # hypoglycaemic zone
COLOR_NORMAL    = "#3fb950"    # normal zone
COLOR_HIGH      = "#d29922"    # high zone
COLOR_VERY_HIGH = "#f85149"    # very high zone
COLOR_GLUCOSE   = "#58a6ff"
COLOR_ABS       = "#bc8cff"
COLOR_TEMP      = "#ffa657"

# ── Data Buffers ──────────────────────────────────────────────
timestamps  = deque(maxlen=HISTORY_LEN)
glucose_buf = deque(maxlen=HISTORY_LEN)
abs_buf     = deque(maxlen=HISTORY_LEN)
temp_buf    = deque(maxlen=HISTORY_LEN)
nir_buf     = deque(maxlen=HISTORY_LEN)
red_buf     = deque(maxlen=HISTORY_LEN)

latest_data: dict = {}
serial_lock = threading.Lock()

# ── Serial Reader Thread ──────────────────────────────────────
def serial_reader(port: str, baud: int) -> None:
    """Background thread: continuously reads JSON lines from Arduino."""
    global latest_data
    try:
        ser = serial.Serial(port, baud, timeout=2)
        print(f"[✓] Connected to {port} @ {baud} baud")
    except serial.SerialException as exc:
        print(f"[ERROR] Cannot open {port}: {exc}")
        sys.exit(1)

    while True:
        try:
            raw = ser.readline().decode("utf-8", errors="ignore").strip()
        except serial.SerialException:
            print("[WARN] Serial read error — reconnecting…")
            time.sleep(2)
            continue

        if not raw.startswith("{"):
            if raw:
                print(f"[INFO] {raw}")
            continue

        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            continue

        now = datetime.now().strftime("%H:%M:%S")
        with serial_lock:
            latest_data = data
            timestamps.append(now)
            glucose_buf.append(data.get("glucose", 0.0))
            abs_buf.append(data.get("absorbance", 0.0))
            temp_buf.append(data.get("temp", 25.0))
            nir_buf.append(data.get("nir", 0.0))
            red_buf.append(data.get("red", 0.0))

        g   = data.get("glucose", 0.0)
        sts = data.get("status", "---")
        t   = data.get("temp", 0.0)
        print(
            f"[{now}] Glucose: {g:6.1f} mg/dL  "
            f"Status: {sts:<10s}  "
            f"Abs: {data.get('absorbance', 0.0):.4f}  "
            f"Temp: {t:.1f}°C"
        )

# ── Dashboard Layout ──────────────────────────────────────────
def build_figure():
    fig = plt.figure(figsize=(13, 7), facecolor=COLOR_BG)
    fig.canvas.manager.set_window_title("NIR Glucose Monitor — Educational Dashboard")
    gs = GridSpec(3, 3, figure=fig, hspace=0.45, wspace=0.35,
                  left=0.07, right=0.97, top=0.92, bottom=0.08)

    ax_glucose = fig.add_subplot(gs[0:2, 0:2])   # main glucose trend
    ax_abs     = fig.add_subplot(gs[2,   0:2])   # absorbance trend
    ax_status  = fig.add_subplot(gs[0,   2])     # status panel
    ax_temp    = fig.add_subplot(gs[1,   2])     # temperature
    ax_disc    = fig.add_subplot(gs[2,   2])     # disclaimer

    for ax in [ax_glucose, ax_abs, ax_status, ax_temp, ax_disc]:
        ax.set_facecolor(COLOR_PANEL)
        for spine in ax.spines.values():
            spine.set_edgecolor(COLOR_GRID)

    return fig, ax_glucose, ax_abs, ax_status, ax_temp, ax_disc


def style_axis(ax, title: str, ylabel: str) -> None:
    ax.set_title(title, color=COLOR_TEXT, fontsize=9, pad=4)
    ax.set_ylabel(ylabel, color=COLOR_TEXT, fontsize=8)
    ax.tick_params(colors=COLOR_TEXT, labelsize=7)
    ax.yaxis.label.set_color(COLOR_TEXT)
    ax.grid(True, color=COLOR_GRID, linewidth=0.5, linestyle="--")
    ax.set_xlim(0, HISTORY_LEN)


def draw_glucose_zones(ax, y_min: float = 0.0, y_max: float = 350.0) -> None:
    """Fill colour bands for glucose classification zones."""
    ax.axhspan(y_min,           GLUCOSE_LOW,       alpha=0.15, color=COLOR_HYPO,      zorder=0)
    ax.axhspan(GLUCOSE_LOW,     GLUCOSE_NORMAL_HI, alpha=0.12, color=COLOR_NORMAL,    zorder=0)
    ax.axhspan(GLUCOSE_NORMAL_HI, GLUCOSE_HIGH_HI, alpha=0.12, color=COLOR_HIGH,      zorder=0)
    ax.axhspan(GLUCOSE_HIGH_HI, y_max,             alpha=0.12, color=COLOR_VERY_HIGH, zorder=0)
    # Dashed threshold lines
    for thresh, col in [(GLUCOSE_LOW, COLOR_HYPO),
                        (GLUCOSE_NORMAL_HI, COLOR_NORMAL),
                        (GLUCOSE_HIGH_HI, COLOR_HIGH)]:
        ax.axhline(thresh, color=col, linewidth=0.8, linestyle=":", alpha=0.7)


def get_status_color(status: str) -> str:
    mapping = {
        "NORMAL":    COLOR_NORMAL,
        "LOW":       COLOR_HYPO,
        "HIGH":      COLOR_HIGH,
        "VERY HIGH": COLOR_VERY_HIGH,
    }
    return mapping.get(status.upper(), COLOR_TEXT)


def animate(frame, fig, ax_glucose, ax_abs, ax_status, ax_temp, ax_disc) -> None:
    """Called by FuncAnimation every REFRESH_MS ms."""
    with serial_lock:
        g_list   = list(glucose_buf)
        a_list   = list(abs_buf)
        t_list   = list(temp_buf)
        ts_list  = list(timestamps)
        current  = dict(latest_data)

    x = list(range(len(g_list)))

    # ── Glucose Trend ────────────────────────────────────────
    ax_glucose.cla()
    ax_glucose.set_facecolor(COLOR_PANEL)
    draw_glucose_zones(ax_glucose)
    if g_list:
        ax_glucose.plot(x, g_list, color=COLOR_GLUCOSE, linewidth=1.8,
                        marker="o", markersize=3, label="Glucose")
        ax_glucose.fill_between(x, g_list, alpha=0.15, color=COLOR_GLUCOSE)
    style_axis(ax_glucose, "Blood Glucose Estimate (mg/dL)", "mg/dL")
    ax_glucose.set_ylim(0, 350)
    ax_glucose.set_xlim(0, HISTORY_LEN)

    # Zone legend
    patches = [
        mpatches.Patch(color=COLOR_HYPO,      alpha=0.6, label=f"Hypo   <{GLUCOSE_LOW:.0f}"),
        mpatches.Patch(color=COLOR_NORMAL,    alpha=0.6, label=f"Normal {GLUCOSE_LOW:.0f}–{GLUCOSE_NORMAL_HI:.0f}"),
        mpatches.Patch(color=COLOR_HIGH,      alpha=0.6, label=f"High   {GLUCOSE_NORMAL_HI:.0f}–{GLUCOSE_HIGH_HI:.0f}"),
        mpatches.Patch(color=COLOR_VERY_HIGH, alpha=0.6, label=f"V.High >{GLUCOSE_HIGH_HI:.0f}"),
    ]
    ax_glucose.legend(handles=patches, loc="upper left",
                      fontsize=6.5, framealpha=0.3,
                      labelcolor=COLOR_TEXT, facecolor=COLOR_PANEL)

    # ── Absorbance Trend ─────────────────────────────────────
    ax_abs.cla()
    ax_abs.set_facecolor(COLOR_PANEL)
    if a_list:
        ax_abs.plot(x, a_list, color=COLOR_ABS, linewidth=1.5,
                    marker="s", markersize=2.5, label="Abs Ratio")
        ax_abs.fill_between(x, a_list, alpha=0.15, color=COLOR_ABS)
    style_axis(ax_abs, "Absorbance Ratio (940 nm / 660 nm)", "Abs")
    ax_abs.set_xlim(0, HISTORY_LEN)
    ax_abs.tick_params(axis="x", colors=COLOR_TEXT, labelsize=7)
    ax_abs.set_xlabel("Sample #", color=COLOR_TEXT, fontsize=8)

    # ── Status Panel ─────────────────────────────────────────
    ax_status.cla()
    ax_status.set_facecolor(COLOR_PANEL)
    ax_status.axis("off")
    status_str = current.get("status", "---")
    glucose_v  = current.get("glucose", 0.0)
    sc = get_status_color(status_str)
    ax_status.text(0.5, 0.85, "CURRENT READING", ha="center", va="center",
                   fontsize=8, color=COLOR_TEXT,
                   transform=ax_status.transAxes)
    ax_status.text(0.5, 0.58, f"{glucose_v:.1f}", ha="center", va="center",
                   fontsize=26, fontweight="bold", color=sc,
                   transform=ax_status.transAxes)
    ax_status.text(0.5, 0.38, "mg/dL", ha="center", va="center",
                   fontsize=10, color=COLOR_TEXT,
                   transform=ax_status.transAxes)
    ax_status.text(0.5, 0.18, status_str, ha="center", va="center",
                   fontsize=12, fontweight="bold", color=sc,
                   transform=ax_status.transAxes)
    ax_status.set_title("Status", color=COLOR_TEXT, fontsize=9, pad=4)

    # ── Temperature ──────────────────────────────────────────
    ax_temp.cla()
    ax_temp.set_facecolor(COLOR_PANEL)
    if t_list:
        ax_temp.plot(list(range(len(t_list))), t_list,
                     color=COLOR_TEMP, linewidth=1.5)
    style_axis(ax_temp, "Finger Temperature (°C)", "°C")
    ax_temp.set_xlim(0, HISTORY_LEN)
    current_temp = current.get("temp", 0.0)
    ax_temp.text(0.97, 0.92, f"{current_temp:.1f}°C",
                 ha="right", va="top", fontsize=10,
                 color=COLOR_TEMP, transform=ax_temp.transAxes)

    # ── Disclaimer Panel ─────────────────────────────────────
    ax_disc.cla()
    ax_disc.set_facecolor("#2d1a1a")
    ax_disc.axis("off")
    disc_text = (
        "⚠ DISCLAIMER\n\n"
        "This is an EDUCATIONAL\n"
        "PROTOTYPE only.\n\n"
        "Values shown are NOT\n"
        "clinically validated.\n\n"
        "DO NOT use for medical\n"
        "diagnosis or treatment.\n\n"
        "Use certified glucometer\n"
        "for clinical measurements."
    )
    ax_disc.text(0.5, 0.5, disc_text, ha="center", va="center",
                 fontsize=7.5, color="#ff9999", linespacing=1.5,
                 transform=ax_disc.transAxes,
                 bbox=dict(facecolor="#2d1a1a", edgecolor="#f85149",
                           boxstyle="round,pad=0.4", linewidth=1.2))

    # Figure title with timestamp
    now_str = ts_list[-1] if ts_list else "--:--:--"
    fig.suptitle(
        f"NIR Glucose Monitor — Educational Dashboard   [{now_str}]",
        color=COLOR_TEXT, fontsize=11, y=0.97
    )


# ── Main ──────────────────────────────────────────────────────
def main() -> None:
    parser = argparse.ArgumentParser(
        description="NIR Glucose Monitor — Serial Dashboard (Educational)"
    )
    parser.add_argument(
        "--port", default="COM3",
        help="Serial port (e.g. COM3, /dev/ttyUSB0, /dev/ttyACM0)"
    )
    parser.add_argument(
        "--baud", type=int, default=BAUD_RATE,
        help=f"Baud rate (default: {BAUD_RATE})"
    )
    args = parser.parse_args()

    print("=" * 60)
    print(" NIR Glucose Monitor — Educational Serial Dashboard")
    print("=" * 60)
    print("⚠  DISCLAIMER: Educational prototype only.")
    print("   NOT for clinical diagnosis or medical use.")
    print("=" * 60)
    print(f"Connecting to {args.port} @ {args.baud} baud…")

    # Start serial reader thread
    reader = threading.Thread(
        target=serial_reader, args=(args.port, args.baud), daemon=True
    )
    reader.start()

    # Build matplotlib figure
    fig, ax_glucose, ax_abs, ax_status, ax_temp, ax_disc = build_figure()

    ani = animation.FuncAnimation(
        fig,
        animate,
        fargs=(fig, ax_glucose, ax_abs, ax_status, ax_temp, ax_disc),
        interval=REFRESH_MS,
        cache_frame_data=False,
    )

    plt.show()


if __name__ == "__main__":
    main()
