import cv2
import dlib
import requests
from scipy.spatial import distance as dist

ESP32_IP = "192.168.4.1"
DROIDCAM_URL = "http://192.168.1.10:4747/video"  # change this to your DroidCam feed URL

# Dlib face detector + 68 facial landmark model
detector = dlib.get_frontal_face_detector()
predictor = dlib.shape_predictor("shape_predictor_68_face_landmarks.dat")

# Function to calculate Eye Aspect Ratio (EAR)
def eye_aspect_ratio(eye):
    A = dist.euclidean(eye[1], eye[5])
    B = dist.euclidean(eye[2], eye[4])
    C = dist.euclidean(eye[0], eye[3])
    return (A + B) / (2.0 * C)

(lStart, lEnd) = (42, 48)
(rStart, rEnd) = (36, 42)

cap = cv2.VideoCapture(DROIDCAM_URL)
threshold = 0.22
frames_closed = 0

while True:
    ret, frame = cap.read()
    if not ret:
        print("No frame received.")
        break

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = detector(gray, 0)

    for face in faces:
        shape = predictor(gray, face)
        coords = [(shape.part(i).x, shape.part(i).y) for i in range(68)]
        leftEye = coords[lStart:lEnd]
        rightEye = coords[rStart:rEnd]

        leftEAR = eye_aspect_ratio(leftEye)
        rightEAR = eye_aspect_ratio(rightEye)
        ear = (leftEAR + rightEAR) / 2.0

        state = "Alert" if ear > threshold else "Drowsy"
        color = (0, 255, 0) if state == "Alert" else (0, 0, 255)

        cv2.putText(frame, f"State: {state}", (50, 50),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, color, 2)

        # Send to ESP32
        try:
            requests.get(f"http://{ESP32_IP}/{state.lower()}")
        except:
            pass

    cv2.imshow("Eye Detection", frame)
    if cv2.waitKey(1) == 27:  # press ESC to exit
        break

cap.release()
cv2.destroyAllWindows()
