import sys
import time
import cv2
import threading
from ultralytics import YOLO
from face_recognizer import BusFaceRecognizer

# 1. 모델 및 모듈 로드 (task='detect' 명시로 불필요한 경고 메시지 차단)
yolo_model = YOLO("yolov8n_ncnn_model", task="detect")
recognizer = BusFaceRecognizer(base_face_dir="faces", sim_threshold=0.45)

inside_bus_children = {}
disembarked_tracks = set()
ENTRY_EXIT_LINE_X = 500 

# 카메라는 오직 여기서 딱 한 번만 엽니다.
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

print("🚨 다중 운전자/아동 분류 통학 버스 시스템 가동...")
time.sleep(1.0) # 카메라 안정화 대기

try:
    while True:
        # 스레드로부터 최신 이미지 프레임 확보
        with read_lock:
            if latest_frame is None:
                continue
            frame = latest_frame.copy()
            latest_frame = None

        # YOLO 실시간 추적 진행
        results = yolo_model.track(
            source=frame, show=False, classes=0, persist=True, tracker="bytetrack.yaml",
            conf=0.4, iou=0.5, verbose=False
        )
        
        r = results[0] # 한 장에 대한 결과 확보
        
        if r.boxes.id is not None:
            track_ids = r.boxes.id.int().tolist()
            boxes = r.boxes.xyxy.int().tolist()
            
            for t_id, box in zip(track_ids, boxes):
                x1, y1, x2, y2 = box
                center_x = (x1 + x2) / 2
                
                # =================================================================
                # A. 기준선 왼쪽 (버스 내부 영역)
                # =================================================================
                if center_x < ENTRY_EXIT_LINE_X:
                    # [핵심 변경점] 하차했던 ID가 다시 버스 내부로 돌아왔다면 블랙리스트에서 해제!
                    if t_id in disembarked_tracks:
                        disembarked_tracks.discard(t_id)
                        print(f"🔄 하차했던 ID:{t_id}가 다시 버스 내부로 재진입하여 추적을 재개합니다.")

                    # 최초로 발견된 고유 ID라면 초기 상태 등록
                    if t_id not in inside_bus_children:
                        inside_bus_children[t_id] = {
                            "name": "Unknown", 
                            "role": "unknown", 
                            "face_identified": False
                        }
                    
                    # 아직 얼굴이 식별되지 않은 대상(False)만 안면 인식 엔진 가동
                    if not inside_bus_children[t_id]["face_identified"]:
                        h, w, _ = frame.shape
                        x1_c, y1_c = max(0, x1), max(0, y1)
                        x2_c, y2_c = min(w, x2), min(h, y2)
                        
                        person_crop = frame[y1_c:y2_c, x1_c:x2_c]
                        name, role = recognizer.identify_face(person_crop)
                        
                        # 인식 결과 매칭에 성공한 경우 상태 업그레이드 및 락(Lock)
                        if name != "Unknown":
                            if role == "driver":
                                if t_id in inside_bus_children:
                                    del inside_bus_children[t_id]
                                print(f"👨‍✈️ 운전기사 [{name}] 포착. 잔류 아동 카운트에서 예외 처리합니다.")
                                continue
                            
                            inside_bus_children[t_id] = {
                                "name": name, 
                                "role": role, 
                                "face_identified": True
                            }
                            print(f"🎯 [인식 성공] ID:{t_id} 고유 식별 완료 -> [{name}]")

                    # 화면 렌더링을 위한 데이터 로드
                    curr_info = inside_bus_children.get(t_id, {"name": "Unknown", "role": "unknown"})
                    if curr_info["role"] == "driver":
                        color = (255, 0, 0)
                        label = f"DRIVER: {curr_info['name']}"
                    else:
                        color = (0, 255, 0)
                        display_name = curr_info["name"] if curr_info["name"] != "Unknown" else "Unregistered"
                        label = f"ID:{t_id} {display_name}"
                        
                    cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
                    cv2.putText(frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
                
                # =================================================================
                # B. 기준선 오른쪽 (하차 영역)
                # =================================================================
                elif center_x >= ENTRY_EXIT_LINE_X:
                    # [구조 변경] 이미 하차 처리되어 블랙리스트에 있는 ID라면 중복 연산 패스
                    if t_id in disembarked_tracks:
                        continue

                    if t_id in inside_bus_children:
                        left_info = inside_bus_children.pop(t_id)
                        
                        if left_info["role"] != "driver":
                            disembarked_tracks.add(t_id)
                            target_name = left_info["name"]
                            
                            if target_name != "Unknown":
                                print(f"👋 [하차 확정] {target_name} 원생이 내렸습니다. 관련된 모든 유령 트랙을 정리합니다.")
                                ghost_keys = [
                                    k for k, v in inside_bus_children.items() 
                                    if v["name"] == target_name
                                ]
                                for gk in ghost_keys:
                                    inside_bus_children.pop(gk, None)
                            else:
                                print(f"👋 [하차 확정] 미등록(Unknown) 대상이 내렸습니다.")

        # 기준선 그리기
        cv2.line(frame, (ENTRY_EXIT_LINE_X, 0), (ENTRY_EXIT_LINE_X, frame.shape[0]), (0, 0, 255), 2)
        
        # 중복 이름 제거 로직 가동 (리스트를 set으로 변경하여 중복 삭제)
        raw_children_names = [
            info["name"] for info in inside_bus_children.values() 
            if info["role"] == "child"
        ]
        unique_children_names = list(set(raw_children_names))
        actual_children_count = len(unique_children_names)
        
        # 터미널에 실시간으로 중복이 없는 잔류 어린이 명단 출력
        if actual_children_count > 0:
            names_str = ", ".join(unique_children_names)
            print(f"🚌 [버스 내부 잔류 아동 명단]: {names_str} (총 {actual_children_count}명)", end="\r")
        else:
            print("🚌 [버스 내부 잔류 아동 명단]: 없음 (총 0명)                          ", end="\r")
        
        # FPS 및 모니터링 현황 렌더링
        now = time.perf_counter()
        dt = now - prev_time
        prev_time = now
        if dt > 0:
            inst_fps = 1.0 / dt
            fps_ema = inst_fps if fps_ema == 0.0 else (EMA_ALPHA * inst_fps + (1 - EMA_ALPHA) * fps_ema)
        
        cv2.putText(frame, f"FPS: {fps_ema:.1f}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
        cv2.putText(frame, f"Inside Children: {actual_children_count}명", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
        
        # 화면창 좌측 하단에도 중복이 제거된 명단 투영
        if actual_children_count > 0:
            cv2.putText(frame, f"Names: {', '.join(unique_children_names)}", (10, frame.shape[0] - 20), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        cv2.imshow("Bus Safety Guard System", frame)
        
        if cv2.waitKey(1) & 0xFF == ord('q'):
            print("\n👋 시스템을 종료합니다.")
            break

except (KeyboardInterrupt, Exception) as e:
    print(f"\n🛑 에러 또는 정지: {e}")
finally:
    cap.release()
    cv2.destroyAllWindows()
    sys.exit(0)