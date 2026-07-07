import cv2

class PureObjectDetectorModule:
    def __init__(self, yolo_model):
        self.yolo_model = yolo_model

    def process_frame(self, frame):
        results = self.yolo_model.predict(source=frame, classes=0, conf=0.25, verbose=False)
        for box in results[0].boxes:
            x1, y1, x2, y2 = box.xyxy[0].int().tolist()
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 165, 255), 2)
            cv2.putText(frame, "Person", (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 165, 255), 2)
        return frame