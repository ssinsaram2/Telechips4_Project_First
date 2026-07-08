import os
import time
import cv2
import numpy as np
import threading
import socket
from insightface.app import FaceAnalysis

# ============================================================
# [네트워크 설정] 라즈베리파이 4 서버 정보 (식별자 데이터 전송용)
# ============================================================
RPI4_IP = "192.168.111.112"  # 상황에 맞는 라파4의 실제 이더넷 고정 IP
RPI4_PORT = 5000

client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    # 실시간 루프 진입 전 1회만 연결을 시도합니다.
    client_socket.connect((RPI4_IP, RPI4_PORT))
    print(f"[네트워크] 라파4({RPI4_IP}:{RPI4_PORT}) 데이터 연결 성공")
except Exception as e:
    print(f"[네트워크 에러] 라파4 연결 실패 (통신 없이 로컬로만 실행합니다): {e}")
    client_socket = None

# [최적화 1] 불필요한 모델(나이, 성별, 3D 특징점) 로드를 제외하여 연산량 감소
app = FaceAnalysis(
    name='buffalo_l',
    allowed_modules=['detection', 'recognition'],  # 검출과 인식만 사용
    providers=['CPUExecutionProvider']
)

# [최적화 2] 입력 해상도를 640x640에서 320x320으로 축소 (CPU 연산량 4배 감소)
app.prepare(ctx_id=0, det_size=(320, 320))

# ============================================================
# [변경 완료] RTSP 네트워크 주소 대신, 복제된 11번 가상 카메라 사용
# ============================================================
VIRTUAL_CAM_INDEX = 11
cap = cv2.VideoCapture(VIRTUAL_CAM_INDEX)

# 하드웨어 버퍼 밀림 방지를 위한 고정값 세팅
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

# ============================================================
# [추가 1] 1:N 탑승자 식별용 등록 DB
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
    master_img = cv2.imread("SJH.jpg")
    if master_img is None:
        print("등록된 사용자 찾을 수 없습니다.")
        if client_socket: client_socket.close()
        exit()
    master_faces = app.get(master_img)
    if not master_faces:
        print("마스터 이미지에서 얼굴을 찾을 수 없습니다.")
        if client_socket: client_socket.close()
        exit()
    registered["SJH"] = master_faces[0].normed_embedding
    print("[등록 완료]")

if not registered:
    print("등록된 얼굴이 없습니다. faces/ 폴더에 이름.jpg 파일을 넣어주세요.")
    if client_socket: client_socket.close()
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

# [추가 2] FPS 측정 (지수 이동 평균으로 값 흔들림 완화)
fps_ema = 0.0
EMA_ALPHA = 0.1  
prev_time = time.perf_counter()

# while True: 진입 직전, 메인 스레드에서 창을 미리 만들어 둡니다.
cv2.namedWindow("Face Identify System")

# 검은색 안내 화면용 빈 이미지 미리 준비 (640x480 사이즈)
disconnect_img = np.zeros((480, 640, 3), dtype=np.uint8)
cv2.putText(disconnect_img, "Camera Stream Disconnected. Checking...", (50, 240),
            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

while True:
    with read_lock:
        if latest_frame is None:
            frame = None
        else:
            frame = latest_frame.copy()

    if frame is None:
        cv2.imshow("Face Identify System", disconnect_img)
        if cv2.waitKey(10) & 0xFF == ord('q'):  
            break
        time.sleep(0.1)  
        continue

    faces = app.get(frame)

    for face in faces:
        x1, y1, x2, y2 = face.bbox.astype(int)
        emb = face.normed_embedding

        # 등록된 모든 사람과 유사도 비교 → 최고 유사도인 사람 선택
        best_name, best_sim = None, -1.0
        for name, ref_emb in registered.items():
            sim = float(np.dot(ref_emb, emb))
            if sim > best_sim:
                best_name, best_sim = name, sim

        if best_sim > SIM_THRESHOLD:
            color = (0, 255, 0)
            label = f"{best_name} ({best_sim:.2f})"
            print(f"[감지] {best_name}님이 감지되었습니다. (유사도: {best_sim:.2f})")
            
            # --------------------------------------------------------
            # [이더넷 전송] 감지된 식별자 라파4로 즉시 송신
            # --------------------------------------------------------
            if client_socket is not None:
                try:
                    send_data = f'{best_name}\n'
                    client_socket.sendto("1\n".encode('utf-8'), (RPI4_IP, RPI4_PORT))
                except Exception as e:
                    print(f"[Network sending failed] data loss occured: {e}")
            # --------------------------------------------------------
        else:
            color = (0, 0, 255)
            label = f"Unknown ({best_sim:.2f})"

        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
        cv2.putText(frame, label, (x1, y1 - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)

    # FPS 계산 및 우측 상단 표시
    now = time.perf_counter()
    dt = now - prev_time
    prev_time = now
    if dt > 0:
        inst_fps = 1.0 / dt
        fps_ema = inst_fps if fps_ema == 0.0 else (EMA_ALPHA * inst_fps + (1 - EMA_ALPHA) * fps_ema)

    fps_text = f"FPS: {fps_ema:.1f}"
    (tw, th), _ = cv2.getTextSize(fps_text, cv2.FONT_HERSHEY_SIMPLEX, 0.7, 2)
    fx = frame.shape[1] - tw - 10  
    fy = th + 10
    cv2.rectangle(frame, (fx - 6, fy - th - 6), (fx + tw + 6, fy + 6), (0, 0, 0), -1)
    cv2.putText(frame, fps_text, (fx, fy),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

    cv2.imshow("Face Identify System", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# 자원 해제 및 소켓 닫기
cap.release()
cv2.destroyAllWindows()
if client_socket:
    client_socket.close()
    print("[네트워크] 연결이 안전하게 해제되었습니다.")
