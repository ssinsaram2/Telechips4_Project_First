import os
import time
import cv2
import numpy as np
import threading
from insightface.app import FaceAnalysis

# [최적화 1] 불필요한 모델(나이, 성별, 3D 특징점) 로드를 제외하여 연산량 감소
app = FaceAnalysis(
    name='buffalo_l',
    allowed_modules=['detection', 'recognition'],  # 검출과 인식만 사용
    providers=['CPUExecutionProvider']
)

# [최적화 2] 입력 해상도를 640x640에서 320x320으로 축소 (CPU 연산량 4배 감소)
app.prepare(ctx_id=0, det_size=(320, 320))

cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

# ============================================================
# [추가 1] 1:N 탑승자 식별용 등록 DB
# faces/ 폴더에 "이름.jpg" 형태로 넣으면 자동 등록됩니다.
#   예) faces/jehyun.jpg, faces/gyubin.jpg
# 폴더가 없으면 기존처럼 IJW.jpg 한 명만 등록합니다.
# ============================================================
registered = {}  # {이름: normed_embedding}

FACE_DIR = "faces"
if os.path.isdir(FACE_DIR):
    for fname in os.listdir(FACE_DIR):
        if not fname.lower().endswith(('.jpg', '.jpeg', '.png')):
            continue
        name = os.path.splitext(fname)[0]
        img = cv2.imread(os.path.join(FACE_DIR, fname))
        if img is None:
            print(f"[등록 실패] {fname}: 이미지를 읽을 수 없습니다.")
            continue
        found = app.get(img)
        if not found:
            print(f"[등록 실패] {fname}: 얼굴을 찾을 수 없습니다.")
            continue
        registered[name] = found[0].normed_embedding
        print(f"[등록 완료] {name}")
else:
    master_img = cv2.imread("IJW.jpg")
    if master_img is None:
        print("IJW.jpg를 찾을 수 없습니다.")
        exit()
    master_faces = app.get(master_img)
    if not master_faces:
        print("마스터 이미지에서 얼굴을 찾을 수 없습니다.")
        exit()
    registered["IJW"] = master_faces[0].normed_embedding
    print("[등록 완료] IJW")

if not registered:
    print("등록된 얼굴이 없습니다. faces/ 폴더에 이름.jpg 파일을 넣어주세요.")
    exit()

# 유사도 임계값: 1:1 인증은 0.6, 1:N 식별은 0.4~0.5 권장
SIM_THRESHOLD = 0.45

print(f"실시간 탑승자 식별 시작 (등록 인원: {len(registered)}명)")

# [최적화 3] 프레임 버퍼 밀림 방지: 캡처 전용 스레드 + 최신 프레임만 유지
latest_frame = None
read_lock = threading.Lock()

def capture_thread():
    global latest_frame
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break
        with read_lock:
            latest_frame = frame

t = threading.Thread(target=capture_thread, daemon=True)
t.start()

# ============================================================
# [추가 2] FPS 측정 (지수 이동 평균으로 값 흔들림 완화)
# 여기서 재는 FPS는 "처리 FPS" = 검출+인식+렌더링을 포함한
# 실제 파이프라인 처리 속도입니다. 카메라 캡처 FPS와는 별개입니다.
# ============================================================
fps_ema = 0.0
EMA_ALPHA = 0.1  # 값이 작을수록 부드럽게 변함
prev_time = time.perf_counter()

while True:
    with read_lock:
        if latest_frame is None:
            frame = None
        else:
            frame = latest_frame.copy()
            latest_frame = None

    if frame is None:
        time.sleep(0.001)  # busy-wait로 CPU 100% 점유하는 것 방지
        continue

    faces = app.get(frame)

    for face in faces:
        x1, y1, x2, y2 = face.bbox.astype(int)
        emb = face.normed_embedding

        # [추가 1] 등록된 모든 사람과 유사도 비교 → 최고 유사도인 사람 선택
        best_name, best_sim = None, -1.0
        for name, ref_emb in registered.items():
            sim = float(np.dot(ref_emb, emb))
            if sim > best_sim:
                best_name, best_sim = name, sim

        if best_sim > SIM_THRESHOLD:
            color = (0, 255, 0)
            label = f"{best_name} ({best_sim:.2f})"
        else:
            color = (0, 0, 255)
            label = f"Unknown ({best_sim:.2f})"

        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
        cv2.putText(frame, label, (x1, y1 - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)

    # [추가 2] FPS 계산 및 우측 상단 표시
    now = time.perf_counter()
    dt = now - prev_time
    prev_time = now
    if dt > 0:
        inst_fps = 1.0 / dt
        fps_ema = inst_fps if fps_ema == 0.0 else (EMA_ALPHA * inst_fps + (1 - EMA_ALPHA) * fps_ema)

    fps_text = f"FPS: {fps_ema:.1f}"
    (tw, th), _ = cv2.getTextSize(fps_text, cv2.FONT_HERSHEY_SIMPLEX, 0.7, 2)
    fx = frame.shape[1] - tw - 10  # 우측 상단, 오른쪽에서 10px 여백
    fy = th + 10
    # 가독성용 반투명 배경 박스
    cv2.rectangle(frame, (fx - 6, fy - th - 6), (fx + tw + 6, fy + 6), (0, 0, 0), -1)
    cv2.putText(frame, fps_text, (fx, fy),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

    cv2.imshow("Face Identify System", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
