## 전체 동작

- main.py
    
    ```python
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
    ```
    
- 라즈베리파이5에서 5020 포트인 서버를 열어서 라즈베리파이4로부터 어떤 AI를 켜야하는 지를 수신
- 라파4로 부터 0을 수신하면 AI를 끄고 1을 수신하면 얼굴 인식(Insight Face)을, 2를 수신하면 객체 감지(YOLOv8n)를 실행

---

## 실행 방법

- 터미널 1
    
    ```bash
    # video 디바이스 파일이 있는지 확인
    # ls /dev/video*
    
    # 디바이스 모듈 올리기
    sudo modprobe -r v4l2loopback
    sudo modprobe v4l2loopback devices=2 video_nr=10,11 card_label="Cam10,Cam11" exclusive_caps=1
    
    # 카메라 디바이스가 video10, video11로 나누어졌는지 확인하기
    # ls -l /dev/video10 /dev/video11
    
    # 카메라 디바이스 파일을 두 개로 나누기
    ffmpeg -f v4l2 -i /dev/video0 \
    -map 0:v -codec copy -f v4l2 /dev/video10 \
    -map 0:v -codec copy -f v4l2 /dev/video11
    ```
    
- 터미널 2
    
    ```bash
    # 폴더에 들어가면 가상환경 자동 설정
    cd ~/rp5/AI/firstproject/
    
    # 메인 코드 실행
    python3 main.py
    ```
    

---

## 이미지 빌드

- 우분투 데스크톱 22.04로 빌드
    
    !ubuntu.png
    
    - 한글 세팅, 마우스 세팅 등이 라즈비안보다 편함

---

## VS 코드 설치

- curl 설치
    
    ```bash
    sudo apt update
    sudo apt install curl -y
    ```
    
- vscode 설치 파일 다운
    
    ```bash
    curl -L https://aka.ms/linux-arm64-deb -o code_arm64.deb
    ```
    
- 설치
    
    ```bash
    sudo apt install ./code_arm64.deb -y
    ```
    

---

## 카메라 동작 확인

- 웹캠 연결 확인
    
    !image.png
    
- 비디오 디바이스 파일 확인
    
    !image.png
    
- `v4l-utils` 를 이용해서 디바이스 파일 찾기
    
    ```bash
    install v4l-utils
    v4l2-ctl --list-devices
    ```
    
    !image.png
    
- 확인
    
    ```bash
    # 1. 패키지 목록 업데이트
    sudo apt update
    
    # 2. ffmpeg(ffplay 포함) 설치
    sudo apt install ffmpeg
    
    # 3. 카메라 실행
    ffplay /dev/video0
    ```
    

---

## 카메라 파일 2개로 나누기

### 이유

- 카메라 디바이스 파일이 하나하면 RTSP와 AI 중 하나는 실행을 못하게 됨
- 카메라 디바이스 파일을 두개로 복사해서 사용하여 문제를 해결

### 환경 설정

- 카메라 디바이스 파일 복사해서 두 개로 나눔(video0 → video10, video11)
    
    ```bash
    # video 디바이스 파일이 있는지 확인
    # ls /dev/video*
    
    # 디바이스 모듈 올리기
    sudo modprobe -r v4l2loopback
    sudo modprobe v4l2loopback devices=2 video_nr=10,11 card_label="Cam10,Cam11" exclusive_caps=1
    
    # 카메라 디바이스가 video10, video11로 나누어졌는지 확인하기
    # ls -l /dev/video10 /dev/video11
    
    # 카메라 디바이스 파일을 두 개로 나누기
    ffmpeg -f v4l2 -i /dev/video0 \
    -map 0:v -codec copy -f v4l2 /dev/video10 \
    -map 0:v -codec copy -f v4l2 /dev/video11
    ```
    
    - video0이 원래 웹캠이 사용하던 디바이스 파일인데 video10과 video11로 나눔
- 하나는 rtsp에서 사용하고 하나는 Insightface가 사용하게 함
    
    ```bash
    gst-launch-1.0 v4l2src device=/dev/video10 ! video/x-raw,width=640,height=480,framerate=30/1 ! \
    videoconvert ! x264enc tune=zerolatency bitrate=2000 speed-preset=ultrafast key-int-max=30 ! \
    rtspclientsink location=rtsp://192.168.111.112:8554/test protocols=tcp
    ```
    
    - video10을 rtsp로 사용

---

## Insight Face를 이용한 얼굴 인식

### Insight Face 환경 설정

- 업데이트
    
    ```bash
    sudo apt update && sudo apt upgrade -y
    ```
    
- 필요한 패키지 설치
    
    ```bash
    sudo apt install -y build-essential python3-dev python3-venv cmake libgl1 libglib2.0-0
    ```
    
- 가상환경 구축
    
    ```bash
    python3 -m venv insight_env
    source insight_env/bin/activate
    pip install --upgrade pip setuptools wheel
    ```
    
- insightface 설치
    
    ```bash
    pip install insightface
    ```
    
- 가상환경 들어가기
    
    ```bash
    cd ~/rp5/AI/insight_project
    source insight_env/bin/activate
    ```
    

### 인식 딜레이 테스트

!image.png

- 대략 0.3~0.6 초 정도의 인식 딜레이를 가짐

### 실행

- face_worker.py
    
    ```python
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
                    
                    cmd_mapping = {
                        "SJH": "1\n",
                        "SHG": "2\n"
                    }
    
                    command = cmd_mapping.get(best_name, "1\n")
    
                    # 라즈베리파이 전송
                    if self.client_socket is not None:
                        try:
                            #self.client_socket.sendto("1\n".encode('utf-8'), self.client_socket.getpeername())
                            self.client_socket.sendto(command.encode('utf-8'), self.client_socket.getpeername())
                            print(f"[Face 전송] {best_name} -> {command.strip()}") # 전송 로그 확인용
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
    ```
    
- 식별된 사람을 1, 2, 3의 형태의 데이터로 라즈베리파이4의 프토 5000번으로 전송

---

## YOLOv8n을 이용한 객체(사람) 감지

### yolo 환경 세팅

#### 윈도우

- 파이썬 설치
- yolov8n을 ncnn으로 만들기
    - 코드
        
        ```python
        from ultralytics import YOLO
        
        # 1. 기본 Nano 모델 로드 (인터넷에서 자동으로 다운로드됩니다)
        model = YOLO("yolov8n.pt") 
        
        # 2. 라즈베리파이 최적화 포맷인 NCNN으로 변환
        model.export(format="ncnn")
        ```
        
- yolov8n_ncnn_model 파일이 생성
    
    !image.png
    
- 라파5로 옮기기

#### 라즈베리파이5

- 설치(강제 설치라서 불안한 사람은 가상환경에서 설치하기)
    
    ```bash
    pip install ultralytics --break-system-packages
    ```
    
    ```bash
    pip install ncnn --break-system-packages
    ```
    
- 테스트
    - 신체 부위도 사람으로 인식
        
        !image.png
        
    - 인원 수 체크는 너무 변수가 많음
        
        !image.png
        
        - 손도 개별 객체로 판단해버림
    - 팔꿈치까지 다 보이면 하나로 인식
        
        !image.png
        

#### 딜레이 측정

!image.png

- 0.15~0.3 초

### 실행

- yolo_worker.py
    
    ```python
    import cv2
    import socket
    from ultralytics import YOLO
    
    class YoloWorker:
        def __init__(self, model_path="yolov8n_ncnn_model", rpi_ip="192.168.111.112", rpi_port=5010):
            # 1. YOLO 모델 로드
            self.model = YOLO(model_path)
            print("[YOLO] NCNN 경량화 모델 로드 완료")
            
            # 2. [추가] 라즈베리파이4 송신용 UDP 소켓 설정
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.rpi_address = (rpi_ip, rpi_port)
            
            try:
                self.client_socket.connect(self.rpi_address)
                print(f"[YOLO] 라파4({rpi_ip}:{rpi_port}) 데이터 연결 성공")
            except Exception as e:
                print(f"[YOLO 네트워크 에러] 연결 실패 (로컬로만 실행): {e}")
                self.client_socket = None
                
            # 3. [추가] 상태 추적용 변수 (None, 0: 없음, 1: 있음)
            self.last_state = None 
    
        def process_frame(self, frame):
            """메인에서 프레임을 받아 객체 추론을 수행하고 이더넷 신호를 송신합니다."""
            results = self.model(frame, classes=0, verbose=False)
            person_count = 0
            
            if len(results) > 0:
                r = results[0]
                person_count = len(r.boxes)
                
                # 화면 드로잉 (바운딩 박스)
                for box in r.boxes:
                    x1, y1, x2, y2 = map(int, box.xyxy[0].tolist())
                    conf = float(box.conf[0])
                    cv2.rectangle(frame, (x1, y1), (x2, y2), (255, 0, 0), 2)
                    cv2.putText(frame, f"Person: {conf:.2f}", (x1, y1 - 10),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 2)
                    
            # 좌측 상단에 인원 수 표시
            cv2.putText(frame, f"Count: {person_count}", (10, 30), 
                        cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 255), 2)
            
            # ============================================================
            # [신규 추가] 사람이 감지 여부에 따른 이더넷 (UDP) 데이터 송신
            # ============================================================
            current_state = 1 if person_count > 0 else 0
            
            # 상태가 변했을 때만 패킷을 전송하여 네트워크 부하 최적화 (Edge Trigger)
            if current_state != self.last_state:
                if self.client_socket is not None:
                    try:
                        send_msg = f"{current_state}\n".encode('utf-8')
                        self.client_socket.sendto(send_msg, self.rpi_address)
                        print(f"[YOLO 네트워크] 상태 변경 전송 -> {current_state}")
                    except Exception as e:
                        print(f"[YOLO 전송 실패] 데이터 유실 발생: {e}")
                
                # 현재 상태를 과거 상태로 갱신
                self.last_state = current_state
                
            return frame
    
        def close(self):
            """자원 해제"""
            if self.client_socket:
                self.client_socket.close()
    ```
    
- 사람이 있으면 1, 없으면 0을 라즈베리파이4의 포트 5010번으로 전송
