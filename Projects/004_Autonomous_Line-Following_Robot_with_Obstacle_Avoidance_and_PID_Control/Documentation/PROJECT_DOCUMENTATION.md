# Project 004 — Autonomous Line-Following Robot with Obstacle Avoidance and PID Control

| | |
|---|---|
| **Author** | vaibhavjagtap1 |
| **Board** | Arduino Uno R3 |
| **Difficulty** | Advanced |
| **Category** | Robotics · Embedded Control |
| **Version** | 1.0.0 |
| **Date** | 2025 |

---

## Project Overview

A **line-following robot** detects a high-contrast line (black tape on a white surface) using
infrared reflectance sensors and steers its motors to keep the robot centred over the line.
The simplest approach — *bang-bang* control — switches the robot sharply left or right based
on which side of the line it has drifted to.  This produces a characteristic weaving motion
that wastes time and can overshoot curves.

This project replaces bang-bang with a **PID (Proportional-Integral-Derivative) controller**:

* **Proportional** term corrects in proportion to how far off-centre the robot is.
* **Integral** term eliminates steady-state offset (e.g., a wheel that is slightly slower).
* **Derivative** term damps oscillation and predicts future error from its rate of change.

The result is a smooth, accurate trajectory even on tight curves.  An **HC-SR04 ultrasonic
sensor** monitors the path ahead; when an obstacle is detected within 15 cm a **state machine**
takes over, manoeuvres around the obstacle, then hands control back to the PID follower.

---

## Features

| Feature | Description |
|---|---|
| 5-sensor array | TCRT5000 sensors provide a weighted position estimate in `[−2, +2]` |
| PID control | Smooth, configurable Kp/Ki/Kd speed correction on each wheel |
| Obstacle avoidance | HC-SR04 triggers a left/right clearance check and bypass manoeuvre |
| State machine | 5 states: LINE_FOLLOW, OBSTACLE_DETECTED, AVOID_LEFT, AVOID_RIGHT, SEARCH |
| JSON telemetry | 100 ms serial JSON packets for real-time monitoring |
| Python dashboard | Matplotlib 2×2 live dashboard via `serial_dashboard.py` |
| No extra libraries | Uses Arduino built-in functions only (`analogWrite`, `pulseIn`, etc.) |

---

## System Architecture

```
 ┌──────────────────────────────────────────────────────────┐
 │                   Arduino Uno R3                         │
 │                                                          │
 │  [IR S0] [S1] [S2] [S3] [S4]  ──►  readSensors()        │
 │                                        │                 │
 │                                  computePosition()       │
 │                                        │                 │
 │              [HC-SR04] ──────────  measureDistance()     │
 │                                        │                 │
 │                          ┌─────────────▼────────────┐    │
 │                          │      State Machine        │    │
 │                          │  LINE_FOLLOW / SEARCH     │    │
 │                          │  OBSTACLE_DETECTED        │    │
 │                          │  AVOID_LEFT / AVOID_RIGHT │    │
 │                          └──────────┬────────────────┘    │
 │                                     │                    │
 │                              computePID()                │
 │                                     │                    │
 │                              setMotors()                 │
 │                                     │                    │
 │                   [L298N] ──────────┘                    │
 │                   /     \                                │
 │            [Motor A]  [Motor B]                         │
 └──────────────────────────────────────────────────────────┘
          │
     Serial (9600 baud)  ──►  serial_dashboard.py
```

---

## Hardware Required

| ID | Component | Qty | Specification |
|---|---|---|---|
| C01 | Arduino Uno R3 | 1 | ATmega328P 5V 16 MHz |
| C02 | IR Proximity Sensor (TCRT5000) | 5 | 3.3–5V, adjustable sensitivity |
| C03 | HC-SR04 Ultrasonic Sensor | 1 | 5V, 2–400 cm ±3 mm |
| C04 | L298N Motor Driver Module | 1 | 5–46V 2A/channel |
| C05 | DC Geared Motor 6V | 2 | 6V 150 RPM 1:48 |
| C06 | Robot Chassis (2WD) | 1 | Acrylic/ABS platform |
| C07 | Rubber Wheels 65 mm | 2 | 65 mm diameter |
| C08 | Caster Ball Wheel | 1 | Universal 360° |
| C09 | 7.4V LiPo Battery | 1 | 1200 mAh 2S 20C |
| C10 | 5V Regulator / Buck Converter | 1 | LM7805 or adjustable buck |
| C11 | 5mm LED (Red) | 1 | Status / obstacle indicator |
| C12 | 220 Ω Resistor | 1 | 1/4W, LED current limiting |
| C13 | 10 kΩ Resistor | 5 | 1/4W, IR pull-up (optional) |
| C14 | Half-size Breadboard | 1 | 400 tie-point |
| C15 | Jumper Wires M-M / M-F | 40 | 20 cm colour-coded |
| C16 | USB Cable Type-A to B | 1 | 1 m |
| C17 | M3 Screws + Spacers | 8 | M3×10 mm |
| C18 | Black Line Tracking Tape | 1 | 19 mm wide matte black |

---

## Circuit Wiring Guide

### IR Sensors (TCRT5000 × 5)

Each TCRT5000 module has three pins: **VCC**, **GND**, and **OUT**.

| Sensor | Module OUT → Arduino Pin |
|---|---|
| S0 (far-left) | D2 |
| S1 (left) | D3 |
| S2 (center) | A0 |
| S3 (right) | A1 |
| S4 (far-right) | A2 |

* Connect all VCC pins to Arduino **5V**.
* Connect all GND pins to common **GND**.
* Mount sensors 5–10 mm above the surface, evenly spaced ~15 mm apart.
* Adjust the blue trimmer pot on each module until the LED turns on over white and off over black.

### HC-SR04 Ultrasonic Sensor

| HC-SR04 Pin | Arduino Pin |
|---|---|
| VCC | 5V |
| TRIG | D8 |
| ECHO | D9 |
| GND | GND |

> Mount the sensor at the **front** of the robot chassis, angled slightly downward to detect objects on the floor.

### L298N Motor Driver

| L298N Pin | Arduino Pin | Note |
|---|---|---|
| ENA | D10 (PWM) | Left motor speed |
| IN1 | D4 | Left motor direction A |
| IN2 | D5 | Left motor direction B |
| ENB | D11 (PWM) | Right motor speed |
| IN3 | D6 | Right motor direction A |
| IN4 | D7 | Right motor direction B |
| 12V | Battery (+) | 7.4V LiPo positive |
| GND | Battery (−) / Arduino GND | Common ground |

* **Motor A** (OUT1/OUT2) → Left geared motor
* **Motor B** (OUT3/OUT4) → Right geared motor
* Remove the ENA/ENB jumpers so PWM control is active.

### Power Section

```
7.4V LiPo (+) ──┬──► L298N 12V input
                │
                └──► 5V Buck Converter IN(+)
                          │
                     Buck OUT(+) ──► Arduino VIN or 5V pin
                     Buck OUT(−) ──► GND (common)
7.4V LiPo (−) ──────────────────── GND (common)
```

> **Warning:** Never connect the 7.4V LiPo directly to the Arduino 5V pin.
> Always use a voltage regulator or buck converter.

---

## Software Setup

No external Arduino libraries are required.  The sketch uses only built-in Arduino functions:

| Function | Use |
|---|---|
| `digitalRead()` | IR sensor reading |
| `digitalWrite()` | Motor direction, LED, TRIG pulse |
| `analogWrite()` | PWM motor speed via ENA/ENB |
| `pulseIn()` | HC-SR04 echo timing |
| `constrain()` | PID clamping |
| `millis()` | Non-blocking serial throttle |

---

## How to Upload & Run

1. Open **Arduino IDE** (2.x or 1.8.x).
2. Connect Arduino Uno via USB.
3. Go to **File → Open** and select `ArduinoCode/LineFollowingRobot.ino`.
4. Select **Tools → Board → Arduino Uno**.
5. Select the correct **Port** under Tools → Port.
6. Click **Upload** (Ctrl+U).
7. Open **Serial Monitor** at 9600 baud to see JSON telemetry.

---

## Serial Dashboard Setup

```bash
# Install dependencies
pip install pyserial matplotlib

# Run dashboard (Linux/macOS)
python ArduinoCode/serial_dashboard.py --port /dev/ttyUSB0 --baud 9600

# Run dashboard (Windows)
python ArduinoCode/serial_dashboard.py --port COM3 --baud 9600
```

The dashboard opens a 2×2 live window:

| Subplot | Content |
|---|---|
| Top-left | PID error trend (blue line, zero reference in green) |
| Top-right | Motor speeds (left=blue, right=red, PWM 0–200) |
| Bottom-left | Obstacle distance (green line, 15 cm threshold in red) |
| Bottom-right | Sensor bar (5 coloured rectangles, green=active) + state label |

---

## PID Tuning Guide

The PID constants are defined as `#define` macros at the top of the sketch:

```cpp
#define KP  0.4f
#define KI  0.01f
#define KD  0.3f
```

### Effect of Each Parameter

| Parameter | Effect when increased | Symptom of too-high value |
|---|---|---|
| **Kp** (Proportional) | Faster response to error | Robot oscillates / weaves |
| **Ki** (Integral) | Eliminates drift / steady-state offset | Slow oscillation, wind-up |
| **Kd** (Derivative) | Damps oscillations, smoother curves | Jerky response to noise |

### Tuning Procedure

1. **Start with Ki=0, Kd=0**.  Increase Kp until the robot follows straight sections smoothly but starts to oscillate on curves.
2. **Add Kd** to reduce the oscillation from step 1.  Increase until response is crisp but not jerky.
3. **Add Ki** (small value, e.g. 0.005) only if the robot drifts to one side on straight sections.
4. Verify performance on tight curves; reduce Kp or increase Kd if overshooting.
5. Adjust `BASE_SPEED` last — higher speed requires higher Kd.

---

## State Machine Explained

| State | Trigger Condition | Actions |
|---|---|---|
| `LINE_FOLLOW` | Default / line re-acquired | PID correction, drive forward |
| `OBSTACLE_DETECTED` | `dist < 15 cm` while LINE_FOLLOW | Stop, call `avoidObstacle()` |
| `AVOID_LEFT` | Left side has more clearance | Turn left, drive past, turn right |
| `AVOID_RIGHT` | Right side has more clearance | Turn right, drive past, turn left |
| `SEARCH` | All 5 sensors inactive | Spin slowly in direction of last error |

```
                  dist < 15 cm
LINE_FOLLOW ─────────────────► OBSTACLE_DETECTED
    ▲                                │
    │              left clear ───────┤
    │         ◄── AVOID_LEFT ◄──────┤
    │         line found             │
    │              right clear ──────┘
    │         ◄── AVOID_RIGHT
    │         line found
    │
    │  all sensors off
    ├───────────────────────► SEARCH
    └─────── line found ─────────────
```

---

## Troubleshooting

| Problem | Likely Cause | Solution |
|---|---|---|
| Robot veers strongly to one side | One motor faster than the other | Adjust `BASE_SPEED` or add `Ki` trim |
| Robot ignores line on curves | `Kp` too low or `BASE_SPEED` too high | Increase Kp, reduce speed |
| Excessive weaving on straight sections | `Kp` too high | Reduce Kp, increase Kd |
| Obstacle avoidance not triggering | HC-SR04 wiring or timeout too short | Check TRIG/ECHO wiring, verify `pulseIn` timeout |
| All sensors read 0 | Sensors too high above surface | Lower sensor mount to 5–10 mm from ground |
| Serial dashboard shows no data | Wrong port / baud mismatch | Confirm port in Device Manager / `ls /dev/tty*` |
| Robot stalls after avoidance | Battery voltage sagging | Charge LiPo or increase `BACKUP_SPEED` threshold |
| PID integral wind-up | `Ki` too large, long off-line periods | Reduce Ki; anti-windup clamp already at ±50 |

---

## Future Enhancements

* **Camera integration** — replace IR sensors with OpenCV line detection via a Raspberry Pi or OV7670 camera module.
* **Maze solving** — implement a left-hand or right-hand wall-following algorithm for maze navigation.
* **Bluetooth / Wi-Fi remote control** — add an HC-05 Bluetooth or ESP8266 Wi-Fi module to tune PID constants wirelessly from a phone app.
* **OLED status display** — mount a 0.96" SSD1306 OLED to show live speed, state, and distance on the robot itself.
* **Encoder feedback** — add quadrature encoders to the motors for closed-loop velocity control (cascade PID).
* **Fuzzy logic controller** — replace PID with a fuzzy logic decision engine for adaptive tuning.
* **Multi-robot coordination** — use IR or RF communication between two robots to follow / lead behaviours.
* **Data logging** — log telemetry to an SD card module for post-run analysis and PID optimisation.
