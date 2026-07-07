import cv2

class DriverRecognizerModule:
    def __init__(self, recognizer):
        self.recognizer = recognizer
        self.pure_insight_app = recognizer.app

    def process_frame(self, frame):
        faces = self.pure_insight_app.get(frame)
        for face in faces:
            x1, y1, x2, y2 = face.bbox.astype(int)
            emb = face.normed_embedding
            
            best_name, best_sim = "Unknown", -1.0
            for name, info in self.recognizer.registered.items():
                if info["role"] == "driver":
                    sim = float(dict.__getitem__(info, "embedding") @ emb)
                    if sim > best_sim:
                        best_name, best_sim = name, sim
            
            if best_sim > self.recognizer.sim_threshold:
                color = (255, 0, 0)
                label = f"DRIVER: {best_name} ({best_sim:.2f})"
            else:
                color = (0, 0, 255)
                label = "Unknown Face"
                
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            cv2.putText(frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
        return frame