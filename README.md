# Arduino Uno 100 Projects 🚀

A curated collection of **100 advanced Arduino Uno projects** spanning Smart Agriculture, Industrial Automation, Healthcare, Robotics, Renewable Energy, Smart City, and more. Each project ships with Arduino source code, a circuit diagram, a bill of materials, technical documentation, and reference datasheets — everything you need to build, learn, and extend.

---

## 📋 Table of Contents

1. [About the Repository](#about-the-repository)
2. [Prerequisites](#prerequisites)
3. [Installation & Setup](#installation--setup)
   - [Step 1 – Install the Arduino IDE](#step-1--install-the-arduino-ide)
   - [Step 2 – Install Required Drivers](#step-2--install-required-drivers)
   - [Step 3 – Configure the Arduino IDE](#step-3--configure-the-arduino-ide)
   - [Step 4 – Install Common Libraries](#step-4--install-common-libraries)
   - [Step 5 – Clone this Repository](#step-5--clone-this-repository)
4. [Repository Structure](#repository-structure)
5. [How to Use a Project](#how-to-use-a-project)
6. [Hardware You Will Need](#hardware-you-will-need)
7. [Project Index](#project-index)
8. [Contributing](#contributing)
9. [License](#license)

---

## About the Repository

This repository provides a ready-to-use framework for **100 real-world Arduino Uno projects**. Whether you are a beginner exploring electronics or an experienced engineer building prototypes, you will find projects covering:

| Domain | Example Projects |
|---|---|
| 🌱 Smart Agriculture | AI-Based Irrigation, Crop Health Monitor, Hydroponics |
| 🏭 Industrial Automation | CNC Controller, Conveyor Monitoring, Predictive Maintenance |
| 🏥 Healthcare / Biomedical | Blood Glucose Monitor, EEG Acquisition, Smart Stethoscope |
| 🤖 Robotics | Self-Balancing Robot, Line-Following Robot, Swarm Robotics |
| ⚡ Renewable Energy | Solar MPPT, Wind Harvester, EV Charging Station |
| 🌆 Smart City | Traffic Light Control, Flood Monitoring, Noise Heatmap |
| 🔒 Security Systems | Biometric ATM, Laser Fence, Smart Door Lock |
| 🛸 Aerospace / Drones | Weather Balloon Telemetry, Crop Spraying Drone |

---

## Prerequisites

Before you start, make sure you have the following:

- A **computer** running Windows 10/11, macOS 12+, or Ubuntu 20.04+
- An **Arduino Uno** board (or compatible clone)
- A **USB-A to USB-B** cable (the standard "printer" cable)
- **Internet access** for downloading software and libraries
- Basic familiarity with electronics (breadboards, resistors, jumper wires)

---

## Installation & Setup

### Step 1 – Install the Arduino IDE

The Arduino IDE is the official code editor and uploader for Arduino boards.

**Windows**
1. Go to [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Click **Windows Win 10 and newer, 64 bits** → Download
3. Run the downloaded `.exe` installer and follow the prompts
4. Accept the driver installation requests during setup

**macOS**
1. Go to [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Download the **macOS** `.dmg` file
3. Open the `.dmg`, drag **Arduino IDE** to your **Applications** folder
4. Open it once and grant the requested permissions

**Linux (Ubuntu/Debian)**
```bash
# Option A – AppImage (recommended)
wget https://downloads.arduino.cc/arduino-ide/arduino-ide_2.x.x_Linux_64bit.AppImage
chmod +x arduino-ide_2.x.x_Linux_64bit.AppImage
./arduino-ide_2.x.x_Linux_64bit.AppImage

# Option B – via snap
sudo snap install arduino-ide --classic
```

> 💡 **Tip:** Arduino IDE 2.x is recommended. It includes an integrated debugger, auto-complete, and a board/library manager.

---

### Step 2 – Install Required Drivers

The Arduino Uno uses a **CH340** or **ATmega16U2** USB-to-serial chip depending on the board variant.

**Windows (CH340 driver)**
1. Download: [https://www.wch-ic.com/downloads/CH341SER_EXE.html](https://www.wch-ic.com/downloads/CH341SER_EXE.html)
2. Run the installer and click **Install**
3. Reboot your computer

**macOS (CH340 driver)**
```bash
# Install via Homebrew (recommended)
brew install --cask wch-ch34x-usb-serial-driver
```
Or download the `.pkg` from the link above.

**Linux** — No extra driver needed; the kernel includes `ch341` and `cdc_acm` modules. You may need to add yourself to the `dialout` group:
```bash
sudo usermod -aG dialout $USER
# Log out and log back in for the change to take effect
```

---

### Step 3 – Configure the Arduino IDE

1. Connect your Arduino Uno to the computer with the USB cable
2. Open the **Arduino IDE**
3. Go to **Tools → Board → Arduino AVR Boards → Arduino Uno**
4. Go to **Tools → Port** and select the port labelled:
   - Windows: `COM3` (or similar, e.g. `COM5`)
   - macOS: `/dev/cu.usbmodem...` or `/dev/cu.wch-ch34x...`
   - Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
5. Click **Tools → Get Board Info** — if the board is detected, everything is working

---

### Step 4 – Install Common Libraries

Most projects in this collection depend on one or more of the following libraries. Install them through the **Arduino IDE Library Manager** (**Sketch → Include Library → Manage Libraries…**) by searching for the name shown below.

| Library | Purpose | Search Name |
|---|---|---|
| DHT sensor library | Temperature & Humidity (DHT11/22) | `DHT sensor library` |
| Adafruit Unified Sensor | Dependency for Adafruit libs | `Adafruit Unified Sensor` |
| LiquidCrystal I2C | I²C LCD displays | `LiquidCrystal I2C` |
| Wire | I²C communication (built-in) | built-in |
| SPI | SPI communication (built-in) | built-in |
| Servo | Servo motor control (built-in) | built-in |
| SoftwareSerial | Extra serial ports (built-in) | built-in |
| PID Library | PID control algorithms | `PID` by Brett Beauregard |
| MPU6050 / IMU | Gyroscope & Accelerometer | `MPU6050` by Electronic Cats |
| Adafruit NeoPixel | WS2812 RGB LEDs | `Adafruit NeoPixel` |
| HX711 | Load cell amplifier | `HX711 Arduino Library` |
| TinyGPS++ | GPS parsing | `TinyGPSPlus` |
| ArduinoJson | JSON serialisation | `ArduinoJson` |
| LoRa | LoRa radio communication | `LoRa` by Sandeep Mistry |
| MFRC522 | RFID reader | `MFRC522` |
| Keypad | Matrix keypad | `Keypad` by Mark Stanley |
| Stepper | Stepper motor control (built-in) | built-in |
| SD | SD card read/write (built-in) | built-in |
| WiFiNINA / ESP8266WiFi | WiFi connectivity | `WiFiNINA` or `ESP8266WiFi` |
| Adafruit Fingerprint | Fingerprint sensor | `Adafruit Fingerprint Sensor Library` |

> 💡 Each individual project's `Components/BOM.json` and `Documentation/` folder lists the exact libraries required for that project.

---

### Step 5 – Clone this Repository

```bash
# Using HTTPS
git clone https://github.com/vaibhavjagtap1/Arduino_Uno_100_Project.git

# Using SSH (if you have an SSH key configured)
git clone git@github.com:vaibhavjagtap1/Arduino_Uno_100_Project.git

# Move into the repository
cd Arduino_Uno_100_Project
```

---

## Repository Structure

```
Arduino_Uno_100_Project/
├── README.md                    ← You are here
├── LICENSE
├── create_projects.py           ← Script used to scaffold all 100 project folders
└── Projects/
    ├── 001_AI-Based_Smart_Irrigation_System.../
    │   ├── ArduinoCode/         ← .ino source files
    │   ├── CircuitDiagram/      ← Schematics (.png / .fzz / .pdf)
    │   ├── Documentation/       ← Technical reports & notes
    │   ├── Components/
    │   │   └── BOM.json         ← Bill of Materials (JSON)
    │   ├── Images/              ← Prototype photos
    │   ├── Datasheets/          ← Component datasheets (PDF)
    │   └── README.md            ← Project-specific description
    ├── 002_Industrial_Predictive_Maintenance.../
    │   └── ...
    └── 100_Smart_Campus_Central_Monitoring.../
        └── ...
```

Every project folder follows the same layout so you always know where to look.

---

## How to Use a Project

1. **Browse the Project Index** below and find a project that interests you
2. **Open the project folder** inside `Projects/`
3. **Read the project README** for an overview, difficulty level, and component list
4. **Check `Components/BOM.json`** for the complete bill of materials
5. **Install any additional libraries** listed in the documentation
6. **Open the sketch** in the Arduino IDE:
   - File → Open → navigate to `Projects/<project>/ArduinoCode/<sketch>.ino`
7. **Wire the circuit** using the schematic in `CircuitDiagram/`
8. **Select the correct board and port** (see Step 3 above)
9. **Click Upload** (→ button) and open the **Serial Monitor** (Ctrl+Shift+M) if needed

---

## Hardware You Will Need

The following items are used across many projects in this collection. Having them on hand lets you work through the most projects without waiting for parts:

| Item | Notes |
|---|---|
| Arduino Uno R3 | Main microcontroller board |
| USB-A to USB-B cable | For programming and serial power |
| Half-size breadboard | For prototyping circuits |
| Jumper wires (M-M, M-F, F-F) | At least 40 of each type |
| 10 kΩ & 1 kΩ resistor assortment | Pull-ups, current limiting |
| LEDs (assorted colours) | Visual indicators |
| DHT11 or DHT22 sensor | Temperature & humidity |
| 16×2 LCD display (I²C) | Output display |
| Servo motor (SG90) | Actuation |
| DC motor + L298N driver | Motor control |
| IR sensor module | Proximity / line following |
| Ultrasonic sensor (HC-SR04) | Distance measurement |
| Relay module (5 V) | Switching high-voltage loads |
| 9 V battery + connector | Portable power supply |
| MPU-6050 IMU module | Gyroscope & accelerometer |
| RFID reader (RC522) + tags | Access control |

---

## Project Index

<details>
<summary>Click to expand all 100 projects</summary>

| # | Project Name | Category |
|---|---|---|
| 001 | AI-Based Smart Irrigation System using Arduino and Soil Analytics | Smart Agriculture IoT |
| 002 | Industrial Predictive Maintenance System using Vibration Analysis | Industrial Automation IoT |
| 003 | Non-Invasive Blood Glucose Monitoring Device using NIR Spectroscopy | Healthcare Biomedical |
| 004 | Autonomous Line-Following Robot with Obstacle Avoidance and PID Control | Robotics Embedded Control |
| 005 | Smart Energy Management System for Solar-Battery Microgrid | Renewable Energy Smart Grid |
| 006 | Smart Water Quality Monitoring Buoy | Environmental IoT |
| 007 | Real-Time Air Quality Index Monitor | Environmental IoT |
| 008 | Wireless Sensor Network with LoRa | IoT Systems |
| 009 | Smart Metering for Industrial Power | Industrial IoT |
| 010 | Environmental Data Logger | Environmental Monitoring |
| 011 | Automated Weather Station | Environmental Monitoring |
| 012 | Smart Building Energy Management | Smart City Energy |
| 013 | AI-Based Smart Irrigation System | Smart Agriculture |
| 014 | Crop Health Monitoring | Smart Agriculture |
| 015 | Automated Pest Detection | Smart Agriculture |
| 016 | Smart Soil Nutrient Analyzer | Smart Agriculture |
| 017 | Livestock Tracking with GPS | Smart Agriculture |
| 018 | Rainfall Prediction Model | Smart Agriculture |
| 019 | Smart Irrigation Valve NB-IoT | Smart Agriculture |
| 020 | Solar Drying System | Renewable Energy |
| 021 | Automated Hydroponics | Smart Agriculture |
| 022 | Greenhouse Climate Control | Smart Agriculture |
| 023 | Smart Weighbridge | Industrial Automation |
| 024 | Precision Farming Network | Smart Agriculture |
| 025 | Plant Disease Detection AI | AI Agriculture |
| 026 | Smart Vertical Farm | Smart Agriculture |
| 027 | Underwater ROV with Depth Control and Camera Feed | Robotics |
| 028 | Wearable Fall Detection Alert System for Elderly Care | Healthcare IoT |
| 029 | Wind Energy Harvester with MPPT and Battery Charging | Renewable Energy |
| 030 | Capacitive Soil Nutrient Analyzer with N-P-K Estimation | Smart Agriculture |
| 031 | Factory Floor Occupancy Counter with Thermal Imaging Array | Industrial IoT |
| 032 | Smart EV Charging Station with Load Balancing | EV Smart Energy |
| 033 | Real-Time Fire Detection and Suppression Trigger System | Safety Automation |
| 034 | Machine Vision Object Sorting Conveyor with Color Detection | Industrial Automation |
| 035 | Piezoelectric Energy Harvesting Floor Tile Monitor | Energy Harvesting |
| 036 | Structural Health Monitoring System for Bridges (SHM) | Civil Engineering IoT |
| 037 | Smart Helmet with Accident Detection and Auto SOS | Safety IoT |
| 038 | Automated Plant Disease Detection via Leaf Image Analysis | AI Agriculture |
| 039 | Self-Charging Autonomous Lawn Mower | Consumer Robotics |
| 040 | Electronic Nose for Food Quality Detection using Gas Sensor Array | Food Technology IoT |
| 041 | Self-Balancing Robot using Complementary Filter and PID | Robotics |
| 042 | Multi-Zone HVAC Controller with Machine Learning Scheduling | Building Automation |
| 043 | Smart Traffic Light Control with Vehicle Density Estimation | Smart City |
| 044 | CNC Pen Plotter with G-Code Interpreter | Industrial Automation |
| 045 | Automatic Solar Panel Cleaning Robot | Renewable Energy |
| 046 | Voice-Controlled Home Automation using CNN Keyword Detection | AI Home Automation |
| 047 | Water Leakage Detection System in Building Pipelines | Smart Building |
| 048 | Anti-Drone Frequency Jammer Detection System | Industrial Safety |
| 049 | Magnetic Levitation Display System with PID Height Control | Advanced Electronics |
| 050 | Multi-Axis CNC Milling Machine Controller | Industrial Automation |
| 051 | Smart Waste Segregation System with Image Classification | Smart City |
| 052 | Load Cell Vending Machine Controller | Industrial Automation |
| 053 | Electroencephalogram (EEG) Signal Acquisition System | Healthcare Biomedical |
| 054 | Smart Aquarium with Auto Feeding, Lighting, and Water Management | IoT Aquaculture |
| 055 | Autonomous Greenhouse Rover with Computer Vision for Weed Detection | Agri-Robotics |
| 056 | Vehicle-to-Grid Communication Protocol Simulator (V2G) | EV Smart Grid |
| 057 | Biometric ATM Security System with Fingerprint and Face Liveness | Security Systems |
| 058 | Smart Fermentation Monitor for Craft Beer Brewing | Industrial IoT |
| 059 | Wearable EMG-Based Prosthetic Hand Controller | Assistive Technology |
| 060 | High-Altitude Weather Balloon Telemetry System | Aerospace IoT |
| 061 | Smart Irrigation Valve with NB-IoT Connectivity | Smart Agriculture |
| 062 | Haptic Navigation System for Blind Individuals | Assistive Technology |
| 063 | Automated PCB Etching Tank Temperature and Agitation Controller | Industrial Automation |
| 064 | Smart Grid Demand Response Load Controller | Smart Energy |
| 065 | Forest Fire Early Warning Network using Mesh Topology | Environmental Monitoring |
| 066 | Smart Stethoscope with Digital Auscultation and AI Classification | Healthcare Biomedical |
| 067 | Underground Mining Safety Monitoring System | Industrial Safety |
| 068 | Real-Time Drowning Detection System for Swimming Pools | Safety IoT |
| 069 | Swarm Robotics System with Inter-Robot Communication | Robotics |
| 070 | Automated Water ATM with Smart Card and Flow Control | Smart City IoT |
| 071 | Neural Prosthetic Signal Processing Unit | Healthcare Biomedical |
| 072 | Smart Cold Chain Monitoring System for Pharmaceutical Logistics | Healthcare IoT |
| 073 | Automated Dam Gate Control with Flood Prediction Algorithm | Water Management IoT |
| 074 | Solar-Powered Autonomous Air Pollution Sampling Drone | Aerospace Environmental |
| 075 | Smart Cement Curing Monitor for Construction Sites | Civil Engineering IoT |
| 076 | Industrial Boiler Pressure and Temperature Safety Controller | Industrial Safety |
| 077 | Laser Security Fence with Intrusion Zone Identification | Security Systems |
| 078 | Automated Livestock Feeding and Health Monitoring System | Smart Agriculture |
| 079 | Active Noise Cancellation System using DSP on Arduino | Audio Engineering |
| 080 | Smart Anaesthesia Machine Monitoring and Alert System | Healthcare Biomedical |
| 081 | Automated Bee Colony Health Monitor with Acoustic Analysis | Smart Agriculture |
| 082 | High-Power LED Grow Light Controller with Spectrum Tuning | Smart Agriculture |
| 083 | Blockchain-Integrated Supply Chain Tracker for Pharmaceuticals | Blockchain IoT |
| 084 | IoT-Based Smart Classroom Attendance and Environment Monitor | IoT Systems |
| 085 | Drone-Based Crop Spraying Controller with GPS Waypoints | Agri Drones |
| 086 | Smart Railway Level Crossing Automation and Monitoring | Smart City Safety |
| 087 | Wearable Posture Correction System using IMU | Assistive Technology |
| 088 | Industrial Conveyor Belt Speed and Jam Detection System | Industrial Automation |
| 089 | Smart Street Parking Guidance with Mobile App Backend Stub | Smart City |
| 090 | IoT-Based Flood Level Monitoring and Siren System | Environmental Safety |
| 091 | Smart Home Energy Disaggregation Prototype | Smart Energy |
| 092 | Warehouse Inventory Tracker with RFID and Handheld Scanner | Industrial IoT |
| 093 | Smart City Noise Pollution Heatmap Node | Environmental Monitoring |
| 094 | TinyML Keyword Spotting Wake-Word Node | AI Arduino TinyML |
| 095 | Smart Wheelchair Controller with Obstacle Avoidance | Assistive Technology |
| 096 | Heliostat Solar Concentrator Mirror Positioning System | Renewable Energy |
| 097 | Multi-Room BLE Beacon Indoor Positioning Demo | IoT Systems |
| 098 | Smart Door Lock with OTP over WiFi Prototype | Security Systems |
| 099 | Helmet-Mounted Head-Up Display (HUD) Prototype | Aerospace / Automotive |
| 100 | Smart Campus Central Monitoring Dashboard Node | Smart City IoT |

</details>

---

## Contributing

Contributions are welcome! To add code, schematics, or documentation for any project:

1. **Fork** this repository
2. Create a new branch: `git checkout -b feature/project-XXX-description`
3. Add your files inside the appropriate `Projects/XXX_.../` sub-folder
4. Commit your changes: `git commit -m "Add Arduino code for project 004"`
5. Push to your fork: `git push origin feature/project-XXX-description`
6. Open a **Pull Request** against `main`

Please follow these guidelines:
- Keep Arduino sketches well-commented
- Include a wiring diagram or updated schematic in `CircuitDiagram/`
- Update `Components/BOM.json` with any new parts
- Test your code on real hardware before submitting

---

## License

This project is licensed under the terms of the [LICENSE](LICENSE) file included in this repository.

---

> Made with ❤️ for the Arduino community. Happy building! 🔧