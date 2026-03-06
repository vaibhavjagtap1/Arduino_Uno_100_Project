/*
 * ============================================================
 *  Project 001: AI-Based Smart Irrigation System
 *              using Arduino and Soil Analytics
 * ============================================================
 *  Author      : vaibhavjagtap1
 *  Board       : Arduino Uno R3
 *  Version     : 1.0.0
 *  Date        : 2025
 * ============================================================
 *
 *  DESCRIPTION:
 *    This system monitors soil moisture, temperature, and
 *    humidity to make intelligent irrigation decisions.
 *    A fuzzy-logic rule engine (simulating AI inference)
 *    controls a relay-driven water pump and logs all sensor
 *    analytics over the Serial port for Python dashboard
 *    integration.
 *
 *  WIRING SUMMARY (see CircuitDiagram/circuit_diagram.svg):
 *    Soil Moisture Sensor  → A0
 *    DHT11 (Temp/Humidity) → D2
 *    LDR (Sunlight)        → A1
 *    Relay Module          → D7
 *    LCD 16×2 (I2C)        → SDA=A4, SCL=A5
 *    Status LED (Green)    → D9
 *    Status LED (Red)      → D10
 *    Buzzer                → D11
 *
 *  LIBRARIES REQUIRED (install via Library Manager):
 *    - DHT sensor library  (Adafruit)
 *    - LiquidCrystal_I2C   (Frank de Brabander)
 * ============================================================
 */

#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ── Pin Definitions ──────────────────────────────────────────
#define SOIL_MOISTURE_PIN  A0
#define LDR_PIN            A1
#define DHT_PIN            2
#define DHT_TYPE           DHT11
#define RELAY_PIN          7
#define LED_GREEN          9
#define LED_RED            10
#define BUZZER_PIN         11

// ── Calibration Constants ────────────────────────────────────
// Calibrate these for your specific soil moisture sensor:
//   DRY_VALUE  = ADC reading in completely dry soil
//   WET_VALUE  = ADC reading in completely saturated soil
#define SOIL_DRY_VALUE    850
#define SOIL_WET_VALUE    350

// ── Thresholds ───────────────────────────────────────────────
#define MOISTURE_LOW      30    // % — irrigate if below this
#define MOISTURE_HIGH     70    // % — stop irrigation above this
#define TEMP_HIGH         35.0  // °C — reduce irrigation if hot
#define HUMIDITY_LOW      40.0  // % — compensate for dry air
#define LDR_NIGHT         200   // ADC — skip irrigation at night

// ── Timing ───────────────────────────────────────────────────
#define SENSOR_INTERVAL   2000  // ms between readings
#define PUMP_ON_DURATION  5000  // ms pump stays ON per cycle
#define SERIAL_BAUD       9600

// ── Objects ──────────────────────────────────────────────────
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);   // Change 0x27 to 0x3F if needed

// ── Global State ─────────────────────────────────────────────
unsigned long lastSensorRead  = 0;
unsigned long pumpStartTime   = 0;
bool          pumpRunning     = false;
float         soilMoisturePct = 0.0;
float         temperature     = 0.0;
float         humidity        = 0.0;
int           lightLevel      = 0;
String        decisionReason  = "";

// ── Function Prototypes ──────────────────────────────────────
float  readSoilMoisture();
float  readTemperature();
float  readHumidity();
int    readLightLevel();
bool   shouldIrrigate(float moisture, float temp, float hum, int light);
void   startPump();
void   stopPump();
void   updateLCD(float moisture, float temp, float hum, bool pumping);
void   printSerialJSON(float moisture, float temp, float hum, int light, bool pumping);
void   blinkLED(int pin, int times, int delayMs);

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);

  // Pin modes
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Safe initial state — pump OFF
  digitalWrite(RELAY_PIN, HIGH);  // Active-LOW relay module
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED,   LOW);

  // DHT sensor
  dht.begin();

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Irrigation");
  lcd.setCursor(0, 1);
  lcd.print("  Initializing..");
  delay(2000);
  lcd.clear();

  // Startup beep
  tone(BUZZER_PIN, 1000, 200);
  delay(300);
  tone(BUZZER_PIN, 1500, 200);
  delay(300);

  Serial.println(F("# Smart Irrigation System v1.0 — Ready"));
  Serial.println(F("# JSON stream format:"));
  Serial.println(F("# {\"moisture\":xx,\"temp\":xx,\"humidity\":xx,"
                   "\"light\":xx,\"pump\":true/false,\"reason\":\"...\"}"));
}

// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  // ── 1. Read sensors at defined interval ──────────────────
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    soilMoisturePct = readSoilMoisture();
    temperature     = readTemperature();
    humidity        = readHumidity();
    lightLevel      = readLightLevel();

    // ── 2. AI Decision Engine (Fuzzy Logic Rules) ─────────
    bool irrigate = shouldIrrigate(soilMoisturePct, temperature,
                                   humidity, lightLevel);

    // ── 3. Actuate Pump ───────────────────────────────────
    if (irrigate && !pumpRunning) {
      startPump();
      pumpStartTime = now;
    }

    // ── 4. Update Outputs ─────────────────────────────────
    updateLCD(soilMoisturePct, temperature, humidity, pumpRunning);
    printSerialJSON(soilMoisturePct, temperature, humidity,
                    lightLevel, pumpRunning);
  }

  // ── 5. Auto-stop pump after ON duration ─────────────────
  if (pumpRunning && (now - pumpStartTime >= PUMP_ON_DURATION)) {
    stopPump();
  }
}

// ════════════════════════════════════════════════════════════
//  SENSOR FUNCTIONS
// ════════════════════════════════════════════════════════════

/**
 * readSoilMoisture()
 * Reads raw ADC and converts to 0–100% moisture percentage.
 * Higher ADC = drier soil (capacitive sensor logic).
 */
float readSoilMoisture() {
  int raw = analogRead(SOIL_MOISTURE_PIN);
  // Map: SOIL_DRY_VALUE → 0%, SOIL_WET_VALUE → 100%
  float pct = map(raw, SOIL_DRY_VALUE, SOIL_WET_VALUE, 0, 100);
  pct = constrain(pct, 0.0, 100.0);
  return pct;
}

/**
 * readTemperature() — DHT11 temperature in °C
 * Returns last valid value if NaN (sensor glitch protection).
 */
float readTemperature() {
  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println(F("# WARN: DHT temperature read failed"));
    return temperature;  // keep last value
  }
  return t;
}

/**
 * readHumidity() — DHT11 relative humidity in %
 */
float readHumidity() {
  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println(F("# WARN: DHT humidity read failed"));
    return humidity;
  }
  return h;
}

/**
 * readLightLevel() — LDR: 0 (dark) to 1023 (bright)
 */
int readLightLevel() {
  return analogRead(LDR_PIN);
}

// ════════════════════════════════════════════════════════════
//  AI DECISION ENGINE — Fuzzy Logic Rule Set
// ════════════════════════════════════════════════════════════

/**
 * shouldIrrigate()
 *
 * Rule Table:
 * ┌────────────────┬──────────┬──────────┬────────┬─────────┐
 * │ Moisture       │ Temp     │ Humidity │ Light  │ ACTION  │
 * ├────────────────┼──────────┼──────────┼────────┼─────────┤
 * │ < LOW          │ any      │ any      │ night  │ SKIP    │
 * │ < LOW          │ < HIGH   │ > LOW    │ day    │ IRRIGATE│
 * │ < LOW          │ >= HIGH  │ any      │ day    │ IRRIGATE│
 * │ < LOW          │ < HIGH   │ <= LOW   │ day    │ IRRIGATE│
 * │ LOW..HIGH      │ >= HIGH  │ <= LOW   │ day    │ IRRIGATE│
 * │ > HIGH         │ any      │ any      │ any    │ SKIP    │
 * └────────────────┴──────────┴──────────┴────────┴─────────┘
 */
bool shouldIrrigate(float moisture, float temp, float hum, int light) {
  bool isNight = (light < LDR_NIGHT);

  // Rule 1: Never irrigate at night
  if (isNight) {
    decisionReason = "Night — skipping";
    return false;
  }

  // Rule 2: Soil already saturated
  if (moisture >= MOISTURE_HIGH) {
    decisionReason = "Soil saturated";
    return false;
  }

  // Rule 3: Critical moisture — always irrigate
  if (moisture < MOISTURE_LOW) {
    decisionReason = "Critical dry — irrigate";
    return true;
  }

  // Rule 4: Moderate moisture + hot & dry conditions
  if (moisture < MOISTURE_HIGH && temp >= TEMP_HIGH && hum <= HUMIDITY_LOW) {
    decisionReason = "Hot+dry conditions";
    return true;
  }

  // Rule 5: Moderate — no action needed
  decisionReason = "Moisture adequate";
  return false;
}

// ════════════════════════════════════════════════════════════
//  PUMP CONTROL
// ════════════════════════════════════════════════════════════

void startPump() {
  pumpRunning = true;
  digitalWrite(RELAY_PIN, LOW);   // Activate relay (active LOW)
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED,   LOW);
  tone(BUZZER_PIN, 800, 100);
  Serial.println(F("# PUMP ON"));
}

void stopPump() {
  pumpRunning = false;
  digitalWrite(RELAY_PIN, HIGH);  // Deactivate relay
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED,   HIGH);
  delay(50);
  digitalWrite(LED_RED,   LOW);
}

// ════════════════════════════════════════════════════════════
//  DISPLAY FUNCTIONS
// ════════════════════════════════════════════════════════════

void updateLCD(float moisture, float temp, float hum, bool pumping) {
  lcd.clear();
  // Row 0: Moisture & Temp
  lcd.setCursor(0, 0);
  lcd.print("M:");
  lcd.print((int)moisture);
  lcd.print("% T:");
  lcd.print((int)temp);
  lcd.print((char)223);  // degree symbol
  lcd.print("C");

  // Row 1: Humidity & Pump status
  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.print((int)hum);
  lcd.print("% ");
  lcd.print(pumping ? "PUMP:ON " : "PUMP:OFF");
}

void printSerialJSON(float moisture, float temp, float hum,
                     int light, bool pumping) {
  Serial.print(F("{\"moisture\":"));
  Serial.print(moisture, 1);
  Serial.print(F(",\"temp\":"));
  Serial.print(temp, 1);
  Serial.print(F(",\"humidity\":"));
  Serial.print(hum, 1);
  Serial.print(F(",\"light\":"));
  Serial.print(light);
  Serial.print(F(",\"pump\":"));
  Serial.print(pumping ? "true" : "false");
  Serial.print(F(",\"reason\":\""));
  Serial.print(decisionReason);
  Serial.println(F("\"}"));
}

// Utility: blink an LED n times
void blinkLED(int pin, int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(delayMs);
    digitalWrite(pin, LOW);
    delay(delayMs);
  }
}
