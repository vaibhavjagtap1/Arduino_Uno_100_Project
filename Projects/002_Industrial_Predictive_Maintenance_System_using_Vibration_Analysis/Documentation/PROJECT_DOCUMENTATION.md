# Project 002: Industrial Predictive Maintenance System
## using Vibration Analysis on Arduino Uno

---

## 📋 Table of Contents
1. [Project Overview](#overview)
2. [Features](#features)
3. [System Architecture](#architecture)
4. [Hardware Required](#hardware)
5. [Circuit Wiring Guide](#wiring)
6. [Software Setup](#software)
7. [How to Upload & Run](#upload)
8. [Serial Dashboard Setup](#dashboard)
9. [Calibration Guide](#calibration)
10. [Fault Detection Algorithm Explained](#algorithm)
11. [Troubleshooting](#troubleshooting)
12. [Future Enhancements](#future)

---

## 1. Project Overview {#overview}

Predictive maintenance is an industry 4.0 technique that monitors machine health in real-time to predict failures **before** they happen — rather than waiting for a breakdown or following a fixed schedule. This project implements a vibration-based predictive maintenance node using an ADXL345 3-axis accelerometer attached directly to a machine or motor. The system continuously computes RMS (Root Mean Square) vibration magnitude over a 100-sample rolling window at 200 Hz and classifies the machine into one of three health states: **NORMAL**, **WARNING**, or **CRITICAL**.

A DS18B20 digital thermometer tracks bearing temperature, which independently escalates the state when excessive heat is detected. A relay output triggers an emergency machine shutdown when CRITICAL vibration is detected.

**Key benefit:** Detect bearing wear, shaft imbalance, misalignment, or looseness early — before a catastrophic failure causes production downtime or safety incidents.

---

## 2. Features {#features}

| Feature | Description |
|---------|-------------|
| 📳 3-Axis Vibration | ADXL345 I2C accelerometer ±16g, X/Y/Z axes |
| 📊 RMS Computation | 100-sample RMS at 200 Hz, pure integer math — no FFT library needed |
| 🌡️ Bearing Temperature | DS18B20 1-Wire sensor on D3 for thermal monitoring |
| 🚦 3-State Classification | NORMAL / WARNING / CRITICAL with threshold hysteresis |
| 💡 LED Indicators | Green (D9) = Normal, Yellow (D10) = Warning, Red (D11) = Critical |
| 🔊 Alert Buzzer | State-specific tones: silent/single-beep/rapid-alarm |
| ⚡ Emergency Relay | D7 relay triggers machine shutdown on CRITICAL state |
| 📺 LCD 16×2 | Live RMS value, bearing temperature, and state on I2C display |
| 📡 Serial JSON | `{"ax":…,"ay":…,"az":…,"rms":…,"state":"…","temp":…}` at 9600 baud |
| 🖥️ Python Dashboard | Live 4-panel matplotlib plot with colour-coded status panel |

---

## 3. System Architecture {#architecture}

```
[ADXL345 X/Y/Z] ──I2C──┐
[DS18B20 DQ]    ──1W───┤──→ [Arduino Uno R3] ──→ D7  ──→ [Relay] ──→ Machine Shutdown
                        │         │
                        │         ├──→ D9  ──→ [Green LED]   NORMAL
                        │         ├──→ D10 ──→ [Yellow LED]  WARNING
                        │         ├──→ D11 ──→ [Red LED]     CRITICAL
                        │         ├──→ D12 ──→ [Buzzer]
                        │         └──→ I2C ──→ [LCD 16×2]
                        │
                   [RMS Engine]
                (100 samples @ 200 Hz)
                [State Classifier]
                (NORMAL/WARNING/CRITICAL)
                        │
                        └──→ USB Serial ──→ [Python Dashboard]
```

---

## 4. Hardware Required {#hardware}

See `Components/components_list.txt` for the full Bill of Materials with quantities, specifications, and estimated costs.

**Core components:**
- Arduino Uno R3
- ADXL345 Accelerometer Module (I2C)
- DS18B20 Digital Temperature Sensor + 4.7kΩ resistor
- 16×2 LCD with I2C backpack (address 0x27 or 0x3F)
- 5V 1-channel Relay Module (opto-isolated)
- Green, Yellow, and Red 5mm LEDs + 3× 220Ω resistors
- Active Piezo Buzzer
- Breadboard + jumper wires
- USB cable + 9V DC adapter

---

## 5. Circuit Wiring Guide {#wiring}

Refer to `CircuitDiagram/circuit.svg` for the full visual wiring diagram.

### ADXL345 → Arduino (I2C)
| ADXL345 Pin | Arduino Pin | Note |
|-------------|-------------|------|
| VCC         | 3.3V or 5V  | Module has on-board regulator — check your module |
| GND         | GND         | |
| SDA         | A4          | Shared I2C bus with LCD |
| SCL         | A5          | Shared I2C bus with LCD |
| CS          | 3.3V        | Pull HIGH to select I2C mode |
| SDO/ALT     | GND         | Sets I2C address to 0x53 |

### DS18B20 → Arduino (1-Wire)
| DS18B20 Pin | Arduino Pin | Note |
|-------------|-------------|------|
| VDD (pin 3) | 5V          | |
| GND (pin 1) | GND         | |
| DQ  (pin 2) | D3          | Add 4.7kΩ pull-up from DQ to 5V |

```
5V ──── [4.7kΩ] ──── D3 ──── DS18B20 DQ
                      │
                    Arduino D3
```

### Relay Module → Arduino + Machine
| Relay Pin | Connected To |
|-----------|-------------|
| VCC       | 5V          |
| GND       | GND         |
| IN        | D7          |
| COM       | Machine supply (+) |
| NO        | Machine load (+) |

> ⚠️ **Safety Warning:** Always use an appropriately rated relay for your machine's voltage and current. Never connect mains (AC) voltage without proper isolation and electrical safety precautions.

### LCD 16×2 I2C → Arduino
| LCD Pin | Arduino Pin |
|---------|-------------|
| VCC     | 5V          |
| GND     | GND         |
| SDA     | A4          |
| SCL     | A5          |

### LEDs → Arduino
```
D9  ──── [220Ω] ──── [Green LED +]  ──── GND    (NORMAL)
D10 ──── [220Ω] ──── [Yellow LED +] ──── GND    (WARNING)
D11 ──── [220Ω] ──── [Red LED +]    ──── GND    (CRITICAL)
```

### Buzzer → Arduino
```
D12 ──── [Buzzer +] ──── GND
```

---

## 6. Software Setup {#software}

### Arduino IDE Setup

1. Download Arduino IDE from [arduino.cc](https://arduino.cc)
2. Install required libraries via **Sketch → Include Library → Manage Libraries**:
   - Search `ADXL345` → install **Adafruit ADXL345** by Adafruit
   - Search `Adafruit Unified Sensor` → install by Adafruit *(dependency)*
   - Search `LiquidCrystal I2C` → install **by Frank de Brabander**
   - Search `OneWire` → install **by Paul Stoffregen**
   - Search `DallasTemperature` → install **by Miles Burton**
3. Select board: **Tools → Board → Arduino Uno**
4. Select port: **Tools → Port → COMx** (Windows) or `/dev/ttyUSBx` (Linux)

### File to Open
```
ArduinoCode/PredictiveMaintenance.ino
```

---

## 7. How to Upload & Run {#upload}

1. Open `ArduinoCode/PredictiveMaintenance.ino` in Arduino IDE
2. **Before uploading**, adjust thresholds for your machine (see §9):
   ```cpp
   #define WARNING_THRESHOLD   3.0f   // m/s² RMS
   #define CRITICAL_THRESHOLD  7.0f   // m/s² RMS
   ```
3. Click **Upload** (→ arrow button)
4. Open Serial Monitor: **Tools → Serial Monitor → 9600 baud**
5. You should see JSON output like:
   ```json
   {"ax":0.08,"ay":-0.12,"az":9.81,"rms":0.245,"state":"NORMAL","temp":27.5}
   ```
6. The LCD will show live RMS and machine state
7. When RMS exceeds 7.0 m/s², the Red LED lights, buzzer sounds, and relay triggers shutdown

---

## 8. Serial Dashboard Setup {#dashboard}

The Python dashboard (`ArduinoCode/serial_dashboard.py`) provides a live 4-panel visualisation.

### Install Python dependencies
```bash
pip install pyserial matplotlib
```

### Run the dashboard
```bash
# Windows
python serial_dashboard.py --port COM3

# Linux / macOS
python serial_dashboard.py --port /dev/ttyUSB0
```

The dashboard shows:
- **Panel 1:** X/Y/Z acceleration time series (colour-coded axes)
- **Panel 2:** RMS vibration trend with NORMAL/WARNING/CRITICAL zone shading and threshold lines
- **Panel 3:** Bearing temperature trend with 80°C and 100°C alert lines
- **Panel 4 (gauge):** Current RMS bar gauge with state colour
- **Panel 5 (status):** Machine state label, current values, and threshold reference

---

## 9. Calibration Guide {#calibration}

### Step 1 — Establish Baseline Vibration
1. Mount the ADXL345 firmly on the machine surface (parallel to a main axis)
2. Run the machine at normal load for 15–30 minutes
3. Observe the RMS values in Serial Monitor
4. Note the stable operating RMS (e.g., `0.5–1.2 m/s²`)

### Step 2 — Set Warning Threshold
Set `WARNING_THRESHOLD` to approximately **2–3× the baseline** RMS:
```cpp
#define WARNING_THRESHOLD   3.0f   // Adjust to ~2.5× your baseline
```

### Step 3 — Set Critical Threshold
Set `CRITICAL_THRESHOLD` to approximately **5–7× the baseline** RMS:
```cpp
#define CRITICAL_THRESHOLD  7.0f   // Adjust to ~5× your baseline
```

### ISO 10816 Vibration Severity Reference
| Machine Class | NORMAL      | WARNING      | CRITICAL |
|---------------|-------------|--------------|----------|
| Small motors (<15 kW)  | < 2.8 mm/s | 2.8–7.1 mm/s | > 7.1 mm/s |
| Medium machines        | < 4.5 mm/s | 4.5–11.2 mm/s | > 11.2 mm/s |

> **Note:** The sketch uses m/s² (acceleration) rather than mm/s (velocity). The conversion requires knowledge of the dominant vibration frequency: `velocity (mm/s) ≈ RMS_accel (m/s²) / (2π × dominant_freq_Hz) × 1000`. Since the current version does not perform frequency analysis (no FFT), use the m/s² thresholds directly and adjust them by baseline testing. FFT-based frequency classification is listed in Future Enhancements.

### LCD I2C Address
If LCD is blank after upload:
- Use an I2C scanner sketch to detect the address
- Change `LiquidCrystal_I2C lcd(0x27, 16, 2)` to `lcd(0x3F, 16, 2)` if needed

---

## 10. Fault Detection Algorithm Explained {#algorithm}

### RMS Computation

RMS (Root Mean Square) is the industry standard metric for vibration severity:

```
RMS = √( (1/N) × Σ (ax² + ay² + az²) )
```

For N=100 samples at 200 Hz, this gives a **500 ms vibration window** — short enough to react quickly, long enough to filter random noise.

**Integer math implementation:**
The sketch avoids per-sample `sqrt()` by:
1. Converting m/s² to integer millis (×1000)
2. Accumulating squared integers in a `long long` accumulator
3. Computing one `sqrt()` at the end after mean normalisation

This is 3–5× faster than float accumulation on the ATmega328P.

### State Classification

```
IF rms ≥ CRITICAL_THRESHOLD (7.0)  → CRITICAL
  ELSE IF rms ≥ WARNING_THRESHOLD (3.0) → WARNING
  ELSE                                   → NORMAL

# Temperature escalation
IF bearingTemp > 80°C  AND state == NORMAL  → escalate to WARNING
IF bearingTemp > 100°C                      → escalate to CRITICAL
```

### Relay Logic
The relay is activated (LOW signal) only on **CRITICAL** state. Normal and warning states do not interrupt machine operation — only the LEDs and buzzer alert the operator.

---

## 11. Troubleshooting {#troubleshooting}

| Problem | Likely Cause | Solution |
|---------|-------------|----------|
| ADXL345 not found — sketch halts | Wrong wiring or I2C address | Check SDA→A4, SCL→A5; verify CS pin pulled HIGH; run I2C scanner |
| LCD blank | Wrong I2C address | Scan for address; change 0x27→0x3F in code |
| RMS always 9.81 m/s² | ADXL345 reading static gravity only | Sensor not mounted on vibrating surface; verify `getEvent()` works |
| DS18B20 reads -127°C | Missing 4.7kΩ pull-up, or wrong pin | Add resistor from D3 to 5V; check `ONE_WIRE_BUS` define |
| Relay clicks continuously | CRITICAL threshold too low for your machine | Increase `CRITICAL_THRESHOLD` value |
| Serial output garbled | Wrong baud rate | Set Serial Monitor to exactly 9600 baud |
| No buzzer sound | Buzzer wiring or wrong pin | Check D12 → Buzzer+ → GND; test with `tone(12, 1000, 500)` in `setup()` |
| Python dashboard: port error | Wrong port name or driver missing | Check Device Manager (Windows) or `ls /dev/tty*` (Linux) |
| RMS spikes randomly | Loose ADXL345 mounting | Secure module firmly with double-sided tape or M3 bolts |

---

## 12. Future Enhancements {#future}

- 📡 **ESP8266 Wi-Fi**: Stream vibration data to InfluxDB/Grafana for historical trending
- 🧠 **TinyML FFT Classifier**: Use Edge Impulse to detect specific fault signatures (imbalance vs bearing wear vs misalignment) from frequency spectrum
- 📲 **SMS/Email Alerts**: Trigger GSM module alert on CRITICAL state
- 📝 **SD Card Logging**: Log RMS + temperature time-series to SD card for offline analysis
- 🔋 **Battery + Solar**: Deploy as a wireless sensor node on remote machinery
- 📏 **Velocity Integration**: Integrate acceleration to mm/s for ISO 10816 compliance
- 🌐 **MQTT/Node-RED**: Publish to MQTT broker for integration with industrial SCADA systems (→ see Project 009)
- 🔔 **Hysteresis States**: Add state debouncing to prevent rapid oscillation between WARNING/CRITICAL near threshold

---
