#include <Arduino.h>
#include <Wire.h>
#include "HUSKYLENS.h"
#include <Servo.h>

HUSKYLENS huskylens;

Servo yawServo;   // Pin 10 (TD-8135MG Base)
Servo pitchServo; // Pin 11 (Standard Arm)
Servo rollServo;  // Pin 12 (The Firing Mechanism)

float pitchAngle = 90.0; 
const float Kp_Pitch = 0.015; 

float currentYawSpeed = 90.0;  
const float bufferRate = 0.35; 

// --- THE LOCK-ON TIMER VARIABLES ---
// Declared globally here so the loop never loses track of them
unsigned long lockStartTime = 0; 
bool isLocked = false;           

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  yawServo.attach(10);
  pitchServo.attach(11);
  rollServo.attach(12);
  
  pitchServo.write((int)pitchAngle);
  yawServo.write(90); 
  rollServo.write(90); 
  
  while (!huskylens.begin(Wire)) {
    Serial.println("HuskyLens connection failed!");
    delay(100);
  }
  
  huskylens.writeAlgorithm(ALGORITHM_FACE_RECOGNITION);
}

void loop() {
  if (huskylens.request()) {
    
    if (!huskylens.available()) {
      // EMPTY ROOM: Instant Brakes and reset the firing timer
      currentYawSpeed = 90;
      yawServo.write(90);
      isLocked = false; 
    } 
    else {
      while (huskylens.available()) {
        HUSKYLENSResult result = huskylens.read();
        
        if (result.command == COMMAND_RETURN_BLOCK) {
          int currentX = result.xCenter;
          int currentY = result.yCenter;
          int errorY = 120 - currentY;

          // --- ASYMMETRIC BRAKING LOGIC ---
          // Using a wider deadzone (100 to 220) to brake earlier
          if (currentX < 100) {
            // MOVE LEFT: Buffer the acceleration so we don't jerk
            currentYawSpeed += (100 - currentYawSpeed) * bufferRate;
            yawServo.write((int)currentYawSpeed);
          } 
          else if (currentX > 220) {
            // MOVE RIGHT: Buffer the acceleration so we don't jerk
            currentYawSpeed += (80 - currentYawSpeed) * bufferRate;  
            yawServo.write((int)currentYawSpeed);
          } 
          else {
            // IN THE DEADZONE: Instant digital brakes. Kill the motor immediately.
            currentYawSpeed = 90;
            yawServo.write(90);  
          }

          // --- POSITIONAL PITCH (ARM) LOGIC ---
          if (abs(errorY) > 15) {
            pitchAngle += (errorY * Kp_Pitch);
            pitchAngle = constrain(pitchAngle, 50, 130); 
            pitchServo.write((int)pitchAngle);
          }

          // --- THE 2-SECOND AUTO-FIRE LOGIC (SHOOT EVERYONE EXCEPT ID 1) ---
          
          if (currentX >= 100 && currentX <= 220 && abs(errorY) <= 15 && result.ID != 1) {
            
            // If we just entered the deadzone, start the stopwatch
            if (!isLocked) {
              isLocked = true;
              lockStartTime = millis(); 
              Serial.println("HOSTILE ACQUIRED (NOT ID 1) - LOCKING ON...");
            } 
            // If we've been locked on for 2000ms (2 seconds)
            else if (millis() - lockStartTime >= 2000) {
              
              Serial.println("FIRING!");
              
              // Spin the roll servo
              rollServo.write(180); 
              delay(300);           
              rollServo.write(90);  
              
              // Reset the stopwatch so it waits 2 seconds before firing again
              lockStartTime = millis(); 
            }
          } 
          else {
            // Face left the center crosshairs, or the VIP (ID 1) stepped in.
            if (isLocked) {
              if (result.ID == 1) {
                 Serial.println("VIP RECOGNIZED (ID 1) - HOLDING FIRE.");
              } else {
                 Serial.println("TARGET LOST - RESETTING TIMER.");
              }
            }
            isLocked = false;
          }
        }
      }
    }
  }
  
  delay(20); 
}
