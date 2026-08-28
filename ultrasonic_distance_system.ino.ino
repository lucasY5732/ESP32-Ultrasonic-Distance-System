#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>

// ===== PIN CONFIGURATION =====
const int trigPin = 5;
const int echoPin = 18;
const int buzzerPin = 23;
const int ledPin = 32;
const int ledNum = 8;

// ===== LCD & LED SETUP =====
LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_NeoPixel strip(ledNum, ledPin, NEO_GRB + NEO_KHZ800);

// ===== TIMING VARIABLES =====
volatile long startTime = 0;
volatile long duration = 0;
volatile bool done = false;

float distance = -1;

// ===== INTERRUPT SERVICE ROUTINE (ECHO SIGNAL) =====
void IRAM_ATTR echoISR() {

  // Rising edge: start timing
  if (digitalRead(echoPin) == HIGH) {
    startTime = micros();
  } 
  // Falling edge: calculate pulse duration
  else {
    duration = micros() - startTime;
    done = true;
  }
}

// ===== TRIGGER ULTRASONIC SENSOR =====
void triggerSensor() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
}

// ===== CONVERT DURATION TO DISTANCE =====
float computeDistance(long d) {
  if (d <= 0) return -1;
  return d * 0.034 / 2;
}

// ===== LED COLOR UPDATE (DISTANCE → COLOR) =====
void updateLED(float d) {

  if (d < 0) {
    strip.clear();
    strip.show();
    return;
  }

  int r = 0, g = 0, b = 0;

  // ===== 1️⃣ Below 15 cm: RED → GREEN =====
  if (d < 15) {

    float t = d / 15.0;   // normalized value (0 → 1)

    r = (int)(255 * (1 - t));
    g = (int)(255 * t);
    b = 0;
  }

  // ===== 2️⃣ 15–30 cm: GREEN → BLUE =====
  else if (d <= 30) {

    float t = (d - 15) / 15.0;  // normalized value (0 → 1)

    r = 0;
    g = (int)(255 * (1 - t));
    b = (int)(255 * t);
  }

  // ===== 3️⃣ Above 30 cm: BLUE =====
  else {
    r = 0;
    g = 0;
    b = 255;
  }

  // ===== APPLY COLOR TO ALL LEDs =====
  for (int i = 0; i < ledNum; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }

  strip.show();
}

// ===== BUZZER CONTROL =====
unsigned long lastBeep = 0;

void updateBuzzer(float d) {

  // Disable buzzer if out of range
  if (d < 0 || d > 30) {
    noTone(buzzerPin);
    return;
  }

  // ===== Map distance to beep interval =====
  int interval = map(d, 30, 1, 800, 50); 
  // 30 cm → slow (800 ms)
  // 1 cm  → fast (50 ms)

  unsigned long now = millis();

  if (now - lastBeep >= interval) {
    lastBeep = now;

    tone(buzzerPin, 2000, 30); // short beep pulse
  }
}

// ===== LCD DISPLAY =====
unsigned long lastLCD = 0;

void updateLCD(float d) {

  // ===== Line 1: Distance =====
  lcd.setCursor(0, 0);
  lcd.print("Dist:           ");
  lcd.setCursor(6, 0);

  if (d < 0) {
    lcd.print("ERR      ");
  } else {
    lcd.print(d, 1);
    lcd.print(" cm   ");
  }

  // ===== Line 2: Status =====
  lcd.setCursor(0, 1);

  if (d < 0) {
    lcd.print("NO SIGNAL       ");
  }
  else if (d > 30) {
    lcd.print("SAFE            ");
  }
  else if (d > 15) {
    lcd.print("WARNING         ");
  }
  else {
    lcd.print("DANGER          ");
  }
}

// ===== SETUP =====
void setup() {

  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);

  // Attach interrupt to echo pin
  attachInterrupt(digitalPinToInterrupt(echoPin), echoISR, CHANGE);

  lcd.init();
  lcd.backlight();

  strip.begin();
  strip.show();

  lcd.print("STABLE MODE");
  delay(1000);
  lcd.clear();
}

// ===== MAIN LOOP =====
void loop() {

  // Trigger sensor
  triggerSensor();

  // Wait for echo signal (handled by ISR)
  delay(50);

  // Safely read shared variables
  noInterrupts();
  long d = duration;
  bool ok = done;
  done = false;
  interrupts();

  // Compute distance if valid signal received
  if (ok) {
    distance = computeDistance(d);
  } else {
    distance = -1;
  }

  // Debug output
  Serial.print("DUR=");
  Serial.print(d);
  Serial.print(" DIST=");
  Serial.println(distance);

  // Update outputs
  updateLCD(distance);
  updateLED(distance);
  updateBuzzer(distance);

  delay(100);
}