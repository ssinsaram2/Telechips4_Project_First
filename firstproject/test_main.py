import time
import threading
import cv2
import numpy as np

# 우리가 만든 모듈 import
from face_worker import FaceWorker
from yolo_worker import YoloWorker

# [중앙 제어 변수] 
# 0: InsightFace 전용, 1: 모두 안함(대기), 2: 둘 다 사용
CURRENT_MODE = 0  
mode_lock = threading.Lock()

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

# 카메라 백그라운드 스레드 시작
t = threading.Thread(target=capture_thread, daemon=True)
t.start()

# 워커 인스턴스 생성
face_worker = FaceWorker()
yolo_worker = YoloWorker()

cv2.namedWindow("Central Control AI System")
print("\n=== 시스템 제어 가이드 ===")
print("키보드 [0] 입력: InsightFace 전용 모드")
print("키보드 [1] 입력: 모든 AI 중지 모드 (스트림만 출력)")
print("키보드 [2] 입력: YOLO 전용 모드")
print("키보드 [q] 입력: 시스템 종료\n")

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
            # 연결 유실 시 검은 화면 표시
            disconnect_img = np.zeros((480, 640, 3), dtype=np.uint8)
            cv2.putText(disconnect_img, "Camera Disconnected...", (50, 240),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
            cv2.imshow("Central Control AI System", disconnect_img)
            if cv2.waitKey(10) & 0xFF == ord('q'): break
            continue

        # 현재 모드 확인
        with mode_lock:
            mode = CURRENT_MODE

        # [중앙 제어 로직] 상태에 따른 모델 분기 처리
        if mode == 0:
            frame = face_worker.process_frame(frame)
        elif mode == 2:
            frame = yolo_worker.process_frame(frame)
        # mode == 1 일 때는 아무 연산도 처리하지 않고 순수 프레임만 전달

        # FPS 및 모드 상태 UI 드로잉
        now = time.perf_counter()
        dt = now - prev_time
        prev_time = now
        if dt > 0:
            inst_fps = 1.0 / dt
            fps_ema = inst_fps if fps_ema == 0.0 else (EMA_ALPHA * inst_fps + (1 - EMA_ALPHA) * fps_ema)

        # 화면에 현재 모드 상태 표시
        mode_str = {0: "InsightFace Only", 1: "AI Disabled", 2: "YOLO Only"}.get(mode, "Unknown")
        cv2.putText(frame, f"MODE: {mode} ({mode_str})", (10, frame.shape[0] - 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        cv2.putText(frame, f"FPS: {fps_ema:.1f}", (frame.shape[1] - 120, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)

        cv2.imshow("Central Control AI System", frame)

        # 사용자 입력에 따른 실시간 제어 스위칭
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break
        elif key == ord('0'):
            with mode_lock: CURRENT_MODE = 0
            print("[상태 변경] 0번 모드: InsightFace 활성화")
        elif key == ord('1'):
            with mode_lock: CURRENT_MODE = 1
            print("[상태 변경] 1번 모드: 모든 AI 비활성화")
        elif key == ord('2'):
            with mode_lock: CURRENT_MODE = 2
            print("[상태 변경] 2번 모드: InsightFace & YOLO 동시 활성화")

finally:
    cap.release()
    cv2.destroyAllWindows()
    face_worker.close()
    yolo_worker.close()
    print("시스템이 안전하게 종료되었습니다.")