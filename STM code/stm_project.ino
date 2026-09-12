#include <Servo.h>
#include <math.h>

#define NTC_PIN PA0
#define LDR_PIN PA1
#define GAS_PIN PA2

#define SERVO_PIN PA6
#define FAN_ENA PB6
#define FAN_IN1 PB13
#define FAN_IN2 PB14

#define RGB_RED PB0
#define RGB_GREEN PB1
#define RGB_BLUE PB10
#define BUZZER_PIN PB5
#define PIR_PIN PB12
#define LED_ROOM PB15


const float R_BALANCE = 10000.0;
const float BETA = 3950.0;
const float ROOM_TEMP_K = 298.15;
const float R_ROOM_TEMP = 10000.0;

const int GAS_THRESHOLD = 700;
const int LDR_THRESHOLD = 2000;

const int MOTION_WINDOW_MS = 8000;
const int MOTION_THRESHOLD = 4;

Servo elServo;

bool cryDetected = false;
bool babyAwake = false;
int servoPos = 0;

unsigned long lastMove = 0;
unsigned long lastSystemReport = 0;

unsigned long motionTimestamps[10];
int motionCount = 0;


float getTemperatureC();
void setFanSpeed(int pwmValue);
void setRGBColor(bool red, bool green, bool blue);

void setup() {
  Serial1.begin(9600);

  pinMode(NTC_PIN ,INPUT_ANALOG);
  pinMode(LDR_PIN ,INPUT_ANALOG);
  pinMode(GAS_PIN ,INPUT_ANALOG);
  pinMode(PIR_PIN ,INPUT_PULLDOWN);
  pinMode(RGB_RED ,OUTPUT);
  pinMode(RGB_GREEN ,OUTPUT);
  pinMode(RGB_BLUE ,OUTPUT);
  pinMode(LED_ROOM ,OUTPUT);
  pinMode(BUZZER_PIN ,OUTPUT);
  pinMode(FAN_ENA ,OUTPUT);
  pinMode(FAN_IN1 ,OUTPUT);
  pinMode(FAN_IN2 ,OUTPUT);

  analogReadResolution(12); //0-4095
  analogWriteResolution(8); //0-255

  elServo.attach(SERVO_PIN);
  elServo.write(90);

  delay(1000);
}

void loop() {
  unsigned long now = millis();

  //recive from python
  if (Serial1.available()> 0) {
    String cmd = Serial1.readStringUntil('\n');
    cmd.trim();

    if (cmd == "CRY_DETECTED") {
      cryDetected = true;
    } 
    else if (cmd == "CRY_ENDED") {
      cryDetected = false;
    } 
    else if (cmd == "BUZZER_ON") {
      digitalWrite(BUZZER_PIN, HIGH);
    } 
    else if (cmd == "BUZZER_OFF") {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  //servo rocking
  if (cryDetected) {
    if (now - lastMove >= 400) {
      lastMove = now;
      if (servoPos == 0) {
        servoPos = 180;
      } else {
        servoPos = 0;
      }
      elServo.write(servoPos);
    }
  } else {
    elServo.write(90);
  }

  //pir
  if (digitalRead(PIR_PIN) == HIGH) {
    if (motionCount == 0 || (now - motionTimestamps[motionCount - 1] > 600)) {
      if (motionCount < 10) {
        motionTimestamps[motionCount] = now;
        motionCount++;
      }
    }
  }

  int validMotions = 0;
  for (int i = 0; i < motionCount; i++) {
    if (now - motionTimestamps[i] <= MOTION_WINDOW_MS) {
      motionTimestamps[validMotions] = motionTimestamps[i];
      validMotions++;
    }
  }
  motionCount = validMotions;

  babyAwake = (motionCount >= MOTION_THRESHOLD);

  //sensors
  if (now - lastSystemReport >= 500) {
    lastSystemReport = now;

    //temp
    float tempC = getTemperatureC();
    int fanPwm = 0;

    if (tempC < 25.0) {
      setRGBColor(false, false, true);
      fanPwm = 0;
    } else if (tempC <= 30.0) {
      setRGBColor(false, true, false);
      fanPwm = 150;
    } else {
      setRGBColor(true, false, false);
      fanPwm = 255;
    }
    setFanSpeed(fanPwm);

    //room led
    int rawLDR = analogRead(LDR_PIN);
    bool isDark = (rawLDR < LDR_THRESHOLD);
    bool TurnLight = isDark && (babyAwake || cryDetected);
    digitalWrite(LED_ROOM, TurnLight ? HIGH : LOW); ///imp

    //gas sesnor
    int rawGas = analogRead(GAS_PIN);
    bool gasDetected = (rawGas > GAS_THRESHOLD);

    if (gasDetected) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else if (!cryDetected) {
      digitalWrite(BUZZER_PIN, LOW);
    }

    //send data to python
    Serial1.print("DATA:TEMP=");
    Serial1.print(tempC, 1); 
    Serial1.print(",GAS=");

    if (gasDetected) {
        Serial1.print("ALERT!");
    } else {
        Serial1.print("Safe");
    }
    Serial1.print(",MOTIONS=");

    if (babyAwake) {
        Serial1.print("Awake");
    } else {
        Serial1.print("Sleeping");
    }

    Serial1.print(",LIGHT=");

    if (isDark) {
        Serial1.println("DARK");
    } else {
        Serial1.println("LIGHT");
    }
      }
    }


float getTemperatureC() {
  int rawNTC = analogRead(NTC_PIN);
  if (rawNTC <= 0) rawNTC = 1;
  if (rawNTC >= 4095) rawNTC = 4094;
  float rNtc = R_BALANCE * ((4095.0 / (float)rawNTC) - 1.0);
  float tempK = 1.0 / ((1.0 / ROOM_TEMP_K) + (1.0 / BETA) * log(rNtc / R_ROOM_TEMP));
  return tempK - 273.15;
}

void setFanSpeed(int pwmValue) {
  if (pwmValue > 0) {
    digitalWrite(FAN_IN1, HIGH);
    digitalWrite(FAN_IN2, LOW);
    analogWrite(FAN_ENA, pwmValue);
  } else {
    digitalWrite(FAN_IN1, LOW);
    digitalWrite(FAN_IN2, LOW);
    analogWrite(FAN_ENA, 0);
  }
}

void setRGBColor(bool red, bool green, bool blue) {
  digitalWrite(RGB_RED, red);
  digitalWrite(RGB_GREEN, green);
  digitalWrite(RGB_BLUE, blue);
}
