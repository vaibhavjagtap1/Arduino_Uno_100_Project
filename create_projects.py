#!/usr/bin/env python3
"""
Automated Script to Create 100 Arduino Project Folder Structure - COMPLETE VERSION
Run this script locally to generate all 100 project directories
"""

import json
from pathlib import Path

# Projects 1–83 as before, plus 84–100 added.
# For 84–100 I'm using generic but sensible placeholders; you can edit names/categories later.
projects = [
    (1, "AI-Based Smart Irrigation System using Arduino and Soil Analytics", "Smart Agriculture IoT"),
    (2, "Industrial Predictive Maintenance System using Vibration Analysis", "Industrial Automation IoT"),
    (3, "Non-Invasive Blood Glucose Monitoring Device using NIR Spectroscopy", "Healthcare Biomedical"),
    (4, "Autonomous Line-Following Robot with Obstacle Avoidance and PID Control", "Robotics Embedded Control"),
    (5, "Smart Energy Management System for Solar-Battery Microgrid", "Renewable Energy Smart Grid"),
    (6, "Smart Water Quality Monitoring Buoy", "Environmental IoT"),
    (7, "Real-Time Air Quality Index Monitor", "Environmental IoT"),
    (8, "Wireless Sensor Network with LoRa", "IoT Systems"),
    (9, "Smart Metering for Industrial Power", "Industrial IoT"),
    (10, "Environmental Data Logger", "Environmental Monitoring"),
    (11, "Automated Weather Station", "Environmental Monitoring"),
    (12, "Smart Building Energy Management", "Smart City Energy"),
    (13, "AI-Based Smart Irrigation System", "Smart Agriculture"),
    (14, "Crop Health Monitoring", "Smart Agriculture"),
    (15, "Automated Pest Detection", "Smart Agriculture"),
    (16, "Smart Soil Nutrient Analyzer", "Smart Agriculture"),
    (17, "Livestock Tracking with GPS", "Smart Agriculture"),
    (18, "Rainfall Prediction Model", "Smart Agriculture"),
    (19, "Smart Irrigation Valve NB-IoT", "Smart Agriculture"),
    (20, "Solar Drying System", "Renewable Energy"),
    (21, "Automated Hydroponics", "Smart Agriculture"),
    (22, "Greenhouse Climate Control", "Smart Agriculture"),
    (23, "Smart Weighbridge", "Industrial Automation"),
    (24, "Precision Farming Network", "Smart Agriculture"),
    (25, "Plant Disease Detection AI", "AI Agriculture"),
    (26, "Smart Vertical Farm", "Smart Agriculture"),
    (27, "Underwater ROV with Depth Control and Camera Feed", "Robotics"),
    (28, "Wearable Fall Detection Alert System for Elderly Care", "Healthcare IoT"),
    (29, "Wind Energy Harvester with MPPT and Battery Charging", "Renewable Energy"),
    (30, "Capacitive Soil Nutrient Analyzer with N-P-K Estimation", "Smart Agriculture"),
    (31, "Factory Floor Occupancy Counter with Thermal Imaging Array", "Industrial IoT"),
    (32, "Smart EV Charging Station with Load Balancing", "EV Smart Energy"),
    (33, "Real-Time Fire Detection and Suppression Trigger System", "Safety Automation"),
    (34, "Machine Vision Object Sorting Conveyor with Color Detection", "Industrial Automation"),
    (35, "Piezoelectric Energy Harvesting Floor Tile Monitor", "Energy Harvesting"),
    (36, "Structural Health Monitoring System for Bridges SHM", "Civil Engineering IoT"),
    (37, "Smart Helmet with Accident Detection and Auto SOS", "Safety IoT"),
    (38, "Automated Plant Disease Detection via Leaf Image Analysis", "AI Agriculture"),
    (39, "Self-Charging Autonomous Lawn Mower", "Consumer Robotics"),
    (40, "Electronic Nose for Food Quality Detection using Gas Sensor Array", "Food Technology IoT"),
    (41, "Self-Balancing Robot using Complementary Filter and PID", "Robotics"),
    (42, "Multi-Zone HVAC Controller with Machine Learning Scheduling", "Building Automation"),
    (43, "Smart Traffic Light Control with Vehicle Density Estimation", "Smart City"),
    (44, "CNC Pen Plotter with G-Code Interpreter", "Industrial Automation"),
    (45, "Automatic Solar Panel Cleaning Robot", "Renewable Energy"),
    (46, "Voice-Controlled Home Automation using CNN Keyword Detection", "AI Home Automation"),
    (47, "Water Leakage Detection System in Building Pipelines", "Smart Building"),
    (48, "Anti-Drone Frequency Jammer Detection System", "Industrial Safety"),
    (49, "Magnetic Levitation Display System with PID Height Control", "Advanced Electronics"),
    (50, "Multi-Axis CNC Milling Machine Controller", "Industrial Automation"),
    (51, "Smart Waste Segregation System with Image Classification", "Smart City"),
    (52, "Load Cell Vending Machine Controller", "Industrial Automation"),
    (53, "Electroencephalogram EEG Signal Acquisition System", "Healthcare Biomedical"),
    (54, "Smart Aquarium with Auto Feeding, Lighting, and Water Management", "IoT Aquaculture"),
    (55, "Autonomous Greenhouse Rover with Computer Vision for Weed Detection", "Agri-Robotics"),
    (56, "Vehicle-to-Grid Communication Protocol Simulator V2G", "EV Smart Grid"),
    (57, "Biometric ATM Security System with Fingerprint and Face Liveness", "Security Systems"),
    (58, "Smart Fermentation Monitor for Craft Beer Brewing", "Industrial IoT"),
    (59, "Wearable EMG-Based Prosthetic Hand Controller", "Assistive Technology"),
    (60, "High-Altitude Weather Balloon Telemetry System", "Aerospace IoT"),
    (61, "Smart Irrigation Valve with NB-IoT Connectivity", "Smart Agriculture"),
    (62, "Haptic Navigation System for Blind Individuals", "Assistive Technology"),
    (63, "Automated PCB Etching Tank Temperature and Agitation Controller", "Industrial Automation"),
    (64, "Smart Grid Demand Response Load Controller", "Smart Energy"),
    (65, "Forest Fire Early Warning Network using Mesh Topology", "Environmental Monitoring"),
    (66, "Smart Stethoscope with Digital Auscultation and AI Classification", "Healthcare Biomedical"),
    (67, "Underground Mining Safety Monitoring System", "Industrial Safety"),
    (68, "Real-Time Drowning Detection System for Swimming Pools", "Safety IoT"),
    (69, "Swarm Robotics System with Inter-Robot Communication", "Robotics"),
    (70, "Automated Water ATM with Smart Card and Flow Control", "Smart City IoT"),
    (71, "Neural Prosthetic Signal Processing Unit", "Healthcare Biomedical"),
    (72, "Smart Cold Chain Monitoring System for Pharmaceutical Logistics", "Healthcare IoT"),
    (73, "Automated Dam Gate Control with Flood Prediction Algorithm", "Water Management IoT"),
    (74, "Solar-Powered Autonomous Air Pollution Sampling Drone", "Aerospace Environmental"),
    (75, "Smart Cement Curing Monitor for Construction Sites", "Civil Engineering IoT"),
    (76, "Industrial Boiler Pressure and Temperature Safety Controller", "Industrial Safety"),
    (77, "Laser Security Fence with Intrusion Zone Identification", "Security Systems"),
    (78, "Automated Livestock Feeding and Health Monitoring System", "Smart Agriculture"),
    (79, "Active Noise Cancellation System using DSP on Arduino", "Audio Engineering"),
    (80, "Smart Anaesthesia Machine Monitoring and Alert System", "Healthcare Biomedical"),
    (81, "Automated Bee Colony Health Monitor with Acoustic Analysis", "Smart Agriculture"),
    (82, "High-Power LED Grow Light Controller with Spectrum Tuning", "Smart Agriculture"),
    (83, "Blockchain-Integrated Supply Chain Tracker for Pharmaceuticals", "Blockchain IoT"),
    # NEW: 84–100 placeholders
    (84, "IoT-Based Smart Classroom Attendance and Environment Monitor", "IoT Systems"),
    (85, "Drone-Based Crop Spraying Controller with GPS Waypoints", "Agri Drones"),
    (86, "Smart Railway Level Crossing Automation and Monitoring", "Smart City Safety"),
    (87, "Wearable Posture Correction System using IMU", "Assistive Technology"),
    (88, "Industrial Conveyor Belt Speed and Jam Detection System", "Industrial Automation"),
    (89, "Smart Street Parking Guidance with Mobile App Backend Stub", "Smart City"),
    (90, "IoT-Based Flood Level Monitoring and Siren System", "Environmental Safety"),
    (91, "Smart Home Energy Disaggregation Prototype", "Smart Energy"),
    (92, "Warehouse Inventory Tracker with RFID and Handheld Scanner", "Industrial IoT"),
    (93, "Smart City Noise Pollution Heatmap Node", "Environmental Monitoring"),
    (94, "TinyML Keyword Spotting Wake-Word Node", "AI Arduino TinyML"),
    (95, "Smart Wheelchair Controller with Obstacle Avoidance", "Assistive Technology"),
    (96, "Heliostat Solar Concentrator Mirror Positioning System", "Renewable Energy"),
    (97, "Multi-Room BLE Beacon Indoor Positioning Demo", "IoT Systems"),
    (98, "Smart Door Lock with OTP over WiFi Prototype", "Security Systems"),
    (99, "Helmet-Mounted Head-Up Display HUD Prototype", "Aerospace / Automotive"),
    (100, "Smart Campus Central Monitoring Dashboard Node", "Smart City IoT"),
]

BASE_DIR = Path("Projects")
FOLDERS = ["ArduinoCode", "CircuitDiagram", "Documentation", "Components", "Images", "Datasheets"]

def safe_folder_name(num: int, name: str) -> str:
    base = f"{num:03d}_" + name.replace(" ", "_")
    # Remove characters unsafe for most filesystems
    return "".join(c for c in base if c.isalnum() or c in "._-")

def create_project_structure():
    BASE_DIR.mkdir(exist_ok=True)
    for proj_num, proj_name, category in projects:
        folder_name = safe_folder_name(proj_num, proj_name)
        project_path = BASE_DIR / folder_name
        project_path.mkdir(parents=True, exist_ok=True)

        # Subdirectories + .gitkeep
        for folder in FOLDERS:
            sub = project_path / folder
            sub.mkdir(exist_ok=True)
            (sub / ".gitkeep").touch()

        # README.md
        readme_content = f"""# Project {proj_num:03d}: {proj_name}

## Category
{category}

## Difficulty
Advanced

## Components
- Arduino Board
- Sensors/Modules
- Connectivity Components

## Documentation
Project structure:
- ArduinoCode/: Complete Arduino sketches
- CircuitDiagram/: Hardware schematics
- Documentation/: Technical reports
- Components/: Bill of Materials
- Images/: Prototype photos
- Datasheets/: Component specifications
"""
        (project_path / "README.md").write_text(readme_content, encoding="utf-8")

        # BOM.json template
        bom_data = {
            "project": proj_name,
            "category": category,
            "projectId": proj_num,
            "components": [
                {"name": "Arduino Board", "quantity": 1, "cost": 500}
            ]
        }
        (project_path / "Components" / "BOM.json").write_text(
            json.dumps(bom_data, indent=2),
            encoding="utf-8"
        )

        print(f"Created: {folder_name}")

if __name__ == "__main__":
    print("Creating 100 Arduino project directories...")
    create_project_structure()
    print("Done! All 100 projects created.")
