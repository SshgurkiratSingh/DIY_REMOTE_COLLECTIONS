#include <Arduino.h>

// PIN CONFIGURATION
const int RPWM = 5;
const int LPWM = 6;
const int L_EN = 7;
const int R_EN = 8;

void setup()
{
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  pinMode(L_EN, OUTPUT);
  pinMode(R_EN, OUTPUT);

  // INITIAL STATE: ALL SYSTEMS COLD
  digitalWrite(RPWM, LOW);
  digitalWrite(LPWM, LOW);
  digitalWrite(L_EN, LOW);
  digitalWrite(R_EN, LOW);

  Serial.begin(9600);
  Serial.println("--- MOTOR DRIVER SYSTEM CHECK COMMENCING ---");
  delay(2000);
}

void loop()
{
  // --- MANEUVER 1: FORWARD DRIVE (RPWM) ---
  Serial.println("MANEUVER: FORWARD RAMP (RPWM)");
  digitalWrite(R_EN, HIGH);
  digitalWrite(L_EN, HIGH);

  for (int i = 0; i <= 255; i++)
  {
    analogWrite(RPWM, i);
    digitalWrite(LPWM, LOW); // Ensure opposite side is grounded
    delay(20);               // Faster ramp for efficiency
  }
  delay(1000);
  analogWrite(RPWM, 0);

  // --- MANEUVER 2: REVERSE DRIVE (LPWM) ---
  Serial.println("MANEUVER: REVERSE RAMP (LPWM)");
  for (int i = 0; i <= 255; i++)
  {
    digitalWrite(RPWM, LOW);
    analogWrite(LPWM, i);
    delay(20);
  }
  delay(1000);
  analogWrite(LPWM, 0);

  // --- MANEUVER 3: ENABLE LOGIC VERIFICATION ---
  Serial.println("MANEUVER: ENABLE PIN INHIBIT TEST");
  digitalWrite(R_EN, LOW);
  digitalWrite(L_EN, LOW);
  analogWrite(RPWM, 200); // Attempting drive while disabled
  Serial.println("RESULT: OUTPUT SHOULD BE ZERO (EN LOW)");
  delay(2000);
  analogWrite(RPWM, 0);

  Serial.println("--- CYCLE COMPLETE ---");
  delay(3000);
}