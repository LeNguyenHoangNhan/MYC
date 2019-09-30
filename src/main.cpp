#include <Arduino.h>
#include <Adafruit_Si7021.h>
#include <LiquidCrystal_I2C.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPAsyncWebServer.h>
#include "main.hpp"
#include <Wire.h>

AsyncWebServer server(80);
LiquidCrystal_I2C lcd (0x27, 16, 2);
TwoWire wire(1);
Adafruit_Si7021 sensor(&wire);

void setup() {
  Serial.begin(9600);
  wire.begin(18, 19);
  if (sensor.begin()) {
    Serial.println("Failed to init sensor, please check the wiring");
    for (;;) {
      Serial.println(".");
      delay(100);
    }
  }
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // put your setup code here, to run once:
}

void loop() {
  pinMode(2, HIGH);
  delay(1000);
  pinMode(2, LOW);
  delay(1000);
  // put your main code here, to run repeatedly:
}