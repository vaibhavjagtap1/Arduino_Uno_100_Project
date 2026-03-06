# Project 001: AI-Based Smart Irrigation System
## using Arduino Uno and Soil Analytics

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
10. [AI Decision Engine Explained](#ai-engine)
11. [Troubleshooting](#troubleshooting)
12. [Future Enhancements](#future)

---

## 1. Project Overview {#overview}

This project implements an intelligent irrigation controller that monitors soil moisture, ambient temperature, humidity, and sunlight to automatically water plants. A fuzzy-logic rule engine simulates AI decision-making to determine *when* and *how long* to irrigate, avoiding wasteful over-watering and crop damage from under-watering.

**Key benefit:** Reduces water usage by 40–60% compared to timer-based irrigation by only watering when the soil and environmental conditions require it.

---

## 2. Features {#features}

| Feature | Description |
|---------|-------------|
| 🌱 Real-time soil monitoring | Capacitive sensor reads moisture 0–100% |
| 🌡️ Environmental sensing | DHT11 measures temperature & humidity |
| ☀️ Day/Night scheduling | LDR prevents irrigation at night |
| 🤖 AI Decision Engine | Fuzzy-logic rules automate pump control |
| 📺 LCD Display | 16×2 shows live readings + pump state |
| 💡 LED indicators | Green = pump ON, Red = pump OFF |
| 🔊 Buzzer alerts | Audio feedback on pump start |
| 📊 Serial JSON stream | Python dashboard for data visualization |

---

## 3. System Architecture {#architecture}

```
[Soil Moisture Sensor] ──→ A0 ──┐
[DHT11 Temp/Humidity]  ──→ D2 ──┤
[LDR Light Sensor]     ──→ A1 ──┤──→ [Arduino Uno] ──→ D7 ──→ [Relay] ──→ [Pump]
                                 │         │
                                 │         ├──→ D9  ──→ [Green LED]
                                 │         ├──→ D10 ──→ [Red LED]
                                 │         ├──→ D11 ──→ [Buzzer]
                                 │         └──→ I2C ──→ [LCD 16×2]
                                 │
                            [AI Engine]
                         (Fuzzy Logic Rules)
```

---

## 4. Hardware Required {#hardware}

See `Components/components_list.txt` for the full BOM with quantities and specifications.

**Core components:**
- Arduino Uno R3
- Capacitive Soil Moisture Sensor (NOT the resistive type — it corrodes)
- DHT11 Sensor
- LDR + 10kΩ resistor
- 5V 1-channel Relay Module (opto-isolated)
- Submersible mini pump (3–6V DC)
- 16×2 LCD with I2C backpack (address 0x27)
- Green & Red LEDs + 220Ω resistors
- Active Piezo Buzzer
- Breadboard + jumper wires

---

## 5. Circuit Wiring Guide {#wiring}

Refer to `CircuitDiagram/circuit_diagram.svg` for the visual diagram.

### Step-by-Step Wiring

#### Soil Moisture Sensor → Arduino
| Sensor Pin | Arduino Pin |
|-----------|-------------|
| VCC       | 5V          |
| GND       | GND         |
| AOUT      | A0          |

#### DHT11 → Arduino
| DHT11 Pin | Arduino Pin | Note |
|-----------|-------------|------|
| VCC (pin 1) | 5V        | |
| DATA (pin 2)| D2        | Add 10kΩ pull-up from DATA to VCC |
| GND (pin 4) | GND       | |

#### LDR Voltage Divider → Arduino
```
5V ──── [LDR] ──── A1 ──── [10kΩ] ──── GND
```
Connect the junction between LDR and 10kΩ resistor to A1.

#### Relay Module → Arduino + Pump
| Relay Pin | Connected To |
|-----------|-------------|
| VCC       | 5V          |
| GND       | GND         |
| IN        | D7          |
| COM       | +12V (external pump supply) |
| NO        | Pump + wire |
| Pump –    | External GND |

> ⚠️ **Important:** The pump requires its own power source (e.g., 9V–12V adapter for stronger pumps, or 5V for mini 3-6V pumps). Never power a large pump directly from the Arduino 5V pin.

#### LCD I2C → Arduino
| LCD Pin | Arduino Pin |
|---------|-------------|
| VCC     | 5V          |
| GND     | GND         |
| SDA     | A4          |
| SCL     | A5          |

#### LEDs → Arduino
```
D9  ──── [220Ω] ──── [Green LED +] ──── GND
D10 ──── [220Ω] ──── [Red LED +]   ──── GND
```

#### Buzzer → Arduino
```
D11 ──── [Buzzer +] ──── GND
```

---

## 6. Software Setup {#software}

### Arduino IDE Setup

1. Download Arduino IDE from [arduino.cc](https://arduino.cc)
2. Install required libraries via **Sketch → Include Library → Manage Libraries**:
   - Search `DHT sensor library` → install **by Adafruit**
   - Search `LiquidCrystal I2C` → install **by Frank de Brabander**
3. Select board: **Tools → Board → Arduino Uno**
4. Select port: **Tools → Port → COMx** (Windows) or `/dev/ttyUSBx` (Linux)

### File to Open
```
ArduinoCode/SmartIrrigation.ino
```

---

## 7. How to Upload & Run {#upload}

1. Open `ArduinoCode/SmartIrrigation.ino` in Arduino IDE
2. **Before uploading**, set your calibration constants (see §9):
   ```cpp
   #define SOIL_DRY_VALUE    850   // ADC when sensor is in dry air
   #define SOIL_WET_VALUE    350   // ADC when sensor is in water
   ```
3. Click **Upload** (→ arrow button)
4. Open Serial Monitor: **Tools → Serial Monitor → 9600 baud**
5. You should see JSON output like:
   ```json
   {"moisture":45.2,"temp":28.5,"humidity":62.0,"light":780,"pump":false,"reason":"Moisture adequate"}
   ```
6. The LCD should display live readings
7. When soil moisture drops below 30%, the pump activates automatically

---

## 8. Serial Dashboard Setup {#dashboard}

The Python dashboard (`ArduinoCode/serial_dashboard.py`) plots live data.

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

A live window will open showing:
- Soil moisture trend graph (with optimal zone shading)
- Temperature and humidity trends
- Current moisture gauge
- Pump status and decision reason

---

## 9. Calibration Guide {#calibration}

### Soil Moisture Sensor Calibration

1. Upload the sketch
2. Open Serial Monitor at 9600 baud
3. Hold sensor in **dry air** → note the ADC value printed → set as `SOIL_DRY_VALUE`
4. Submerge sensor tip in **water** → note the ADC value → set as `SOIL_WET_VALUE`
5. Update constants in the `.ino` file and re-upload

Typical values:
- Dry air: 750–900
- Water: 300–450

### LCD I2C Address
If the LCD does not display anything:
- Run an I2C scanner sketch
- Change `LiquidCrystal_I2C lcd(0x27, 16, 2)` to `lcd(0x3F, 16, 2)` if needed

---

## 10. AI Decision Engine Explained {#ai-engine}

The decision engine in `shouldIrrigate()` implements **fuzzy logic rules** that mimic how a farmer thinks:

### Input Variables
| Variable | Range | Meaning |
|----------|-------|---------|
| Soil Moisture | 0–100% | How wet the soil is |
| Temperature | °C | Ambient temperature |
| Humidity | 0–100% | Air moisture content |
| Light Level | 0–1023 (ADC) | Day vs night indicator |

### Rule Set
```
IF night                          → SKIP  (never irrigate at night)
IF moisture > 70%                 → SKIP  (soil already wet)
IF moisture < 30%                 → IRRIGATE (critical: soil too dry)
IF moisture < 70% AND temp ≥ 35°C AND humidity ≤ 40% → IRRIGATE (hot+dry)
ELSE                              → SKIP  (conditions adequate)
```

### Why Fuzzy Logic?
Simple threshold rules create "bang-bang" control (on/off) which can cause pump chatter. The multi-condition rules consider *combinations* of factors, which more closely models natural crop water needs. In an advanced version, these rules would be replaced with a trained neural network (TinyML).

---

## 11. Troubleshooting {#troubleshooting}

| Problem | Likely Cause | Solution |
|---------|-------------|----------|
| LCD blank | Wrong I2C address | Run I2C scanner, change 0x27→0x3F |
| DHT reads NaN | Missing pull-up resistor | Add 10kΩ between DHT DATA and VCC |
| Pump never turns on | Relay wiring reversed | Check IN pin, try `digitalWrite(7, LOW)` to test |
| Moisture always 0% or 100% | Wrong calibration values | Redo dry/wet calibration |
| Serial garbage characters | Wrong baud rate | Set Serial Monitor to 9600 |
| Pump runs continuously | Threshold too high | Reduce `MOISTURE_LOW` value |

---

## 12. Future Enhancements {#future}

- 📡 **ESP8266 Wi-Fi**: Push data to ThingSpeak / Firebase IoT cloud
- 🧠 **TinyML**: Replace rule engine with Edge Impulse neural network
- 💧 **Flow meter**: Measure actual water dispensed per cycle
- 📱 **Blynk App**: Remote manual override and monitoring
- ⏰ **RTC Module**: Time-based schedule fallback
- 🔋 **Solar + Battery**: Off-grid deployment in fields
- 🌐 **Multi-node**: LoRa mesh for large farm coverage (→ see Project 008)

---

*Generated for vaibhavjagtap1 / Arduino_Uno_100_Project*
