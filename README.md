# SafeDrive System 🚗💤  
An IoT + AI-based Driver Drowsiness and Safety Monitoring System  

## 📘 Project Overview
SafeDrive is an integrated system combining **Arduino IoT sensors** and **AI-based eye tracking** to monitor driver alertness in real time.  
It features:
- Motion, grip, and pulse detection using the **MPU6050**, **FSR**, and **MAX30105** sensors.
- Drowsiness detection using **OpenCV + MediaPipe**.
- ESP-NOW + Wi-Fi communication between **Sensor** and **Display Nodes**.
- Alerts triggered via **buzzer** and **visual indicators**.

---

## 🧩 Components
| Module | Description |
|--------|--------------|
| **Sensor Node** | Collects driver data and transmits via ESP-NOW. |
| **Display Node** | Displays readings, logs events, and triggers alerts. |
| **AI Detection Script** | Uses webcam or phone camera to detect drowsiness. |

---

## 📁 Repository Structure
SafeDrive-System/
│
├── README.md
│
├── arduino/
│   ├── sensor_node_v3.3/
│   │   ├── sensor_node_v3.3.ino
│   │   ├── README.md
│   │   └── libraries.txt
│   │
│   └── display_node_v1.7/
│       ├── display_node_v1.7.ino
│       ├── README.md
│       └── libraries.txt
│
└── python/
    ├── drowsiness_detection/
    │   ├── drowsy_detection.py
    │   ├── requirements.txt
    │   └── README.md

