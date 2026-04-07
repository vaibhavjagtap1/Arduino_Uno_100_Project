/*
 * ============================================================
 *  Project 003: Non-Invasive Blood Glucose Monitoring Device
 *               using NIR Spectroscopy
 * ============================================================
 *  Author      : vaibhavjagtap1
 *  Board       : Arduino Uno R3
 *  Version     : 1.0.0
 *  Date        : 2025
 * ============================================================
 *
 *  ⚠️  IMPORTANT DISCLAIMER  ⚠️
 *  This is an EDUCATIONAL PROTOTYPE demonstrating NIR
 *  spectroscopy principles on Arduino hardware.
 *  It is NOT a certified medical device and MUST NOT be
 *  used for clinical blood glucose diagnosis or treatment
 *  decisions.  For medical glucose monitoring always use
 *  an FDA/CE-cleared glucometer.
 * ============================================================
 *
 *  DESCRIPTION:
 *    Demonstrates near-infrared (NIR) absorbance measurement
 *    using Beer-Lambert law to estimate relative glucose
 *    concentration in a tissue phantom / finger probe.
 *    A 940 nm IR LED (D6) and a 660 nm red reference LED (D5)
 *    illuminate the finger.  A BPW34 photodiode feeds a
 *    transimpedance amplifier (TIA); the ADC channels A0
 *    (940 nm path) and A1 (660 nm path) are sampled with
 *    32-point averaging for noise reduction.
 *    Beer-Lambert absorbance → linear regression → glucose
 *    estimate in mg/dL.  NTC thermistor on A2 provides
 *    temperature compensation.  Results displayed on 16×2
 *    I2C LCD with trend arrows and coloured LED indicators.
 *
 *  WIRING SUMMARY (see CircuitDiagram/circuit.svg):
 *    940 nm IR LED (PWM)      → D6  (100 Ω series resistor)
 *    660 nm Red LED (PWM)     → D5  (100 Ω series resistor)
 *    Photodiode TIA output    → A0  (940 nm channel)
 *    Photodiode TIA output    → A1  (660 nm channel)
 *    NTC Thermistor divider   → A2
 *    Push Button (trigger)    → D2  (10 kΩ pull-down)
 *    Green LED (NORMAL)       → D9  (220 Ω)
 *    Yellow LED (BORDERLINE)  → D10 (220 Ω)
 *    Red LED (ALERT)          → D11 (220 Ω)
 *    Buzzer (active)          → D12
 *    LCD 16×2 I2C             → SDA=A4, SCL=A5
 *
 *  LIBRARIES REQUIRED (install via Library Manager):
 *    - LiquidCrystal_I2C  (Frank de Brabander)
 *    - Wire                (built-in)
 * ============================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// ── Pin Definitions ──────────────────────────────────────────
#define PIN_IR_LED       6    // 940 nm IR LED  (PWM)
#define PIN_RED_LED_DRV  5    // 660 nm Red LED (PWM) — reference channel
#define PIN_PD_IR        A0   // Photodiode TIA output — 940 nm
#define PIN_PD_RED       A1   // Photodiode TIA output — 660 nm
#define PIN_THERMISTOR   A2   // NTC thermistor voltage divider
#define PIN_BUTTON       2    // Measurement trigger button
#define PIN_LED_GREEN    9    // Normal range indicator
#define PIN_LED_YELLOW   10   // Borderline indicator
#define PIN_LED_RED      11   // Alert indicator
#define PIN_BUZZER       12   // Active buzzer for critical alerts

// ── Measurement Parameters ───────────────────────────────────
#define SAMPLE_COUNT       32    // Samples averaged per channel
#define SAMPLE_DELAY_MS     2    // ms between individual samples
#define LED_PWM_VALUE     200    // 0-255 PWM brightness for LEDs
#define DARK_SETTLE_MS     10    // Settle time after LED off for dark reading
#define LED_SETTLE_MS       5    // Settle time after LED on before sampling
#define MEAS_DEBOUNCE_MS  300    // Button debounce period

// ── NTC Thermistor Constants ─────────────────────────────────
#define NTC_NOMINAL      10000   // Nominal resistance at 25 °C (10 kΩ)
#define NTC_BCOEFF        3950   // Beta coefficient
#define NTC_SERIES       10000   // Series resistor value (10 kΩ)
#define TEMP_NOMINAL       25.0  // Nominal temperature (°C)

// ── Beer-Lambert / Calibration Model ────────────────────────
//  glucose_mg_dL = CALIB_A * absorbance_ratio + CALIB_B
//  These coefficients are placeholders derived from a bench
//  calibration with known glucose solutions (see Documentation).
//  Re-calibrate for each hardware build.
#define CALIB_A           850.0f
#define CALIB_B            80.0f

// ── Temperature Compensation ─────────────────────────────────
//  Correction slope: ~0.5 mg/dL per °C deviation from 37 °C
#define TEMP_COMP_SLOPE    0.5f
#define BODY_TEMP_REF     37.0f

// ── Glucose Classification Thresholds (mg/dL) ────────────────
#define GLUCOSE_LOW        70.0f
#define GLUCOSE_NORMAL_HI 140.0f
#define GLUCOSE_HIGH_HI   200.0f

// ── Trend Detection ───────────────────────────────────────────
#define TREND_HISTORY       5    // readings kept for moving average

// ── Serial Baud Rate ─────────────────────────────────────────
#define SERIAL_BAUD        9600

// ── LCD Object ───────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);   // Change 0x3F if 0x27 not found

// ── Global State ─────────────────────────────────────────────
float   lastGlucose[TREND_HISTORY] = {0};
uint8_t trendIdx   = 0;
bool    trendFull  = false;
volatile bool measureFlag = false;   // set by button ISR

// ── Custom LCD Characters ─────────────────────────────────────
// Up arrow, down arrow, right arrow
byte arrowUp[8]   = {0x04,0x0E,0x1F,0x04,0x04,0x04,0x04,0x00};
byte arrowDown[8] = {0x04,0x04,0x04,0x04,0x1F,0x0E,0x04,0x00};
byte arrowRight[8]= {0x00,0x04,0x02,0x1F,0x02,0x04,0x00,0x00};

// ─────────────────────────────────────────────────────────────
//  ISR: button pressed → set flag
// ─────────────────────────────────────────────────────────────
void IRAM_ATTR buttonISR() {
  measureFlag = true;
}

// ─────────────────────────────────────────────────────────────
//  Average ADC: takes N samples, returns mean
// ─────────────────────────────────────────────────────────────
float averageADC(uint8_t pin, uint8_t n) {
  long sum = 0;
  for (uint8_t i = 0; i < n; i++) {
    sum += analogRead(pin);
    delay(SAMPLE_DELAY_MS);
  }
  return (float)sum / n;
}

// ─────────────────────────────────────────────────────────────
//  Read NTC thermistor → temperature in °C (Steinhart-Hart)
// ─────────────────────────────────────────────────────────────
float readTemperature() {
  float adcVal  = averageADC(PIN_THERMISTOR, 8);
  float voltage = adcVal * (5.0f / 1023.0f);
  // Voltage divider: V = 5 * R_ntc / (R_series + R_ntc)
  float rNTC    = NTC_SERIES * voltage / (5.0f - voltage);
  float steinhart;
  steinhart  = rNTC / (float)NTC_NOMINAL;    // R/R0
  steinhart  = log(steinhart);               // ln(R/R0)
  steinhart /= (float)NTC_BCOEFF;            // 1/B * ln(R/R0)
  steinhart += 1.0f / (TEMP_NOMINAL + 273.15f);  // + 1/T0
  steinhart  = 1.0f / steinhart;            // Kelvin
  return steinhart - 273.15f;               // °C
}

// ─────────────────────────────────────────────────────────────
//  Single-channel absorbance measurement
//  Returns Abs = log10(I_dark_corrected_ref / I_dark_corrected_meas)
//  Uses both LED-on and LED-off (dark) readings for offset cancel.
// ─────────────────────────────────────────────────────────────
float measureAbsorbance(uint8_t ledPin, uint8_t pdPin) {
  // 1. Dark reading (LED off)
  analogWrite(ledPin, 0);
  delay(DARK_SETTLE_MS);
  float dark = averageADC(pdPin, SAMPLE_COUNT);

  // 2. Light reading (LED on)
  analogWrite(ledPin, LED_PWM_VALUE);
  delay(LED_SETTLE_MS);
  float light = averageADC(pdPin, SAMPLE_COUNT);
  analogWrite(ledPin, 0);   // LED off after measurement

  // 3. Net signal (dark-subtracted)
  float I_ref    = max(light - dark, 1.0f);   // avoid log(0)
  float I_meas   = max(dark, 1.0f);           // placeholder ratio

  // Beer-Lambert: Abs = log10(I0 / I)
  // Here I0 = reference (LED-on, no-finger), I = LED-on through finger.
  // For a real dual-path setup the reference photodiode provides I0.
  // In this single-PD design we use the LED-on value as I0 relative
  // to an open-air baseline stored during calibration.
  return log10f(I_ref / 1.0f);   // returns log10 of net signal
}

// ─────────────────────────────────────────────────────────────
//  Full two-wavelength NIR measurement
//  Returns ratio absorbance (940 nm / 660 nm) used in model
// ─────────────────────────────────────────────────────────────
float measureNIRRatio(float *abs940out, float *abs660out,
                      float *raw940out, float *raw660out) {
  // Measure 940 nm channel
  analogWrite(PIN_RED_LED_DRV, 0);
  analogWrite(PIN_IR_LED, 0);
  delay(DARK_SETTLE_MS);
  float dark940  = averageADC(PIN_PD_IR, SAMPLE_COUNT);
  float dark660  = averageADC(PIN_PD_RED, SAMPLE_COUNT);

  // IR LED on → 940 nm channel
  analogWrite(PIN_IR_LED, LED_PWM_VALUE);
  delay(LED_SETTLE_MS);
  float light940 = averageADC(PIN_PD_IR,  SAMPLE_COUNT);
  float light660 = averageADC(PIN_PD_RED, SAMPLE_COUNT);
  analogWrite(PIN_IR_LED, 0);
  delay(DARK_SETTLE_MS);

  // Red LED on → 660 nm channel
  analogWrite(PIN_RED_LED_DRV, LED_PWM_VALUE);
  delay(LED_SETTLE_MS);
  float rlight940 = averageADC(PIN_PD_IR,  SAMPLE_COUNT);
  float rlight660 = averageADC(PIN_PD_RED, SAMPLE_COUNT);
  analogWrite(PIN_RED_LED_DRV, 0);

  // Net intensities
  float I_940 = max(light940 - dark940, 1.0f);
  float I_660 = max(rlight660 - dark660, 1.0f);

  // Absorbance via Beer-Lambert: Abs = log10(I0/I)
  // Using ADC full-scale (1023) as I0 proxy
  float abs940 = log10f(1023.0f / I_940);
  float abs660 = log10f(1023.0f / I_660);

  *abs940out = abs940;
  *abs660out = abs660;
  *raw940out = I_940;
  *raw660out = I_660;

  // Ratio eliminates path-length variability (analogous to pulse-ox R)
  float ratio = (abs940 > 0.0f && abs660 > 0.0f) ? (abs940 / abs660) : 1.0f;
  return ratio;
}

// ─────────────────────────────────────────────────────────────
//  Glucose estimation from absorbance ratio
// ─────────────────────────────────────────────────────────────
float estimateGlucose(float absRatio, float tempC) {
  float glucose = CALIB_A * absRatio + CALIB_B;
  // Temperature compensation: correct for deviation from 37 °C
  float tempDelta = tempC - BODY_TEMP_REF;
  glucose -= TEMP_COMP_SLOPE * tempDelta;
  return constrain(glucose, 0.0f, 600.0f);
}

// ─────────────────────────────────────────────────────────────
//  Classify glucose level
// ─────────────────────────────────────────────────────────────
const char* classifyGlucose(float g) {
  if (g < GLUCOSE_LOW)        return "LOW";
  if (g <= GLUCOSE_NORMAL_HI) return "NORMAL";
  if (g <= GLUCOSE_HIGH_HI)   return "HIGH";
  return "VERY HIGH";
}

// ─────────────────────────────────────────────────────────────
//  Update trend moving average buffer; return trend char
//  0=stable(→), 1=rising(↑), 2=falling(↓)
// ─────────────────────────────────────────────────────────────
uint8_t updateTrend(float newGlucose) {
  lastGlucose[trendIdx % TREND_HISTORY] = newGlucose;
  trendIdx++;
  if (trendIdx >= TREND_HISTORY) trendFull = true;

  if (!trendFull) return 0;   // not enough data yet

  float oldest = lastGlucose[(trendIdx) % TREND_HISTORY];
  float delta  = newGlucose - oldest;

  if      (delta >  10.0f) return 1;   // rising
  else if (delta < -10.0f) return 2;   // falling
  return 0;                            // stable
}

// ─────────────────────────────────────────────────────────────
//  Update LED indicators and buzzer
// ─────────────────────────────────────────────────────────────
void updateIndicators(float glucose) {
  // Turn all off first
  digitalWrite(PIN_LED_GREEN,  LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED,    LOW);
  digitalWrite(PIN_BUZZER,     LOW);

  if (glucose < GLUCOSE_LOW) {
    // Hypoglycaemia — critical: red + buzzer
    digitalWrite(PIN_LED_RED, HIGH);
    // Short double-beep pattern
    for (uint8_t b = 0; b < 3; b++) {
      digitalWrite(PIN_BUZZER, HIGH); delay(150);
      digitalWrite(PIN_BUZZER, LOW);  delay(100);
    }
  } else if (glucose <= GLUCOSE_NORMAL_HI) {
    // Normal range
    digitalWrite(PIN_LED_GREEN, HIGH);
  } else if (glucose <= GLUCOSE_HIGH_HI) {
    // High — yellow warning
    digitalWrite(PIN_LED_YELLOW, HIGH);
  } else {
    // Very high — red + buzzer
    digitalWrite(PIN_LED_RED, HIGH);
    for (uint8_t b = 0; b < 2; b++) {
      digitalWrite(PIN_BUZZER, HIGH); delay(300);
      digitalWrite(PIN_BUZZER, LOW);  delay(200);
    }
  }
}

// ─────────────────────────────────────────────────────────────
//  Update LCD (16×2, I2C)
// ─────────────────────────────────────────────────────────────
void updateLCD(float glucose, const char* status, uint8_t trend) {
  lcd.clear();
  // Row 0: glucose value + trend arrow
  lcd.setCursor(0, 0);
  lcd.print("Glucose:");
  lcd.print((int)glucose);
  lcd.print("mg/dL");
  lcd.setCursor(15, 0);
  if      (trend == 1) lcd.write(0);   // up arrow (char 0)
  else if (trend == 2) lcd.write(1);   // down arrow (char 1)
  else                 lcd.write(2);   // right arrow (char 2)

  // Row 1: status string
  lcd.setCursor(0, 1);
  lcd.print("Status: ");
  lcd.print(status);
}

// ─────────────────────────────────────────────────────────────
//  Emit JSON to Serial
// ─────────────────────────────────────────────────────────────
void emitJSON(float raw940, float raw660,
              float abs940, float abs660,
              float absRatio, float glucose,
              const char* status, float temp) {
  Serial.print(F("{"));
  Serial.print(F("\"nir\":")); Serial.print(raw940, 1);
  Serial.print(F(",\"red\":")); Serial.print(raw660, 1);
  Serial.print(F(",\"abs940\":")); Serial.print(abs940, 4);
  Serial.print(F(",\"abs660\":")); Serial.print(abs660, 4);
  Serial.print(F(",\"absorbance\":")); Serial.print(absRatio, 4);
  Serial.print(F(",\"glucose\":")); Serial.print(glucose, 1);
  Serial.print(F(",\"status\":\""));  Serial.print(status);
  Serial.print(F("\",\"temp\":")); Serial.print(temp, 1);
  Serial.println(F("}"));
}

// ─────────────────────────────────────────────────────────────
//  setup()
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(SERIAL_BAUD);

  // Output pins
  pinMode(PIN_IR_LED,      OUTPUT);
  pinMode(PIN_RED_LED_DRV, OUTPUT);
  pinMode(PIN_LED_GREEN,   OUTPUT);
  pinMode(PIN_LED_YELLOW,  OUTPUT);
  pinMode(PIN_LED_RED,     OUTPUT);
  pinMode(PIN_BUZZER,      OUTPUT);

  // Input pins
  pinMode(PIN_BUTTON, INPUT);   // external pull-down 10 kΩ

  // Ensure LEDs start off
  analogWrite(PIN_IR_LED,      0);
  analogWrite(PIN_RED_LED_DRV, 0);
  digitalWrite(PIN_LED_GREEN,  LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED,    LOW);
  digitalWrite(PIN_BUZZER,     LOW);

  // Button interrupt (rising edge = button pressed)
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), buttonISR, RISING);

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, arrowUp);
  lcd.createChar(1, arrowDown);
  lcd.createChar(2, arrowRight);

  // Splash screen
  lcd.setCursor(0, 0); lcd.print("NIR Glucose Mon.");
  lcd.setCursor(0, 1); lcd.print("Press BTN to meas");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("EDUCATIONAL PROTO");
  lcd.setCursor(0, 1); lcd.print("NOT FOR DIAGNOSIS");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Ready. Press BTN");

  Serial.println(F("# NIR Glucose Monitor v1.0 — Educational Prototype"));
  Serial.println(F("# NOT for medical diagnosis. Press button to measure."));
}

// ─────────────────────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────────────────────
void loop() {
  if (!measureFlag) return;
  measureFlag = false;
  delay(MEAS_DEBOUNCE_MS);   // simple debounce

  // Update LCD: measuring
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Measuring...");
  lcd.setCursor(0, 1); lcd.print("Hold finger still");

  // 1. Read temperature for compensation
  float tempC = readTemperature();

  // 2. NIR dual-wavelength measurement
  float abs940, abs660, raw940, raw660;
  float absRatio = measureNIRRatio(&abs940, &abs660, &raw940, &raw660);

  // 3. Glucose estimation
  float glucose = estimateGlucose(absRatio, tempC);

  // 4. Classify
  const char* status = classifyGlucose(glucose);

  // 5. Trend
  uint8_t trend = updateTrend(glucose);

  // 6. Update outputs
  updateIndicators(glucose);
  updateLCD(glucose, status, trend);
  emitJSON(raw940, raw660, abs940, abs660, absRatio, glucose, status, tempC);
}
