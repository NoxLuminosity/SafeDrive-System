# Sensor Node v3.3

## Description
Arduino sketch for the **Sensor Node** that collects driver data using the MPU6050, MAX30105, and FSR sensors.  
Data is transmitted to the Display Node via **ESP-NOW**.

## Features
- Collects motion, pulse, and grip data  
- Detects driver inactivity  
- Activates buzzer and LED on drowsiness signal  
- Communicates via ESP-NOW

## Dependencies
List of required libraries (see `libraries.txt`):
- Adafruit_GFX
- Adafruit_SSD1306
- ESP-NOW
- WiFi
- MPU6050
- MAX30105
