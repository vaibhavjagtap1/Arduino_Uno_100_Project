# Project 003 — Non-Invasive Blood Glucose Monitoring Device using NIR Spectroscopy

## Documentation Guide

---

## ⚠️ Medical Disclaimer

> **THIS IS AN EDUCATIONAL PROTOTYPE ONLY.**
>
> The device described in this documentation DOES NOT constitute a certified
> medical device and MUST NOT be used for clinical blood glucose diagnosis,
> treatment decisions, or any medical purpose.
>
> Near-infrared non-invasive glucose measurement is an active area of medical
> research. No consumer-grade NIR device based on Arduino hardware can match
> the accuracy required for clinical glucose monitoring.
>
> **For actual blood glucose monitoring, always use an FDA/CE/CDSCO-cleared
> glucometer with certified test strips.**
>
> The authors, contributors, and repository maintainers accept no liability
> for any harm resulting from misuse of this prototype.

---

## 1. Project Overview

### What Is This Project?

This project demonstrates the **Beer-Lambert law** and **near-infrared (NIR)
spectroscopy** principles using common Arduino components. A 940 nm IR LED and
a 660 nm red LED illuminate a finger probe. A silicon photodiode with a
transimpedance amplifier (TIA) converts the transmitted optical signal to a
voltage. The Arduino ADC digitises both channels, computes an absorbance ratio,
and maps it to a glucose estimate through a linear calibration model.

### Why NIR for Glucose?

Glucose absorbs NIR light preferentially at specific wavelengths (~940 nm, ~1450 nm,
~2100 nm). The ratio of absorption at 940 nm vs. a reference wavelength (660 nm)
correlates with glucose concentration — this is the same principle used in
commercial pulse oximeters (SpO₂), but extended to a different spectral band.

### Key Educational Concepts Demonstrated

| Concept | Implementation |
|---------|---------------|
| Beer-Lambert Law | Abs = log₁₀(I₀ / I) per channel |
| Ratio-metric measurement | 940nm / 660nm absorbance ratio |
| Transimpedance amplifier (TIA) | LM358 op-amp + 1MΩ feedback |
| Signal averaging (noise reduction) | 32-sample ADC averaging |
| Temperature compensation | NTC thermistor Steinhart-Hart model |
| Moving-average trend detection | 5-point circular buffer |
| I2C communication | LCD display, PCF8574 expander |
| PWM LED driving | analogWrite() for intensity control |

---

## 2. Features

| Feature | Details |
|---------|---------|
| Dual-wavelength NIR | 940 nm (glucose channel) + 660 nm (reference) |
| Signal averaging | 32 ADC samples per channel per measurement |
| Temperature compensation | NTC thermistor on A2 |
| Glucose estimation | Linear regression model (Beer-Lambert based) |
| LCD display | 16×2 I2C, shows value + trend arrow (↑↓→) |
| Status classification | LOW / NORMAL / HIGH / VERY HIGH |
| LED indicators | Green (normal), Yellow (high), Red (alert) |
| Buzzer alert | Critical levels trigger audible alarm |
| Serial JSON telemetry | For Python dashboard / data logging |
| Button-triggered measurement | D2 with pull-down resistor |
| Trend detection | 5-point moving average (rising/falling/stable) |

---

## 3. System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    NIR Glucose Monitor — Block Diagram              │
└─────────────────────────────────────────────────────────────────────┘

   ┌──────────────┐     ┌──────────────┐
   │  940nm IR LED│     │  660nm Red   │
   │  (D6 PWM)    │     │  LED (D5 PWM)│
   └──────┬───────┘     └──────┬───────┘
          │ light               │ light
          ▼                     ▼
   ┌──────────────────────────────────┐
   │         FINGER PROBE             │
   │   (tissue / phantom sample)      │
   └──────────────────────────────────┘
          │ transmitted              │ transmitted
          ▼                         ▼
   ┌────────────┐            ┌────────────┐
   │ BPW34 PD   │            │ BPW34 PD   │
   │ (940nm ch) │            │ (660nm ch) │
   └──────┬─────┘            └──────┬─────┘
          │ I_photo                 │ I_photo
          ▼                         ▼
   ┌────────────┐            ┌────────────┐
   │ LM358 TIA  │            │ LM358 TIA  │
   │ Rf = 1MΩ   │            │ Rf = 1MΩ   │
   └──────┬─────┘            └──────┬─────┘
          │ V_out(A0)               │ V_out(A1)
          └──────────┬──────────────┘
                     ▼
            ┌────────────────┐
            │  Arduino Uno   │
            │  ATmega328P    │
            │                │
            │  ADC A0 (NIR)  │
            │  ADC A1 (Red)  │
            │  ADC A2 (NTC)  │
            │                │
            │  Processing:   │
            │  • 32× average │
            │  • Beer-Lambert│
            │  • Calibration │
            │  • Temp. comp  │
            │  • Trend detect│
            └───────┬────────┘
                    │
        ┌───────────┼───────────────────┐
        ▼           ▼                   ▼
   ┌─────────┐ ┌──────────┐   ┌────────────────┐
   │LCD 16×2 │ │LED panel │   │ Serial JSON    │
   │(I2C)    │ │G/Y/R +   │   │ → PC Dashboard │
   │Glucose  │ │Buzzer    │   │ (Python)       │
   │+ trend  │ └──────────┘   └────────────────┘
   └─────────┘
```

---

## 4. Hardware Required

Refer to `Components/components_list.txt` for the full bill of materials with
quantities, specifications, and costs.

### Core Components

- Arduino Uno R3 (1×)
- 940 nm IR LED (1×) + 100 Ω series resistor
- 660 nm Red LED (1×) + 100 Ω series resistor
- BPW34 silicon photodiode (2×)
- LM358 dual op-amp in DIP-8 package (1×)
- 1 MΩ metal-film resistors for TIA feedback (2×)
- 10 kΩ NTC thermistor + 10 kΩ series resistor
- Tactile push button + 10 kΩ pull-down resistor
- 16×2 LCD with I2C backpack (PCF8574 @ 0x27)
- Green, Yellow, Red 5mm LEDs + 220 Ω resistors each
- 5V active buzzer
- Decoupling capacitors: 0.1 µF × 4, 10 µF × 2
- Breadboard, jumper wires

---

## 5. Circuit Wiring Guide

See `CircuitDiagram/circuit.svg` for the complete visual schematic.

### 5.1 LED Driver Circuit

```
Arduino D6 ──► 100 Ω ──► 940nm IR LED (anode) ──► GND (cathode)
Arduino D5 ──► 100 Ω ──► 660nm Red LED (anode) ──► GND (cathode)
```

- Both LEDs are driven with `analogWrite()` (PWM ≈ 78% = value 200/255).
- LEDs are **multiplexed** — only one is on at a time during measurement.
- Keep wires to LEDs short and route them away from photodiode inputs.

### 5.2 Transimpedance Amplifier (TIA) — LM358

```
BPW34 cathode ──► LM358 inverting input (pin 2 / pin 6)
BPW34 anode   ──► GND

Feedback:  LM358 inverting input ←──── 1MΩ ────── LM358 output
Non-inv:   LM358 non-inverting (pin 3 / pin 5) ──► GND (virtual ground)

Output pin 1 ──► Arduino A0   (940nm channel)
Output pin 7 ──► Arduino A1   (660nm channel)
```

> **Gain**: V_out = I_photo × R_f = I_photo × 1MΩ
> For a typical photodiode current of ~1 µA through tissue: V_out ≈ 1 V.

### 5.3 Finger Probe Design

The probe is the most critical mechanical element:

1. Use a 3D-printed clip or fold black cardboard into a U-shape (~20 mm wide).
2. Mount the **LEDs on one arm**, the **photodiode on the opposite arm** —
   light must travel *through* the finger.
3. Line all interior surfaces with **black electrical tape** or black foam
   to eliminate ambient light leakage.
4. Photodiode window should face directly toward the LED.
5. Keep probe rigid during measurement — movement artifacts are the
   primary noise source.

### 5.4 NTC Thermistor Voltage Divider

```
5V ──► NTC Thermistor (10kΩ) ──►●──► 10kΩ (series) ──► GND
                                 │
                                 └──► Arduino A2
```

Temperature computed using Steinhart-Hart simplified (B-coefficient) equation.

### 5.5 Status LEDs and Buzzer

```
D9  ──► 220 Ω ──► Green LED  ──► GND   (NORMAL:  70–140 mg/dL)
D10 ──► 220 Ω ──► Yellow LED ──► GND   (HIGH:   140–200 mg/dL)
D11 ──► 220 Ω ──► Red LED    ──► GND   (ALERT:  <70 or >200 mg/dL)
D12 ──► Active Buzzer (+) ──► GND      (CRITICAL levels)
```

---

## 6. Software Setup

### 6.1 Arduino IDE

1. Download and install **Arduino IDE** (v2.x recommended): https://www.arduino.cc/en/software
2. Connect Arduino Uno via USB.
3. Select **Board**: `Arduino Uno` and correct **Port** in the IDE.

### 6.2 Required Libraries

Install via **Sketch → Include Library → Manage Libraries**:

| Library | Author | Version |
|---------|--------|---------|
| `LiquidCrystal_I2C` | Frank de Brabander | 1.1.2+ |
| `Wire` | Arduino (built-in) | — |

`math.h` is part of the AVR-libc toolchain — no separate install needed.

### 6.3 Python Dashboard Dependencies

```bash
pip install pyserial matplotlib
```

---

## 7. How to Upload & Run

```bash
# 1. Open sketch
File → Open → ArduinoCode/GlucoseMonitor.ino

# 2. Verify compilation
Sketch → Verify/Compile   (Ctrl+R)

# 3. Upload to board
Sketch → Upload           (Ctrl+U)

# 4. Open Serial Monitor (9600 baud) to see JSON output
Tools → Serial Monitor

# 5. Place finger in probe, press button → measurement starts
```

### Expected Serial Output

```json
{"nir":412.5,"red":387.2,"abs940":0.3953,"abs660":0.4218,"absorbance":0.9373,"glucose":877.0,"status":"VERY HIGH","temp":36.8}
```

> **Note**: Uncalibrated output will show extreme values.
> See Section 8 (Calibration Guide) before interpreting results.

---

## 8. Serial Dashboard Setup

```bash
# Install dependencies (once)
pip install pyserial matplotlib

# Run dashboard — Windows
python ArduinoCode/serial_dashboard.py --port COM3

# Run dashboard — Linux/Mac
python ArduinoCode/serial_dashboard.py --port /dev/ttyUSB0

# Custom baud
python ArduinoCode/serial_dashboard.py --port /dev/ttyACM0 --baud 9600
```

The dashboard shows:
- **Glucose trend plot** with colour zones (hypo/normal/high/very high)
- **Absorbance ratio trend**
- **Real-time status panel** with current value
- **Temperature trend**
- **Disclaimer panel** (always visible)

---

## 9. Calibration Guide

> ⚠️ Calibration with real blood is not safe for a prototype.
> Use **known glucose solutions** (dissolved lab-grade glucose in distilled water)
> for bench calibration.

### 9.1 Reference Calibration Procedure

1. **Prepare known solutions** (e.g., 0 mg/dL, 100 mg/dL, 200 mg/dL, 400 mg/dL)
   using food-grade glucose (dextrose) dissolved in distilled water.
2. Place each solution in a small transparent cuvette or tube.
3. Insert cuvette between LED and photodiode (instead of finger).
4. Press button → record `absorbance` from Serial Monitor for each concentration.
5. Plot absorbance vs. known concentration → fit a linear regression line.
6. Update `CALIB_A` and `CALIB_B` constants in `GlucoseMonitor.ino`.

### 9.2 Calibration Constants in Code

```cpp
#define CALIB_A   850.0f   // slope     (mg/dL per absorbance unit)
#define CALIB_B    80.0f   // intercept (mg/dL)
// glucose = CALIB_A × absorbance_ratio + CALIB_B
```

### 9.3 Ideal Linearity Check

| Known [Glucose] mg/dL | Expected Abs Ratio | Measured Abs Ratio |
|----------------------|-------------------|-------------------|
| 0                    | ~0.00             | _record_          |
| 100                  | ~0.024            | _record_          |
| 200                  | ~0.047            | _record_          |
| 400                  | ~0.094            | _record_          |

> Actual values depend on your specific LEDs, photodiode, and probe geometry.

---

## 10. NIR Spectroscopy Principle Explained

### Beer-Lambert Law

The attenuation of light through a medium is governed by:

```
I = I₀ × 10^(−ε·c·l)
```

Where:
- `I₀` = incident light intensity
- `I`  = transmitted light intensity
- `ε`  = molar absorptivity (wavelength-dependent constant)
- `c`  = concentration of absorbing species (glucose)
- `l`  = optical path length (finger thickness)

Rearranging:

```
Absorbance (A) = log₁₀(I₀/I) = ε × c × l
```

So **absorbance is proportional to concentration**.

### Why Ratio-Metric Measurement?

The path length `l` varies with finger size and probe placement. By computing:

```
R = A_940nm / A_660nm
```

the path-length dependence largely cancels out (similar to how pulse-oximeters
compute SpO₂ from the ratio of 660nm/940nm absorbances).

### Limitations of This Prototype

| Limitation | Reason | Mitigation |
|-----------|--------|-----------|
| Low SNR | Arduino 10-bit ADC, wide-band LEDs | 32× averaging, dark subtraction |
| Scattering | Tissue scatters NIR non-linearly | Not corrected in simple model |
| Motion artifacts | Probe movement during scan | User must keep still |
| Temperature drift | LED & PD response shifts with temp | NTC compensation applied |
| Individual variation | Skin tone, finger thickness | Per-user calibration needed |

---

## 11. Troubleshooting

| Symptom | Likely Cause | Solution |
|---------|-------------|---------|
| LCD shows nothing | Wrong I2C address | Change `0x27` to `0x3F` in sketch |
| All readings = 0 | A0/A1 not connected | Check TIA output wires to A0, A1 |
| Glucose stuck at max | Probe not light-sealed | Add black tape to probe sides |
| Wildly varying readings | Op-amp oscillating | Add 100pF cap in parallel with Rf |
| Button not triggering | Pull-down missing | Verify 10kΩ on D2 to GND |
| Serial shows `{...` garbage | Baud mismatch | Set Serial Monitor to 9600 |
| Temp reads -273°C | Thermistor disconnected | Check A2 wiring and 10kΩ divider |
| Buzzer always on | Glucose math overflow | Re-calibrate CALIB_A, CALIB_B |

---

## 12. Future Enhancements

| Enhancement | Description |
|------------|-------------|
| Better sensors | Replace IR LED + PD with AS7263 6-channel NIR spectral sensor |
| Pulse-ox integration | Add MAX30102 for simultaneous SpO₂ + pulse + NIR |
| Machine learning model | Collect multi-wavelength data → train regression or LSTM model |
| Bluetooth app | Add HC-05 module → Android app for trend logging |
| OLED upgrade | Replace LCD with 128×64 OLED for graph on device |
| Multi-user profiles | Store per-user calibration in EEPROM |
| Certified sensor chain | ESP32 + certified NIR module for research-grade prototype |
| Web dashboard | ESP8266/ESP32 for Wi-Fi MQTT to Grafana dashboard |

---

## 13. References & Further Reading

- Beer-Lambert Law: https://en.wikipedia.org/wiki/Beer%E2%80%93Lambert_law
- NIR Glucose Measurement Survey: Vashist, S.K. (2012). *Non-invasive glucose monitoring technology*. Diagnostics.
- BPW34 Photodiode Datasheet: Vishay Semiconductors
- LM358 Op-Amp Datasheet: Texas Instruments
- LiquidCrystal_I2C Library: https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
- Arduino Wire Library: https://www.arduino.cc/reference/en/language/functions/communication/wire/

---

*Part of the **Arduino Uno 100 Projects** series.*
*Author: vaibhavjagtap1 | Version: 1.0.0 | Board: Arduino Uno R3*
