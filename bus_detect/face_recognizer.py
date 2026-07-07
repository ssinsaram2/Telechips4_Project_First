import os
import cv2
import numpy as np
from insightface.app import FaceAnalysis

class BusFaceRecognizer:
    def __init__(self, base_face_dir="faces", sim_threshold=0.45):
        self.sim_threshold = sim_threshold
        self.registered = {}  # {이름: {"embedding": 벡터, "role": "driver"|"child"}}
        
        self.app = FaceAnalysis(
            name='buffalo_l',
            allowed_modules=['detection', 'recognition'],
            providers=['CPUExecutionProvider']
        )
        self.app.prepare(ctx_id=0, det_size=(320, 320))
        
        # 각각의 폴더를 스캔하여 역할을 지정하며 로드
        self._load_group(os.path.join(base_face_dir, "drivers"), role="driver")
        self._load_group(os.path.join(base_face_dir, "children"), role="child")

    def _load_group(self, folder_path, role):
        """특정 폴더를 읽어 지정된 역할(role)로 DB에 등록합니다."""
        if not os.path.isdir(folder_path):
            print(f"[경고] 폴더를 찾을 수 없습니다: {folder_path}")
            return

        for fname in os.listdir(folder_path):
            if not fname.lower().endswith(('.jpg', '.jpeg', '.png')):
                continue
            name = os.path.splitext(fname)[0]
            img = cv2.imread(os.path.join(folder_path, fname))
            if img is None:
                continue
                
            found = self.app.get(img)
            if found:
                # 임베딩값과 역할을 함께 저장 (구조화)
                self.registered[name] = {
                    "embedding": found[0].normed_embedding,
                    "role": role
                }
                print(f"[안면 DB 등록 완료] 분류: {role} | 이름: {name}")
            else:
                print(f"[안면 DB 등록 실패] {fname}: 얼굴을 찾을 수 없음")

    def identify_face(self, person_crop):
        """YOLO 크롭 이미지에서 얼굴을 매칭하고 (이름, 역할)을 반환합니다."""
        if person_crop.size == 0:
            return "Unknown", "unknown"

        faces = self.app.get(person_crop)
        if not faces:
            return "Unknown", "unknown"

        emb = faces[0].normed_embedding
        best_name, best_role, best_sim = "Unknown", "unknown", -1.0

        # 1:N 비교 수행
        for name, info in self.registered.items():
            sim = float(np.dot(info["embedding"], emb))
            if sim > best_sim:
                best_name, best_role, best_sim = name, info["role"], sim

        if best_sim > self.sim_threshold:
            return best_name, best_role
        
        return "Unknown", "unknown"