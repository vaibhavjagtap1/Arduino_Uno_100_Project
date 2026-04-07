/*
 * ============================================================
 *  Project 004: Autonomous Line-Following Robot with
 *               Obstacle Avoidance and PID Control
 * ============================================================
 *  Author      : vaibhavjagtap1
 *  Board       : Arduino Uno R3
 *  Version     : 1.0.0
 *  Date        : 2025
 * ============================================================
 *  Description :
 *    A 2-wheel-drive robot that follows a black line on a white
 *    surface using 5 TCRT5000 IR sensors and a PID controller.
 *    An HC-SR04 ultrasonic sensor detects obstacles ahead and
 *    triggers an avoidance state machine. Motor speed is
 *    controlled via L298N PWM inputs.
 *
 *  Hardware Summary:
 *    - 5× IR sensors  : D2, D3, A0, A1, A2
 *    - HC-SR04        : TRIG=D8, ECHO=D9
 *    - L298N          : ENA=D10, IN1=D4, IN2=D5
 *                       ENB=D11, IN3=D6, IN4=D7
 *    - Status LED     : D13
 *
 *  Pin Map:
 *    D2  ── IR Sensor S0 (far left)
 *    D3  ── IR Sensor S1 (left)
 *    A0  ── IR Sensor S2 (center)
 *    A1  ── IR Sensor S3 (right)
 *    A2  ── IR Sensor S4 (far right)
 *    D8  ── HC-SR04 TRIG
 *    D9  ── HC-SR04 ECHO
 *    D10 ── L298N ENA (PWM, left motor speed)
 *    D4  ── L298N IN1
 *    D5  ── L298N IN2
 *    D11 ── L298N ENB (PWM, right motor speed)
 *    D6  ── L298N IN3
 *    D7  ── L298N IN4
 *    D13 ── Status LED
 * ============================================================
 */

// ── Pin Definitions ─────────────────────────────────────────
// IR Sensors (LOW = on black line = 1)
#define S0_PIN   2   // Far-left sensor
#define S1_PIN   3   // Left sensor
#define S2_PIN   A0  // Center sensor
#define S3_PIN   A1  // Right sensor
#define S4_PIN   A2  // Far-right sensor

// HC-SR04 Ultrasonic
#define TRIG_PIN 8
#define ECHO_PIN 9

// L298N Motor Driver
#define ENA_PIN  10  // Left motor PWM speed
#define IN1_PIN  4   // Left motor direction A
#define IN2_PIN  5   // Left motor direction B
#define ENB_PIN  11  // Right motor PWM speed
#define IN3_PIN  6   // Right motor direction A
#define IN4_PIN  7   // Right motor direction B

// Status LED
#define LED_PIN  13

// ── PID Constants ────────────────────────────────────────────
#define KP 0.4f   // Proportional gain
#define KI 0.01f  // Integral gain
#define KD 0.3f   // Derivative gain

// ── Speed Constants ──────────────────────────────────────────
#define BASE_SPEED      150  // Normal cruising PWM (0-255)
#define MAX_SPEED       200  // Maximum allowable PWM
#define TURN_SPEED      130  // Speed during avoidance turns
#define BACKUP_SPEED    120  // Speed while reversing

// ── Obstacle Threshold ───────────────────────────────────────
#define OBSTACLE_DIST_CM 15  // Trigger avoidance below this (cm)

// ── State Machine ────────────────────────────────────────────
enum RobotState {
  LINE_FOLLOW,        // Normal PID line following
  OBSTACLE_DETECTED,  // Obstacle found, deciding turn direction
  AVOID_LEFT,         // Turning left to bypass obstacle
  AVOID_RIGHT,        // Turning right to bypass obstacle
  SEARCH              // All sensors lost line, searching
};

// ── Global PID Variables ─────────────────────────────────────
float lastError  = 0.0f;
float integral   = 0.0f;
const float setpoint = 0.0f;  // Desired position: line under center

// ── Global State ─────────────────────────────────────────────
RobotState currentState = LINE_FOLLOW;

// Cached values for JSON output
int   sensorValues[5] = {0};
float linePosition    = 0.0f;
float pidOutput       = 0.0f;
int   leftMotorSpeed  = 0;
int   rightMotorSpeed = 0;
float lastDist        = 999.0f;

// Serial print throttle
unsigned long lastPrintMs = 0;
const unsigned long PRINT_INTERVAL_MS = 100;

// ============================================================
//  readSensors()
//  Reads all 5 IR sensors into the global sensorValues array.
//  Convention: LOW signal → sensor over black line → value = 1
// ============================================================
void readSensors() {
  sensorValues[0] = (digitalRead(S0_PIN) == LOW) ? 1 : 0;
  sensorValues[1] = (digitalRead(S1_PIN) == LOW) ? 1 : 0;
  sensorValues[2] = (digitalRead(S2_PIN) == LOW) ? 1 : 0;
  sensorValues[3] = (digitalRead(S3_PIN) == LOW) ? 1 : 0;
  sensorValues[4] = (digitalRead(S4_PIN) == LOW) ? 1 : 0;
}

// ============================================================
//  computePosition()
//  Weighted average of sensor positions.
//  Weights: S0=-2, S1=-1, S2=0, S3=+1, S4=+2
//  Returns a float in [-2, +2]:
//    negative → line is to the left
//    zero     → line is centered
//    positive → line is to the right
// ============================================================
float computePosition() {
  const int weights[5] = {-2, -1, 0, 1, 2};
  int   weightedSum = 0;
  int   activeCount = 0;

  for (int i = 0; i < 5; i++) {
    weightedSum += sensorValues[i] * weights[i];
    activeCount += sensorValues[i];
  }

  if (activeCount == 0) {
    // No sensors active — preserve last known position
    return linePosition;
  }
  return (float)weightedSum / (float)activeCount;
}

// ============================================================
//  computePID(float error)
//  Classic P·I·D calculation.
//  Integral is clamped to prevent wind-up.
//  Returns a correction value to add/subtract from BASE_SPEED.
// ============================================================
float computePID(float error) {
  // Proportional term
  float P = KP * error;

  // Integral term with anti-windup clamp (±50)
  integral += error;
  integral  = constrain(integral, -50.0f, 50.0f);
  float I   = KI * integral;

  // Derivative term
  float derivative = error - lastError;
  float D = KD * derivative;

  lastError = error;
  return P + I + D;
}

// ============================================================
//  measureDistance()
//  Triggers HC-SR04 and measures echo pulse width.
//  Returns distance in centimetres (float).
//  Returns 999 if no echo received within timeout.
// ============================================================
float measureDistance() {
  // Ensure TRIG is low before pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // 10 µs HIGH pulse to trigger
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure echo duration (timeout = 25 ms → ~432 cm max)
  long duration = pulseIn(ECHO_PIN, HIGH, 25000UL);
  if (duration == 0) return 999.0f;

  // Speed of sound ≈ 0.0343 cm/µs; divide by 2 for round-trip
  return (float)duration * 0.0343f / 2.0f;
}

// ============================================================
//  setMotors(int leftSpeed, int rightSpeed)
//  Drives both motors at the given PWM speeds.
//  Positive → forward, negative → backward.
//  Speed is clamped to [-MAX_SPEED, +MAX_SPEED].
// ============================================================
void setMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -MAX_SPEED, MAX_SPEED);
  rightSpeed = constrain(rightSpeed, -MAX_SPEED, MAX_SPEED);

  // ── Left Motor ──────────────────────────────────────────
  if (leftSpeed >= 0) {
    digitalWrite(IN1_PIN, HIGH);
    digitalWrite(IN2_PIN, LOW);
  } else {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, HIGH);
    leftSpeed = -leftSpeed;
  }
  analogWrite(ENA_PIN, leftSpeed);

  // ── Right Motor ─────────────────────────────────────────
  if (rightSpeed >= 0) {
    digitalWrite(IN3_PIN, HIGH);
    digitalWrite(IN4_PIN, LOW);
  } else {
    digitalWrite(IN3_PIN, LOW);
    digitalWrite(IN4_PIN, HIGH);
    rightSpeed = -rightSpeed;
  }
  analogWrite(ENB_PIN, rightSpeed);

  // Cache for telemetry
  leftMotorSpeed  = leftSpeed;
  rightMotorSpeed = rightSpeed;
}

// ============================================================
//  stopMotors()
//  Coast stop — removes PWM drive from both motors.
// ============================================================
void stopMotors() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, 0);
  analogWrite(ENB_PIN, 0);
  leftMotorSpeed  = 0;
  rightMotorSpeed = 0;
}

// ============================================================
//  avoidObstacle()
//  Blocking obstacle-avoidance manoeuvre:
//    1. Back up briefly
//    2. Check left and right for clearance
//    3. Turn toward the clearer side
//    4. Inch forward past the obstacle
//    5. Return to LINE_FOLLOW
// ============================================================
void avoidObstacle() {
  // Step 1: Stop and back up
  stopMotors();
  delay(200);
  setMotors(-BACKUP_SPEED, -BACKUP_SPEED);
  delay(400);
  stopMotors();
  delay(150);

  // Step 2: Check left clearance (turn left ~45°, measure)
  setMotors(-TURN_SPEED, TURN_SPEED);
  delay(300);
  stopMotors();
  delay(100);
  float distLeft = measureDistance();

  // Return to center
  setMotors(TURN_SPEED, -TURN_SPEED);
  delay(300);
  stopMotors();
  delay(100);

  // Step 3: Check right clearance (turn right ~45°, measure)
  setMotors(TURN_SPEED, -TURN_SPEED);
  delay(300);
  stopMotors();
  delay(100);
  float distRight = measureDistance();

  // Step 4: Choose side with more clearance
  if (distLeft >= distRight) {
    // ── Avoid LEFT ─────────────────────────────────────
    currentState = AVOID_LEFT;
    digitalWrite(LED_PIN, HIGH);

    // Swing back to face forward
    setMotors(-TURN_SPEED, TURN_SPEED);
    delay(300);
    stopMotors();
    delay(100);

    // Turn left ~90°
    setMotors(-TURN_SPEED, TURN_SPEED);
    delay(600);
    stopMotors();
    delay(150);

    // Drive forward past obstacle
    setMotors(BASE_SPEED, BASE_SPEED);
    delay(800);
    stopMotors();
    delay(150);

    // Turn right ~90° back toward original heading
    setMotors(TURN_SPEED, -TURN_SPEED);
    delay(600);
    stopMotors();
    delay(150);
  } else {
    // ── Avoid RIGHT ────────────────────────────────────
    currentState = AVOID_RIGHT;
    digitalWrite(LED_PIN, HIGH);

    // Already facing right from step 3, turn more
    setMotors(TURN_SPEED, -TURN_SPEED);
    delay(300);
    stopMotors();
    delay(150);

    // Drive forward past obstacle
    setMotors(BASE_SPEED, BASE_SPEED);
    delay(800);
    stopMotors();
    delay(150);

    // Turn left ~90° back toward original heading
    setMotors(-TURN_SPEED, TURN_SPEED);
    delay(600);
    stopMotors();
    delay(150);
  }

  // Step 5: Re-acquire line
  currentState = LINE_FOLLOW;
  integral  = 0.0f;  // Reset integrator after avoidance
  lastError = 0.0f;
  digitalWrite(LED_PIN, LOW);
}

// ============================================================
//  printJSON()
//  Outputs a single-line JSON telemetry packet over Serial.
//  Format:
//  {"sensors":[s0,s1,s2,s3,s4],"pos":X.XX,"error":X.XX,
//   "pid_out":X.XX,"left_spd":NNN,"right_spd":NNN,
//   "state":"STATE","dist":XX.X}
// ============================================================
void printJSON() {
  // State name string
  const char* stateName;
  switch (currentState) {
    case LINE_FOLLOW:       stateName = "LINE_FOLLOW";       break;
    case OBSTACLE_DETECTED: stateName = "OBSTACLE_DETECTED"; break;
    case AVOID_LEFT:        stateName = "AVOID_LEFT";        break;
    case AVOID_RIGHT:       stateName = "AVOID_RIGHT";       break;
    case SEARCH:            stateName = "SEARCH";            break;
    default:                stateName = "UNKNOWN";           break;
  }

  Serial.print(F("{\"sensors\":["));
  for (int i = 0; i < 5; i++) {
    Serial.print(sensorValues[i]);
    if (i < 4) Serial.print(',');
  }
  Serial.print(F("],\"pos\":"));
  Serial.print(linePosition,  2);
  Serial.print(F(",\"error\":"));
  Serial.print(linePosition - setpoint, 2);
  Serial.print(F(",\"pid_out\":"));
  Serial.print(pidOutput, 2);
  Serial.print(F(",\"left_spd\":"));
  Serial.print(leftMotorSpeed);
  Serial.print(F(",\"right_spd\":"));
  Serial.print(rightMotorSpeed);
  Serial.print(F(",\"state\":\""));
  Serial.print(stateName);
  Serial.print(F("\",\"dist\":"));
  Serial.print(lastDist, 1);
  Serial.println('}');
}

// ============================================================
//  setup()
// ============================================================
void setup() {
  Serial.begin(9600);

  // IR sensor inputs
  pinMode(S0_PIN, INPUT);
  pinMode(S1_PIN, INPUT);
  pinMode(S2_PIN, INPUT);
  pinMode(S3_PIN, INPUT);
  pinMode(S4_PIN, INPUT);

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Motor driver outputs
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);

  // Status LED
  pinMode(LED_PIN, OUTPUT);

  // Startup blink: 3 flashes to signal ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }

  stopMotors();

  Serial.println(F("{\"event\":\"startup\",\"project\":\"004\","
                   "\"version\":\"1.0.0\"}"));
}

// ============================================================
//  loop()
// ============================================================
void loop() {
  // 1. Read all sensors
  readSensors();

  // 2. Compute line position
  linePosition = computePosition();

  // 3. Measure obstacle distance (non-blocking; runs every loop)
  lastDist = measureDistance();

  // 4. Check for obstacle — highest-priority interrupt
  if (lastDist < OBSTACLE_DIST_CM && currentState == LINE_FOLLOW) {
    currentState = OBSTACLE_DETECTED;
    digitalWrite(LED_PIN, HIGH);
    stopMotors();
    delay(100);
    avoidObstacle();
    return;  // Skip rest of loop; avoidObstacle() is blocking
  }

  // 5. Detect "all sensors off" → SEARCH state
  int totalActive = 0;
  for (int i = 0; i < 5; i++) totalActive += sensorValues[i];

  if (totalActive == 0 && currentState == LINE_FOLLOW) {
    currentState = SEARCH;
  } else if (totalActive > 0 && currentState == SEARCH) {
    currentState = LINE_FOLLOW;
    integral  = 0.0f;
    lastError = 0.0f;
  }

  // 6. Drive according to state
  if (currentState == LINE_FOLLOW) {
    // ── PID line following ──────────────────────────────
    float error = linePosition - setpoint;
    pidOutput   = computePID(error);

    int leftSpd  = (int)(BASE_SPEED - pidOutput);
    int rightSpd = (int)(BASE_SPEED + pidOutput);

    setMotors(leftSpd, rightSpd);

  } else if (currentState == SEARCH) {
    // ── Search: spin slowly to re-acquire line ──────────
    // Turn in the direction of last known error
    if (lastError >= 0) {
      setMotors(TURN_SPEED, -TURN_SPEED);  // Spin right
    } else {
      setMotors(-TURN_SPEED, TURN_SPEED);  // Spin left
    }
  }

  // 7. Serial telemetry (throttled)
  unsigned long now = millis();
  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    printJSON();
  }
}
