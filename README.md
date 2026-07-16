# 개인화를 위한 얼굴인식(InsightFace), 잔류 감지를 위한 객체(사람) 인식(YOLOv8n) 

## 전체 동작
    
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
    ```
    
    ```
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
    
    <img width="852" height="596" alt="image" src="https://github.com/user-attachments/assets/8af9b4cb-5eb5-4bb2-a150-1434200c8ed6" />

    
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
