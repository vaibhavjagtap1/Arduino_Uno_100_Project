/*
 * ============================================================
 *  Project 005 : Smart Energy Management System for
 *                Solar-Battery Microgrid
 * ============================================================
 *  Board   : Arduino Uno R3
 *  Author  : vaibhavjagtap1
 *  Version : 1.0.0
 *
 *  Description:
 *    Three INA219 current/power sensors monitor the solar panel,
 *    battery, and load circuits.  A Perturb-and-Observe (P&O)
 *    MPPT algorithm drives a MOSFET gate (D9, PWM) to extract
 *    maximum power from the solar panel.  Battery State-of-Charge
 *    (SOC) is estimated from the open-circuit voltage curve and
 *    drives a four-stage charger (BULK → ABSORPTION → FLOAT →
 *    EQUALIZE) plus priority-based load shedding.
 *    A 16×2 I²C LCD and JSON serial output provide real-time
 *    visibility.
 *
 *  Pin Map:
 *    D4  – Load 1 relay (highest priority)
 *    D5  – Load 2 relay
 *    D6  – Load 3 relay
 *    D7  – Load 4 relay (lowest priority)
 *    D9  – MOSFET gate (MPPT PWM, 31 kHz via Timer1)
 *    D11 – Green LED  (charging active)
 *    D12 – Red LED    (battery low alert)
 *    D13 – Buzzer     (critical SOC warning)
 *    A2  – Battery voltage backup (resistor divider)
 *    I²C – INA219 ×3 (0x40 solar, 0x41 battery, 0x44 load)
 *    I²C – 16×2 LCD  (PCF8574 at 0x27)
 *
 *  Libraries required:
 *    Wire, Adafruit_INA219, LiquidCrystal_I2C
 * ============================================================
 */

#include <Wire.h>
#include <Adafruit_INA219.h>
#include <LiquidCrystal_I2C.h>

// ── Pin definitions ─────────────────────────────────────────
#define PIN_LOAD1      4
#define PIN_LOAD2      5
#define PIN_LOAD3      6
#define PIN_LOAD4      7
#define PIN_MPPT_PWM   9
#define PIN_LED_GREEN  11
#define PIN_LED_RED    12
#define PIN_BUZZER     13
#define PIN_BATT_ADC   A2   // Resistor-divider backup (100 kΩ / 47 kΩ)

// ── INA219 instances ─────────────────────────────────────────
Adafruit_INA219 inaSolar(0x40);   // Solar panel
Adafruit_INA219 inaBatt(0x41);    // Battery
Adafruit_INA219 inaLoad(0x44);    // Load bus

// ── 16×2 I²C LCD ─────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ── Charging-state machine ────────────────────────────────────
enum ChargeState { BULK, ABSORPTION, FLOAT_CHARGE, EQUALIZE };
ChargeState chargeState = BULK;

// Voltage thresholds (12 V lead-acid)
const float V_BULK_END   = 14.4f;  // Finish bulk, enter absorption
const float V_ABS_END    = 14.4f;  // Hold for absTime, then float
const float V_FLOAT      = 13.6f;  // Float maintenance voltage
const float V_EQUALIZE   = 14.8f;  // Periodic equalisation voltage

// ── MPPT – Perturb & Observe ──────────────────────────────────
uint8_t  mpptDuty      = 128;   // 0-255 PWM duty cycle
float    mpptLastPower = 0.0f;
int8_t   mpptStep      = 3;     // Perturbation step (±)
unsigned long mpptTimer = 0;
const unsigned long MPPT_INTERVAL = 500; // ms between perturbations

// ── Measurement variables ─────────────────────────────────────
float solarV = 0, solarI = 0, solarW = 0;
float battV  = 0, battI  = 0;
float loadI  = 0, loadW  = 0;
float battVadc = 0;
int   battSOC  = 0;

// ── Load states ───────────────────────────────────────────────
bool loads[4] = {true, true, true, true};  // All on by default

// ── Timing ────────────────────────────────────────────────────
unsigned long lastLog    = 0;
unsigned long absTimer   = 0;
unsigned long eqTimer    = 0;
const unsigned long LOG_INTERVAL = 10000UL;  // 10 s
const unsigned long ABS_HOLD     = 120000UL; // 2 min absorption hold
const unsigned long EQ_INTERVAL  = 7200000UL;// 2-hour equalize cycle

// ── ADC voltage divider constants ────────────────────────────
// R1 = 100 kΩ, R2 = 47 kΩ → Vbatt = Vadc × (100+47)/47
const float ADC_VREF  = 5.0f;
const float ADC_SCALE = (147.0f / 47.0f) * (ADC_VREF / 1023.0f);

// ─────────────────────────────────────────────────────────────
// Battery SOC from open-circuit voltage (12 V lead-acid)
// Lookup table: voltage → SOC%
// ─────────────────────────────────────────────────────────────
int voltageToSOC(float v) {
    // Clamped linear interpolation from OCV table
    const float vt[] = {11.0f, 11.5f, 11.8f, 12.0f, 12.2f,
                        12.4f, 12.6f, 12.8f, 13.0f, 13.2f};
    const int   st[] = {0,     10,    20,    30,    40,
                        50,    60,    75,    90,    100};
    if (v <= vt[0]) return 0;
    if (v >= vt[9]) return 100;
    for (int i = 0; i < 9; i++) {
        if (v >= vt[i] && v < vt[i + 1]) {
            float frac = (v - vt[i]) / (vt[i + 1] - vt[i]);
            return (int)(st[i] + frac * (st[i + 1] - st[i]));
        }
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────
// MPPT – Perturb & Observe
// ─────────────────────────────────────────────────────────────
void runMPPT() {
    if (millis() - mpptTimer < MPPT_INTERVAL) return;
    mpptTimer = millis();

    float currentPower = solarW;

    if (currentPower >= mpptLastPower) {
        // Moving in right direction, keep perturbing
        mpptDuty = constrain(mpptDuty + mpptStep, 10, 245);
    } else {
        // Power dropped – reverse direction
        mpptStep  = -mpptStep;
        mpptDuty  = constrain(mpptDuty + mpptStep, 10, 245);
    }
    mpptLastPower = currentPower;
    analogWrite(PIN_MPPT_PWM, mpptDuty);
}

// ─────────────────────────────────────────────────────────────
// Charging state machine
// ─────────────────────────────────────────────────────────────
void updateChargeState() {
    switch (chargeState) {

        case BULK:
            // Charge at maximum current until voltage rises to V_BULK_END
            if (battV >= V_BULK_END) {
                chargeState = ABSORPTION;
                absTimer    = millis();
            }
            break;

        case ABSORPTION:
            // Hold at V_ABS_END until current tapers or timer expires
            if (millis() - absTimer >= ABS_HOLD) {
                chargeState = FLOAT_CHARGE;
            }
            // If battery voltage drops significantly, go back to BULK
            if (battV < 12.8f) {
                chargeState = BULK;
            }
            break;

        case FLOAT_CHARGE:
            // Maintain at V_FLOAT; periodic equalization
            if (millis() - eqTimer >= EQ_INTERVAL) {
                chargeState = EQUALIZE;
                eqTimer     = millis();
            }
            // Drop back to BULK if load drains battery
            if (battV < 12.4f) {
                chargeState = BULK;
            }
            break;

        case EQUALIZE:
            // Bring cells to full equality
            if (millis() - eqTimer >= 3600000UL) { // 1 hour
                chargeState = FLOAT_CHARGE;
                eqTimer     = millis();
            }
            if (battV < 12.4f) {
                chargeState = BULK;
            }
            break;
    }
}

// ─────────────────────────────────────────────────────────────
// Priority-based load shedding
// ─────────────────────────────────────────────────────────────
void updateLoadShedding() {
    if (battSOC < 15) {
        // Critical – shed all loads
        loads[0] = false;
        loads[1] = false;
        loads[2] = false;
        loads[3] = false;
    } else if (battSOC < 30) {
        // Low – keep only highest-priority loads 1 & 2
        loads[0] = true;
        loads[1] = true;
        loads[2] = false;
        loads[3] = false;
    } else {
        // Normal – all loads on
        loads[0] = true;
        loads[1] = true;
        loads[2] = true;
        loads[3] = true;
    }

    // Write relay pins (active-HIGH relay module)
    digitalWrite(PIN_LOAD1, loads[0] ? HIGH : LOW);
    digitalWrite(PIN_LOAD2, loads[1] ? HIGH : LOW);
    digitalWrite(PIN_LOAD3, loads[2] ? HIGH : LOW);
    digitalWrite(PIN_LOAD4, loads[3] ? HIGH : LOW);
}

// ─────────────────────────────────────────────────────────────
// Alert LEDs & buzzer
// ─────────────────────────────────────────────────────────────
void updateAlerts() {
    bool charging = (solarW > 1.0f);  // Solar is delivering power

    digitalWrite(PIN_LED_GREEN, charging ? HIGH : LOW);
    digitalWrite(PIN_LED_RED,   (battSOC < 20) ? HIGH : LOW);

    if (battSOC < 10) {
        // Short beep every 3 s
        if ((millis() / 3000) % 2 == 0) {
            tone(PIN_BUZZER, 1000, 200);
        }
    } else {
        noTone(PIN_BUZZER);
    }
}

// ─────────────────────────────────────────────────────────────
// LCD update – row 0: solar & SOC, row 1: load state & state
// ─────────────────────────────────────────────────────────────
void updateLCD() {
    lcd.setCursor(0, 0);
    lcd.print("Sol:");
    lcd.print(solarW, 1);
    lcd.print("W  ");

    lcd.setCursor(10, 0);
    lcd.print("SOC");
    if (battSOC < 10) lcd.print(" ");
    if (battSOC < 100) lcd.print(" ");
    lcd.print(battSOC);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("Ld:");
    lcd.print(loadW, 1);
    lcd.print("W ");

    // Abbreviate charge state
    lcd.setCursor(10, 1);
    switch (chargeState) {
        case BULK:        lcd.print("BULK  "); break;
        case ABSORPTION:  lcd.print("ABS   "); break;
        case FLOAT_CHARGE:lcd.print("FLOAT "); break;
        case EQUALIZE:    lcd.print("EQUAL "); break;
    }
}

// ─────────────────────────────────────────────────────────────
// JSON serial log
// ─────────────────────────────────────────────────────────────
void logJSON() {
    const char* stateStr;
    switch (chargeState) {
        case BULK:         stateStr = "BULK";        break;
        case ABSORPTION:   stateStr = "ABSORPTION";  break;
        case FLOAT_CHARGE: stateStr = "FLOAT";       break;
        case EQUALIZE:     stateStr = "EQUALIZE";    break;
        default:           stateStr = "UNKNOWN";     break;
    }

    Serial.print(F("{\"solar_v\":"));  Serial.print(solarV, 2);
    Serial.print(F(",\"solar_i\":"));  Serial.print(solarI, 3);
    Serial.print(F(",\"solar_w\":"));  Serial.print(solarW, 2);
    Serial.print(F(",\"batt_v\":"));   Serial.print(battV,  2);
    Serial.print(F(",\"batt_soc\":"));Serial.print(battSOC);
    Serial.print(F(",\"batt_i\":"));   Serial.print(battI,  3);
    Serial.print(F(",\"load_w\":"));   Serial.print(loadW,  2);
    Serial.print(F(",\"mppt_duty\":"));Serial.print(map(mpptDuty, 0, 255, 0, 100));
    Serial.print(F(",\"state\":\""));  Serial.print(stateStr);
    Serial.print(F("\",\"loads\":["));
    for (int i = 0; i < 4; i++) {
        Serial.print(loads[i] ? 1 : 0);
        if (i < 3) Serial.print(',');
    }
    Serial.println(F("]}"));
}

// ─────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Relay & indicator pins
    pinMode(PIN_LOAD1,     OUTPUT);
    pinMode(PIN_LOAD2,     OUTPUT);
    pinMode(PIN_LOAD3,     OUTPUT);
    pinMode(PIN_LOAD4,     OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED,   OUTPUT);
    pinMode(PIN_BUZZER,    OUTPUT);

    // All loads on at startup
    digitalWrite(PIN_LOAD1, HIGH);
    digitalWrite(PIN_LOAD2, HIGH);
    digitalWrite(PIN_LOAD3, HIGH);
    digitalWrite(PIN_LOAD4, HIGH);

    // MPPT PWM – Timer1 phase-correct 31 kHz on D9
    TCCR1A = _BV(COM1A1) | _BV(WGM10);
    TCCR1B = _BV(CS10);
    OCR1A  = mpptDuty;

    // INA219 sensors
    inaSolar.begin();
    inaBatt.begin();
    inaLoad.begin();

    // Calibrate INA219 for high current (optional tweak)
    inaSolar.setCalibration_32V_2A();
    inaBatt.setCalibration_32V_2A();
    inaLoad.setCalibration_32V_2A();

    // LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Solar Microgrid");
    lcd.setCursor(0, 1);
    lcd.print("  Initialising  ");
    delay(2000);
    lcd.clear();

    Serial.println(F("# Smart Energy Management System v1.0.0"));
    Serial.println(F("# Format: JSON, interval 10s"));
}

// ─────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────
void loop() {
    // ── Read sensors ─────────────────────────────────────────
    solarV = inaSolar.getBusVoltage_V()
           + inaSolar.getShuntVoltage_mV() / 1000.0f;
    solarI = inaSolar.getCurrent_mA() / 1000.0f;
    solarW = inaSolar.getPower_mW()   / 1000.0f;

    battV  = inaBatt.getBusVoltage_V()
           + inaBatt.getShuntVoltage_mV() / 1000.0f;
    battI  = inaBatt.getCurrent_mA()  / 1000.0f;

    loadI  = inaLoad.getCurrent_mA()  / 1000.0f;
    loadW  = inaLoad.getPower_mW()    / 1000.0f;

    // Battery voltage crosscheck via ADC divider
    battVadc = analogRead(PIN_BATT_ADC) * ADC_SCALE;
    // Prefer INA219 reading; fall back to ADC if INA219 returns 0
    if (battV < 0.5f) battV = battVadc;

    // Sanity-clamp negatives (shunt noise at low current)
    if (solarI < 0) solarI = 0;
    if (solarW < 0) solarW = 0;

    // ── Derived quantities ────────────────────────────────────
    battSOC = voltageToSOC(battV);

    // ── Control algorithms ────────────────────────────────────
    runMPPT();
    updateChargeState();
    updateLoadShedding();
    updateAlerts();

    // ── Periodic display & logging ────────────────────────────
    if (millis() - lastLog >= LOG_INTERVAL) {
        lastLog = millis();
        updateLCD();
        logJSON();
    }
}
