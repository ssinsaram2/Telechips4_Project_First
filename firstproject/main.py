import time
import threading
import socket
import cv2
import numpy as np

# 우리가 만든 모듈 import
from face_worker import FaceWorker
from yolo_worker import YoloWorker

# [중앙 제어 변수] 
CURRENT_MODE = 0  # 기본값을 대기(0) 상태로 시작
mode_lock = threading.Lock()

# [네트워크 세팅 - UDP Listener로 수정]
# 라파4 코드가 포트 지정이 없을 때 기본값으로 송신하는 5020번 포트를 엽니다.
MY_IP = "0.0.0.0"      # 라파5의 모든 네트워크 인터페이스에서 수신 허용
MY_PORT = 5020         # 라파4의 UDP 목적지 기본 포트

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

# [수정] 라파4로부터 UDP 패킷을 상시 수신하는 백그라운드 스레드
def network_receiver_thread():
    global CURRENT_MODE
    
    # TCP(SOCK_STREAM) 대신 UDP(SOCK_DGRAM) 소켓 생성
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # 포트 재사용 설정 (종료 후 재실행 시 포트 선점 에러 방지)
    # server_socket.setsockopt(socket.socket.SOL_SOCKET, socket.socket.SO_REUSEADDR, 1)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    print(f"[네트워크] UDP 포트 {MY_PORT}번 바인딩 및 대기 중...")
    try:
        server_socket.bind((MY_IP, MY_PORT))
        print("[네트워크] UDP 수신 서버 준비 완료! 라파4의 명령을 대기합니다.")
        
        while True:
            # UDP는 연결(connect/accept) 없이 recvfrom으로 패킷을 바로 받습니다.
            data, addr = server_socket.recvfrom(1024)
            if not data:
                continue
            
            print(f"[RAW DATA 수신 from {addr}]: {data}")

            # 문자열로 변환 및 공백 제거
            command = data.decode('utf-8').strip()
            
            # 수신된 명령 검증 및 처리 (테스트용 '5' 포함)
            if command in ['0', '1', '2', '5']:
                new_mode = int(command)
                
                # 테스트로 보낸 5번은 AI 미사용(0) 처리하거나 원하는 로직으로 매핑
                if new_mode == 5:
                    new_mode = 0 
                
                with mode_lock:
                    CURRENT_MODE = new_mode
                
                mode_str = {0: "AI Disabled", 1: "InsightFace Only", 2: "YOLO Only"}.get(new_mode, "Unknown")
                print(f"[네트워크 명령 수신] 모드가 변경되었습니다 -> {new_mode}: {mode_str}")
                
    except Exception as e:
        print(f"[네트워크 에러] 오류 발생: {e}")
    finally:
        server_socket.close()
        print("[네트워크] UDP 소켓을 안전하게 닫았습니다.")

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
        if mode == 1:
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

        mode_str = {0: "AI Disabled", 1: "InsightFace Only", 2: "YOLO Only"}.get(mode, "Unknown")
        cv2.putText(frame, f"MODE: {mode} ({mode_str})", (10, frame.shape[0] - 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        cv2.putText(frame, f"FPS: {fps_ema:.1f}", (frame.shape[1] - 120, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)

        cv2.imshow("Central Control AI System", frame)

        # 제어는 라파4가 하므로 키보드 입력은 'q'(종료)만 처리
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break

finally:
    cap.release()
    cv2.destroyAllWindows()
    face_worker.close()
    yolo_worker.close()
    print("시스템이 안전하게 종료되었습니다.")