# Drowsiness Detection (MediaPipe + OpenCV)

## Description
Python script that detects drowsiness using eye aspect ratio and MediaPipe facial landmarks.  
When drowsiness is detected, it sends a signal to the ESP32 Display Node through HTTP.

## Dependencies
See `requirements.txt`:
- opencv-python
- mediapipe
- requests
- numpy
