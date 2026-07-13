import time
import threading
import socket  # 추가
import cv2
import numpy as np

# 우리가 만든 모듈 import
from face_worker import FaceWorker
from yolo_worker import YoloWorker

# [중앙 제어 변수] 
CURRENT_MODE = 1  # 기본값을 대기(1) 상태로 시작
mode_lock = threading.Lock()

# [네트워크 세팅]
# 라파4가 5000번 포트를 열고 대기 중이라고 가정합니다.
SERVER_IP = "192.168.111.112"  # 실제 라즈베리파이 4의 IP 주소로 변경하세요.
SERVER_PORT = 5000

# 카메라 세팅
VIRTUAL_CAM_INDEX = 11
cap = cv2.VideoCapture(VIRTUAL_CAM_INDEX)
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

latest_frame = None
frame_lock = threading.Lock()

def capture_thread():
    global latest_frame
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret: break
        with frame_lock:
            latest_frame = frame

# 신규 추가: 라파4로부터 명령을 수신하는 백그라운드 스레드
def network_receiver_thread():
    global CURRENT_MODE
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    print(f"[네트워크] 라파4({SERVER_IP}:{SERVER_PORT}) 연결 시도 중...")
    try:
        client_socket.connect((SERVER_IP, SERVER_PORT))
        print("[네트워크] 라파4와 연결 성공! 명령을 대기합니다.")
        
        while True:
            # 라파4가 보낸 데이터를 바이트 형태로 수신 (예: b'0', b'1', b'2')
            data = client_socket.recv(1024)
            if not data:
                print("[네트워크] 라파4와의 연결이 끊어졌습니다.")
                break
            
            # 문자열로 변환 및 공백 제거
            command = data.decode('utf-8').strip()
            
            # 수신된 명령에 따라 모드 스위칭
            if command in ['0', '1', '2']:
                new_mode = int(command)
                with mode_lock:
                    CURRENT_MODE = new_mode
                
                mode_str = {0: "InsightFace Only", 1: "AI Disabled", 2: "YOLO Only"}.get(new_mode)
                print(f"[네트워크 명령 수신] 모드가 변경되었습니다 -> {new_mode}: {mode_str}")
                
    except Exception as e:
        print(f"[네트워크 에러] 연결 실패 또는 오류 발생: {e}")
    finally:
        client_socket.close()
        print("[네트워크] 소켓을 닫았습니다.")

# 스레드 시작
t_cam = threading.Thread(target=capture_thread, daemon=True)
t_cam.start()

t_net = threading.Thread(target=network_receiver_thread, daemon=True)
t_net.start()

# 워커 인스턴스 생성
face_worker = FaceWorker()
yolo_worker = YoloWorker()

cv2.namedWindow("Central Control AI System")

fps_ema = 0.0
EMA_ALPHA = 0.1
prev_time = time.perf_counter()

try:
    while True:
        with frame_lock:
            if latest_frame is None:
                frame = None
            else:
                frame = latest_frame.copy()

        if frame is None:
            disconnect_img = np.zeros((480, 640, 3), dtype=np.uint8)
            cv2.putText(disconnect_img, "Camera Disconnected...", (50, 240),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
            cv2.imshow("Central Control AI System", disconnect_img)
            if cv2.waitKey(10) & 0xFF == ord('q'): break
            continue

        # 현재 모드 확인 (네트워크 스레드가 변경해 준 값이 반영됨)
        with mode_lock:
            mode = CURRENT_MODE

        # [중앙 제어 로직] 상태에 따른 모델 분기 처리
        if mode == 0:
            frame = face_worker.process_frame(frame)
        elif mode == 2:
            frame = yolo_worker.process_frame(frame)

        # FPS 및 모드 상태 UI 드로잉
        now = time.perf_counter()
        dt = now - prev_time
        prev_time = now
        if dt > 0:
            inst_fps = 1.0 / dt
            fps_ema = inst_fps if fps_ema == 0.0 else (EMA_ALPHA * inst_fps + (1 - EMA_ALPHA) * fps_ema)

        mode_str = {0: "InsightFace Only", 1: "AI Disabled", 2: "YOLO Only"}.get(mode, "Unknown")
        cv2.putText(frame, f"MODE: {mode} ({mode_str})", (10, frame.shape[0] - 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        cv2.putText(frame, f"FPS: {fps_ema:.1f}", (frame.shape[1] - 120, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)

        cv2.imshow("Central Control AI System", frame)

        # 이제 제어는 라파4가 하므로 키보드 입력은 'q'(종료)만 처리
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break

finally:
    cap.release()
    cv2.destroyAllWindows()
    face_worker.close()
    yolo_worker.close()
    print("시스템이 안전하게 종료되었습니다.")