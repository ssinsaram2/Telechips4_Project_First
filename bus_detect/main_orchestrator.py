import sys
import time
import cv2
import threading
from ultralytics import YOLO
from face_recognizer import BusFaceRecognizer

# 나머지 3개의 독립 컴포넌트 모듈 파일로부터 클래스 로드
from driver_recognizer import DriverRecognizerModule
from bus_child_tracker import BusChildTrackerModule
from pure_object_detector import PureObjectDetectorModule

# 1. 공통 AI 핵심 가중치 엔진 단 한번만 초기화 및 로드
print("⚙️ Loading Shared AI Backend Models...")
shared_yolo = YOLO("yolov8n_ncnn_model", task="detect")
shared_recognizer = BusFaceRecognizer(base_face_dir="faces", sim_threshold=0.45)

# 2. 각 전담 모듈 인스턴스 생성 및 의존성 공유 주입
driver_module = DriverRecognizerModule(shared_recognizer)
child_module = BusChildTrackerModule(shared_yolo, shared_recognizer)
pure_yolo_module = PureObjectDetectorModule(shared_yolo)

# 상태 제어 매핑 관리자 (영문 변환으로 깨짐 현상 원천 차단)
current_mode = 0
mode_names = {
    0: "MODE 1: Driver Face Recognition (InsightFace)",
    1: "MODE 2: Bus Child Counter (YOLO + InsightFace)",
    2: "MODE 3: Pure Body Detection (YOLO Only)"
}

# 싱글 카메라 스레딩 관리 파이프라인
cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

latest_frame = None
read_lock = threading.Lock()

def capture_thread():
    global latest_frame
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.01)
            continue
        with read_lock:
            latest_frame = frame

t = threading.Thread(target=capture_thread, daemon=True)
t.start()

fps_ema = 0.0
EMA_ALPHA = 0.1
prev_time = time.perf_counter()

print("🚨 Master Orchestrator Activated Successfully.")
print("⌨️  Press [Key 1] -> Driver | [Key 2] -> Child Tracker | [Key 3] -> Pure YOLO")
time.sleep(1.0)

try:
    while True:
        with read_lock:
            if latest_frame is None: continue
            frame = latest_frame.copy()
            latest_frame = None

        # 런타임 라우팅 및 동적 플러그인 제어
        if current_mode == 0:
            frame = driver_module.process_frame(frame)
        elif current_mode == 1:
            frame = child_module.process_frame(frame)
        elif current_mode == 2:
            frame = pure_yolo_module.process_frame(frame)

        # 안내 UI 렌더링 및 FPS 노출
        cv2.putText(frame, mode_names[current_mode], (10, frame.shape[0] - 50), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 255), 2)
        
        # 키보드 조작 가이드 추가 노출
        cv2.putText(frame, "Press Key [1], [2], [3] to Switch Mode", (10, frame.shape[0] - 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
        
        now = time.perf_counter()
        dt = now - prev_time
        prev_time = now
        if dt > 0:
            inst_fps = 1.0 / dt
            fps_ema = inst_fps if fps_ema == 0.0 else (EMA_ALPHA * inst_fps + (1 - EMA_ALPHA) * fps_ema)
        cv2.putText(frame, f"FPS: {fps_ema:.1f}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

        cv2.imshow("Bus Multi-AI Guard System", frame)
        
        # 키보드 이벤트 분기 처리
        key = cv2.waitKey(1) & 0xFF
        if key == ord('1'):
            current_mode = 0
            print(f"\n🔘 [Event] Switched to -> {mode_names[current_mode]}")
        elif key == ord('2'):
            current_mode = 1
            print(f"\n🔘 [Event] Switched to -> {mode_names[current_mode]}")
        elif key == ord('3'):
            current_mode = 2
            print(f"\n🔘 [Event] Switched to -> {mode_names[current_mode]}")
        elif key == ord('q'):
            print("\n👋 Shutting down orchestrator safely.")
            break

except (KeyboardInterrupt, Exception) as e:
    print(f"\n🛑 Critical Error: {e}")
finally:
    cap.release()
    cv2.destroyAllWindows()
    sys.exit(0)