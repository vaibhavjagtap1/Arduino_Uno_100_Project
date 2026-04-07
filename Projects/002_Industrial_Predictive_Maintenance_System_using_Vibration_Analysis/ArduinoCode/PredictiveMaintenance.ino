/*
 * ============================================================
 *  Project 002: Industrial Predictive Maintenance System
 *              using Vibration Analysis
 * ============================================================
 *  Author      : vaibhavjagtap1
 *  Board       : Arduino Uno R3
 *  Version     : 1.0.0
 *  Date        : 2025
 * ============================================================
 *
 *  DESCRIPTION:
 *    This system monitors machine vibration using an ADXL345
 *    3-axis accelerometer, computes RMS vibration magnitude
 *    over a rolling 100-sample window at 200 Hz, and classifies
 *    machine health as NORMAL / WARNING / CRITICAL.
 *    A DS18B20 temperature sensor tracks bearing temperature.
 *    All data is streamed as JSON over Serial for Python
 *    dashboard visualisation. A relay triggers emergency
 *    machine shutdown on CRITICAL vibration.
 *
 *  WIRING SUMMARY (see CircuitDiagram/circuit.svg):
 *    ADXL345 (I2C)         → SDA=A4, SCL=A5
 *    DS18B20 (1-Wire)      → D3  (+ 4.7kΩ pull-up to 5V)
 *    Relay Module          → D7
 *    Status LED (Green)    → D9  (NORMAL)
 *    Status LED (Yellow)   → D10 (WARNING)
 *    Status LED (Red)      → D11 (CRITICAL)
 *    Buzzer                → D12
 *    LCD 16×2 (I2C 0x27)  → SDA=A4, SCL=A5
 *
 *  LIBRARIES REQUIRED (install via Library Manager):
 *    - Adafruit ADXL345     (Adafruit Industries)
 *    - LiquidCrystal_I2C    (Frank de Brabander)
 *    - OneWire              (Paul Stoffregen)
 *    - DallasTemperature    (Miles Burton)
 *    - Wire                 (built-in)
 * ============================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ── Pin Definitions ──────────────────────────────────────────
#define LED_GREEN         9    // Normal state indicator
#define LED_YELLOW        10   // Warning state indicator
#define LED_RED           11   // Critical state indicator
#define BUZZER_PIN        12   // Alert buzzer
#define RELAY_PIN         7    // Machine shutdown relay (active LOW)
#define ONE_WIRE_BUS      3    // DS18B20 data pin

// ── Vibration RMS Thresholds (m/s²) ─────────────────────────
// Adjust these for your specific machine baseline:
//   NORMAL   : RMS < WARNING_THRESHOLD  → healthy operation
//   WARNING  : WARNING_THRESHOLD ≤ RMS < CRITICAL_THRESHOLD
//   CRITICAL : RMS ≥ CRITICAL_THRESHOLD → trigger shutdown
#define WARNING_THRESHOLD   3.0f   // m/s² RMS
#define CRITICAL_THRESHOLD  7.0f   // m/s²  RMS

// ── Sampling Configuration ───────────────────────────────────
#define SAMPLE_COUNT       100     // Samples per RMS window
#define SAMPLE_INTERVAL_US 5000    // 5000 µs = 200 Hz sample rate
#define SERIAL_BAUD        9600

// ── Objects ──────────────────────────────────────────────────
Adafruit_ADXL345_Unified accel  = Adafruit_ADXL345_Unified(12345);
LiquidCrystal_I2C        lcd    = LiquidCrystal_I2C(0x27, 16, 2);
OneWire                  oneWire(ONE_WIRE_BUS);
DallasTemperature        tempSensor(&oneWire);

// ── State Enumeration ────────────────────────────────────────
enum MachineState { NORMAL, WARNING, CRITICAL };

// ── Global Variables ─────────────────────────────────────────
float         rmsValue      = 0.0f;
float         bearingTemp   = 25.0f;
int16_t       ax_raw        = 0, ay_raw = 0, az_raw = 0;
MachineState  machineState  = NORMAL;
MachineState  lastLcdState  = NORMAL;   // track state changes for LCD refresh
unsigned long lastLcdUpdate = 0;
unsigned long lastTempRead  = 0;
// Non-blocking buzzer state for CRITICAL alarm pattern
unsigned long buzzerStepTime = 0;
uint8_t       buzzerStep     = 0;

// ── Function Prototypes ──────────────────────────────────────
float        collectRMS(int16_t &ax_out, int16_t &ay_out, int16_t &az_out);
float        readBearingTemp();
MachineState classifyState(float rms);
void         setIndicators(MachineState state);
void         triggerBuzzer(MachineState state);
void         updateLCD(float rms, float temp, MachineState state);
void         printSerialJSON(int16_t ax, int16_t ay, int16_t az,
                             float rms, float temp, MachineState state);
const char*  stateString(MachineState s);

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);

  // Pin modes
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN,  OUTPUT);

  // Safe initial state — relay off (HIGH = deactivated for active-LOW module)
  digitalWrite(RELAY_PIN,  HIGH);
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED,    LOW);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Predictive Maint");
  lcd.setCursor(0, 1);
  lcd.print("  Initializing..");
  delay(2000);
  lcd.clear();

  // ADXL345
  if (!accel.begin()) {
    Serial.println(F("# ERROR: ADXL345 not found. Check wiring!"));
    lcd.setCursor(0, 0);
    lcd.print("ADXL345 ERROR!  ");
    while (true) { tone(BUZZER_PIN, 400, 200); delay(500); }
  }
  // Set measurement range: ±16g for industrial vibration
  accel.setRange(ADXL345_RANGE_16_G);

  // DS18B20
  tempSensor.begin();

  // Startup tones
  tone(BUZZER_PIN, 1200, 150); delay(200);
  tone(BUZZER_PIN, 1600, 150); delay(200);
  tone(BUZZER_PIN, 2000, 150); delay(200);

  // Green LED on — system ready
  digitalWrite(LED_GREEN, HIGH);

  Serial.println(F("# Predictive Maintenance System v1.0 — Ready"));
  Serial.println(F("# JSON format: {\"ax\":x,\"ay\":y,\"az\":z,\"rms\":r,\"state\":\"...\",\"temp\":t}"));
}

// ════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  // ── 1. Collect 100 samples at 200 Hz and compute RMS ─────
  rmsValue = collectRMS(ax_raw, ay_raw, az_raw);

  // ── 2. Read bearing temperature every 2 seconds ──────────
  if (now - lastTempRead >= 2000UL) {
    lastTempRead = now;
    bearingTemp  = readBearingTemp();
  }

  // ── 3. Classify machine health ───────────────────────────
  machineState = classifyState(rmsValue);

  // ── 4. Drive LEDs, buzzer, and relay ─────────────────────
  setIndicators(machineState);
  triggerBuzzer(machineState);

  // ── 5. Update LCD every 500 ms to avoid flicker ──────────
  if (now - lastLcdUpdate >= 500UL) {
    lastLcdUpdate = now;
    updateLCD(rmsValue, bearingTemp, machineState);
  }

  // ── 6. Send JSON to Serial ────────────────────────────────
  printSerialJSON(ax_raw, ay_raw, az_raw, rmsValue, bearingTemp, machineState);
}

// ════════════════════════════════════════════════════════════
//  RMS COLLECTION (200 Hz × 100 samples = 500 ms window)
// ════════════════════════════════════════════════════════════

/**
 * collectRMS()
 * Gathers SAMPLE_COUNT acceleration readings at SAMPLE_INTERVAL_US
 * spacing, then computes the combined 3-axis RMS magnitude.
 *
 * RMS = sqrt( mean( ax²+ay²+az² ) )
 *
 * Uses integer accumulation to avoid float per-sample overhead:
 *   sum_sq += ax*ax + ay*ay + az*az  (scaled by 1000 to keep integer range)
 */
float collectRMS(int16_t &ax_out, int16_t &ay_out, int16_t &az_out) {
  sensors_event_t event;
  long long sum_sq = 0;   // accumulator for squared magnitudes (integer scaled)

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    unsigned long t0 = micros();

    accel.getEvent(&event);
    // Convert m/s² to integer millis (×1000) for integer math
    int32_t ax_i = (int32_t)(event.acceleration.x * 1000.0f);
    int32_t ay_i = (int32_t)(event.acceleration.y * 1000.0f);
    int32_t az_i = (int32_t)(event.acceleration.z * 1000.0f);

    sum_sq += (long long)ax_i * ax_i
            + (long long)ay_i * ay_i
            + (long long)az_i * az_i;

    // Keep last sample for JSON reporting (raw axis in m/s² × 100)
    if (i == SAMPLE_COUNT - 1) {
      ax_out = (int16_t)(event.acceleration.x * 100.0f);
      ay_out = (int16_t)(event.acceleration.y * 100.0f);
      az_out = (int16_t)(event.acceleration.z * 100.0f);
    }

    // Busy-wait to maintain 200 Hz cadence
    while ((micros() - t0) < (unsigned long)SAMPLE_INTERVAL_US);
  }

  // mean_sq in (m/s² × 1000)² units → convert back to (m/s²)²
  // mean_sq_mss = (sum_sq / SAMPLE_COUNT) / 1e6
  float mean_sq_mss = (float)(sum_sq / SAMPLE_COUNT) / 1000000.0f;
  return sqrt(mean_sq_mss);
}

// ════════════════════════════════════════════════════════════
//  TEMPERATURE
// ════════════════════════════════════════════════════════════

/**
 * readBearingTemp()
 * Reads DS18B20 temperature in °C. Returns last valid value on error.
 */
float readBearingTemp() {
  tempSensor.requestTemperatures();
  float t = tempSensor.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C) {
    Serial.println(F("# WARN: DS18B20 disconnected"));
    return bearingTemp;  // retain last reading
  }
  return t;
}

// ════════════════════════════════════════════════════════════
//  FAULT DETECTION
// ════════════════════════════════════════════════════════════

/**
 * classifyState()
 * Maps RMS vibration to one of three health states.
 *
 * Also considers bearing temperature for WARNING escalation:
 *   bearingTemp > 80°C upgrades state to at least WARNING.
 */
MachineState classifyState(float rms) {
  MachineState s;

  if (rms >= CRITICAL_THRESHOLD) {
    s = CRITICAL;
  } else if (rms >= WARNING_THRESHOLD) {
    s = WARNING;
  } else {
    s = NORMAL;
  }

  // Temperature-based escalation
  if (bearingTemp > 80.0f && s == NORMAL) {
    s = WARNING;
  }
  if (bearingTemp > 100.0f) {
    s = CRITICAL;
  }

  return s;
}

// ════════════════════════════════════════════════════════════
//  INDICATORS
// ════════════════════════════════════════════════════════════

void setIndicators(MachineState state) {
  // Exclusive LED logic
  digitalWrite(LED_GREEN,  state == NORMAL   ? HIGH : LOW);
  digitalWrite(LED_YELLOW, state == WARNING  ? HIGH : LOW);
  digitalWrite(LED_RED,    state == CRITICAL ? HIGH : LOW);

  // Relay: engage shutdown on CRITICAL (active LOW)
  if (state == CRITICAL) {
    digitalWrite(RELAY_PIN, LOW);   // Activate relay → shutdown machine
  } else {
    digitalWrite(RELAY_PIN, HIGH);  // Deactivate relay → normal run
  }
}

/**
 * triggerBuzzer()
 * Non-blocking buzzer driver using millis() time-stepping:
 *   NORMAL   → silent (no tone)
 *   WARNING  → single 1000 Hz, 100 ms beep
 *   CRITICAL → rapid 500/1500 Hz alternating pattern (4 × 80 ms bursts)
 *              Advances one step per call via buzzerStep/buzzerStepTime.
 */
void triggerBuzzer(MachineState state) {
  unsigned long now = millis();
  if (state == NORMAL) {
    noTone(BUZZER_PIN);
    buzzerStep = 0;
    return;
  }
  if (state == WARNING) {
    tone(BUZZER_PIN, 1000, 100);
    buzzerStep = 0;
    return;
  }
  // CRITICAL: 4-step alternating alarm, each step 100 ms apart
  if (now - buzzerStepTime >= 100UL) {
    buzzerStepTime = now;
    uint16_t freqs[4] = {500, 1500, 500, 1500};
    tone(BUZZER_PIN, freqs[buzzerStep % 4], 80);
    buzzerStep = (buzzerStep + 1) % 4;
  }
}

// ════════════════════════════════════════════════════════════
//  DISPLAY
// ════════════════════════════════════════════════════════════

void updateLCD(float rms, float temp, MachineState state) {
  // Full clear only on state transitions to avoid artefacts
  if (state != lastLcdState) {
    lcd.clear();
    lastLcdState = state;
  }

  // Row 0: RMS value and bearing temp (overwrite in place)
  lcd.setCursor(0, 0);
  lcd.print("RMS:");
  lcd.print(rms, 2);
  lcd.print(" T:");
  // Pad temp to 3 chars to clear any previous longer value
  if ((int)temp < 10)  lcd.print(' ');
  if ((int)temp < 100) lcd.print(' ');
  lcd.print((int)temp);
  lcd.print((char)223);   // degree symbol
  lcd.print("C");

  // Row 1: Machine state (pad to 10 chars to erase previous text)
  lcd.setCursor(0, 1);
  lcd.print("State:");
  const char* s = stateString(state);
  lcd.print(s);
  // Pad remaining columns with spaces
  uint8_t slen = (uint8_t)strlen(s);
  for (uint8_t i = slen; i < 10; i++) lcd.print(' ');
}

void printSerialJSON(int16_t ax, int16_t ay, int16_t az,
                     float rms, float temp, MachineState state) {
  // ax/ay/az stored as integer (actual_value × 100); divide back for JSON
  Serial.print(F("{\"ax\":"));
  Serial.print(ax / 100.0f, 2);
  Serial.print(F(",\"ay\":"));
  Serial.print(ay / 100.0f, 2);
  Serial.print(F(",\"az\":"));
  Serial.print(az / 100.0f, 2);
  Serial.print(F(",\"rms\":"));
  Serial.print(rms, 3);
  Serial.print(F(",\"state\":\""));
  Serial.print(stateString(state));
  Serial.print(F("\",\"temp\":"));
  Serial.print(temp, 1);
  Serial.println(F("}"));
}

// ════════════════════════════════════════════════════════════
//  UTILITY
// ════════════════════════════════════════════════════════════

const char* stateString(MachineState s) {
  switch (s) {
    case NORMAL:   return "NORMAL";
    case WARNING:  return "WARNING";
    case CRITICAL: return "CRITICAL";
    default:       return "UNKNOWN";
  }
}
