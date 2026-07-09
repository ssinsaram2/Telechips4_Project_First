import os
import cv2
import numpy as np
import socket
from insightface.app import FaceAnalysis

class FaceWorker:
    def __init__(self, rpi_ip="192.168.111.112", rpi_port=5000):
        # 1. 네트워크 및 모델 초기화
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self.client_socket.connect((rpi_ip, rpi_port))
            print(f"[Face] 라파4({rpi_ip}:{rpi_port}) 연결 성공")
        except Exception as e:
            print(f"[Face 네트워크 에러] 로컬로만 실행합니다: {e}")
            self.client_socket = None

        self.app = FaceAnalysis(
            name='buffalo_l',
            allowed_modules=['detection', 'recognition'],
            providers=['CPUExecutionProvider']
        )
        self.app.prepare(ctx_id=0, det_size=(320, 320))
        
        self.registered = {}
        self.sim_threshold = 0.45
        self._load_database()

    def _load_database(self):
        FACE_DIR = "faces"
        if os.path.isdir(FACE_DIR):
            for fname in os.listdir(FACE_DIR):
                if not fname.lower().endswith(('.jpg', '.jpeg', '.png')):
                    continue
                name = os.path.splitext(fname)[0]
                img = cv2.imread(os.path.join(FACE_DIR, fname))
                if img is None: continue
                found = self.app.get(img)
                if found:
                    self.registered[name] = found[0].normed_embedding
                    print(f"[Face 등록] {name}")
        
        if not self.registered:
            print("[Face 경고] 등록된 얼굴이 없습니다. faces/ 폴더를 확인하세요.")

    def process_frame(self, frame):
        """메인에서 프레임을 받아 얼굴 인식을 수행하고 원본 프레임에 시각화합니다."""
        faces = self.app.get(frame)
        for face in faces:
            x1, y1, x2, y2 = face.bbox.astype(int)
            emb = face.normed_embedding

            best_name, best_sim = None, -1.0
            for name, ref_emb in self.registered.items():
                sim = float(np.dot(ref_emb, emb))
                if sim > best_sim:
                    best_name, best_sim = name, sim

            if best_sim > self.sim_threshold:
                color = (0, 255, 0)
                label = f"{best_name} ({best_sim:.2f})"
                
                # 라즈베리파이 전송
                if self.client_socket is not None:
                    try:
                        self.client_socket.sendto("1\n".encode('utf-8'), self.client_socket.getpeername())
                    except Exception:
                        pass
            else:
                color = (0, 0, 255)
                label = f"Unknown ({best_sim:.2f})"

            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            cv2.putText(frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
        return frame

    def close(self):
        if self.client_socket:
            self.client_socket.close()