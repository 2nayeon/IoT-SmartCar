#include <Servo.h>
#include <SoftwareSerial.h>

SoftwareSerial BT(5, 6); 

Servo servoLeft;
Servo servoRight;

#define LEFT_S   2
#define RIGHT_S  4
#define TRIG_F   8
#define ECHO_F   9

#define TRIG_B   10
#define ECHO_B   11

#define LED_L A0
#define LED_R A1
#define LDR   A2
#define BUZZER A3

const int SPEED_FWD = 60;
const int SPEED_TURN_FAST = 80;
const int SPEED_TURN_ADJUST = 50;
const int OBSTACLE_DIST = 8;

const int PARKING_DIST = 8; 

const int BACK_SPEED_L = -55;
const int BACK_SPEED_R = -35;

int traceMode = 0;

void stopMove() {
  servoLeft.writeMicroseconds(1500);
  servoRight.writeMicroseconds(1500);
}

long getDistance() {
  digitalWrite(TRIG_F, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_F, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_F, LOW);
  long duration = pulseIn(ECHO_F, HIGH);
  return duration * 0.034 / 2;
}

long getBackDistance() {
  digitalWrite(TRIG_B, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_B, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_B, LOW);
  long duration = pulseIn(ECHO_B, HIGH); 
  return duration * 0.034 / 2;
}

void maneuver(int left, int right, int ms) {
  servoLeft.writeMicroseconds(1500 + left);
  servoRight.writeMicroseconds(1500 - right);
  if (ms > 0) delay(ms);
}

void runSmartTracing() {
  int L = digitalRead(LEFT_S);
  int R = digitalRead(RIGHT_S);
  long distance = getDistance();

  if (distance > 0 && distance <= OBSTACLE_DIST) {
    stopMove();
    tone(BUZZER, 2000); delay(100); noTone(BUZZER);
    return;
  }

  if (traceMode == 0) {
    if (L == 0) traceMode = 1;
    else if (R == 0) traceMode = 2;
    else maneuver(SPEED_FWD, SPEED_FWD, 20);
  }
  else if (traceMode == 1) {
    if (R == 0) {
      traceMode = 2;
      maneuver(0, 0, 100);
      maneuver(SPEED_TURN_FAST, -80, 400);
    }
    else if (L == 0) maneuver(SPEED_FWD, SPEED_FWD, 20);
    else maneuver(0, SPEED_TURN_ADJUST, 20);
  }
  else if (traceMode == 2) {
    if (L == 0) {
      traceMode = 1;
      maneuver(0, 0, 100);
      maneuver(-80, SPEED_TURN_FAST, 400);
    }
    else if (R == 0) maneuver(SPEED_FWD, SPEED_FWD, 20);
    else maneuver(SPEED_TURN_ADJUST, 0, 20);
  }
}

void autoHeadlight() {
  int lightVal = analogRead(LDR);
  if (lightVal < 600) {
    analogWrite(LED_L, 180);
    analogWrite(LED_R, 180);
  } 
  else {
    analogWrite(LED_L, 0);
    analogWrite(LED_R, 0);
  }
}

void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  pinMode(LEFT_S, INPUT);
  pinMode(RIGHT_S, INPUT);
  pinMode(TRIG_F, OUTPUT);
  pinMode(ECHO_F, INPUT);
  pinMode(TRIG_B, OUTPUT);
  pinMode(ECHO_B, INPUT);
  pinMode(LED_L, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  servoLeft.attach(13);
  servoRight.attach(12);

  stopMove();

  tone(BUZZER, 2000, 100); delay(150);
  tone(BUZZER, 3000, 100);

  Serial.println("ABot Ready!");
}

void loop() {

  autoHeadlight();

  if (BT.available()) {

    char cmd = BT.read();
    if (cmd == '\n' || cmd == '\r') return;

    if (cmd == 'X') {
      stopMove();
      tone(BUZZER, 1000, 200);
    }

    else if (cmd == 'S') {
      traceMode = 0;
      tone(BUZZER, 3000, 100);

      while (true) {
        autoHeadlight();

        long dist = getDistance();
        if (dist > 0 && dist < 10) {
          stopMove();
          for (int i = 0; i < 3; i++) { tone(BUZZER, 2000, 100); delay(150); }
          break;
        }

        runSmartTracing();
        if (BT.available() && BT.read() == 'X') break;
      }
    }

    else if (cmd == 'D') {
      traceMode = 0;
      tone(BUZZER, 3000, 100);

      while (true) {
        autoHeadlight();
        runSmartTracing();
        if (BT.available() && BT.read() == 'X') break;
      }
    }

    else if (cmd == 'P') {

      Serial.println("Auto Parking START!");
      tone(BUZZER, 3000, 100);

      Serial.println("Step 1: Curve Back Left...");

      unsigned long startTime = millis();
      while (true) {
        long b_dist = getBackDistance();

        if ((b_dist > 0 && b_dist < PARKING_DIST)) {
          stopMove(); delay(500); break;
        }
        if (millis() - startTime > 2000) {
          stopMove(); break;
        }

        maneuver(-30, -80, 0);
        delay(10);

        if (BT.available() && BT.read() == 'X') return;
      }

      Serial.println("Step 1.5: Straight Backup...");
      while (true) {
        long b_dist = getBackDistance();
        if ((b_dist > 0 && b_dist <= PARKING_DIST) || b_dist == 0) {
          stopMove(); break;
        }

        maneuver(BACK_SPEED_L, BACK_SPEED_R, 0);
        delay(20);

        if (BT.available() && BT.read() == 'X') return;
      }
      delay(500);

      Serial.println("Step 2: Wari-gari (3 times)");

      for (int i = 1; i <= 3; i++) {

        maneuver(40, 40, 400);
        stopMove(); delay(300);

        unsigned long backupStart = millis();
        while (true) {
          long b_dist = getBackDistance();

          if ((b_dist > 0 && b_dist <= PARKING_DIST) || b_dist == 0) {
            stopMove(); break;
          }
          if (millis() - backupStart > 2000) {
            stopMove(); break;
          }

          maneuver(BACK_SPEED_L, BACK_SPEED_R, 0);
          delay(20);

          if (BT.available() && BT.read() == 'X') return;
        }

        delay(300);
      }

      Serial.println("Parking Complete!");
      for (int i = 0; i < 3; i++) {
        analogWrite(LED_L, 255);
        analogWrite(LED_R, 255);
        tone(BUZZER, 2000); delay(200);
        analogWrite(LED_L, 0);
        analogWrite(LED_R, 0);
        noTone(BUZZER); delay(200);
      }
    }

  } 
}
