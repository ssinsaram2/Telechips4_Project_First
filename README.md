🚀 Raspberry Pi 5 Central Control AI System
라즈베리파이 5(RPi5)에서 구동되는 멀티 스레드 기반 중앙 제어 AI 시스템입니다.

본 시스템은 라즈베리파이 4(RPi4)로부터 UDP 명령을 수신하여 실시간으로 동작 모드를 전환하며, 카메라 스트림을 분할하여 인공지능 분석(얼굴 인식, 객체 감지)과 RTSP 스트리밍을 동시에 처리하는 에지 디바이스(Edge Device) 파이프라인을 구축합니다.

🗺️ 1. System Architecture (시스템 아키텍처)
전체 시스템은 [입력 및 스트림 분할] ➡️ [중앙 모드 제어] ➡️ [AI 추론 및 결과 전송]의 구조를 가집니다. 단일 웹캠 소스를 논리적으로 복제하여 자원 충돌을 방지하고, 독립적인 멀티스레드로 각 역할을 수행합니다.

             ┌──────────────────────┐
             │   라즈베리파이 4      │
             └──────────┬───────────┘
                        │ (UDP 5020: Control Command 0, 1, 2)
                        ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 라즈베리파이 5 (Ubuntu Desktop 22.04 LTS)                              │
│                                                                        │
│  [Hardware Camera] -> /dev/video0                                      │
│                           │ (v4l2loopback & ffmpeg 복제)                │
│                           ├──> /dev/video10 (RTSP Stream Source)       │
│                           └──> /dev/video11 (AI Main Python - cv2)     │
│                                                                        │
│  [Network Thread] ──────────────────────────┐                          │
│     └─ UDP Listener (Port 5020)              │                          │
│                                              ▼ Mode Switch (0, 1, 2)    │
│  [Main Inference Thread] ───────────► [Central Control]                │
│     ├─ Mode 0: AI Disabled (Bypass)                                    │
│     ├─ Mode 1: InsightFace ──────────► RPi4 IP:5000 (SJH=1, SHG=2)     │
│     └─ Mode 2: YOLOv8n (NCNN) ───────► RPi4 IP:5010 (Detect=1, None=0) │
└────────────────────────────────────────────────────────────────────────┘
🔄 데이터 흐름 및 상태 제어 시나리오
명령 수신: main.py 내 network_receiver_thread가 UDP 5020 포트를 상시 리스닝하며 대기합니다.

모드 스위칭 (Thread-safe): mode_lock을 통해 스레드 간 동기화를 유지하며 CURRENT_MODE를 안전하게 변경합니다.

분기 처리:

Mode 0: AI 연산을 수행하지 않는 일반 패스스루(Pass-through) 모드입니다.

Mode 1 (InsightFace): 얼굴 식별을 수행하며, 지정된 인물 검출 시 해당 ID 매핑 데이터를 RPi4(Port 5000)로 송신합니다.

Mode 2 (YOLOv8n): NCNN 경량화 모델을 통해 사람(Person, class 0)을 감지합니다. 에지 트리거(Edge Trigger) 방식을 적용하여 상태가 변할 때만 결과값(1/0)을 RPi4(Port 5010)로 송신합니다.

🛠️ 2. Environment Setup (환경 설정 및 종속성)
OS 빌드 정보
OS: Ubuntu Desktop 22.04 LTS (ARM64 Target)

선정 이유: 라즈비안(Raspbian) 대비 한글 로캘 세팅, GUI 디버깅, 입력 디바이스 인터페이스 제어 및 외부 C++ 빌드 의존성 해결이 용이하여 실무 현업 적용에 적합한 빌드 타깃으로 선정되었습니다.

VS Code 및 필수 유틸리티 설치
Bash
# 패키지 업데이트 및 시스템 라이브러리 설치
sudo apt update && sudo apt upgrade -y
sudo apt install curl v4l-utils ffmpeg build-essential python3-dev python3-venv cmake libgl1 libglib2.0-0 -y

# VS Code ARM64 deb 패키지 다운로드 및 설치
curl -L https://aka.ms/linux-arm64-deb -o code_arm64.deb
sudo apt install ./code_arm64.deb -y
📹 3. Hardware Camera Demultiplexing (V4L2 Loopback)
단일 하드웨어 웹캠 소스(/dev/video0)로 RTSP 전송과 AI Inference를 동시에 실행하면 디바이스 파일 독점 에러(Device or resource busy)가 발생합니다. 이를 해결하기 위해 커널 모듈 v4l2loopback을 사용하여 논리적으로 가상 디바이스 분할(Demux)을 진행합니다.

1) 가상 비디오 디바이스 생성 및 스트림 복제 (Terminal 1)
Bash
# 기존 루프백 모듈 언로드 및 가상 인터페이스 2개 활성화
sudo modprobe -r v4l2loopback
sudo modprobe v4l2loopback devices=2 video_nr=10,11 card_label="Cam10,Cam11" exclusive_caps=1

# 물리 카메라 스트림을 가상 인터페이스(/dev/video10, /dev/video11)로 실시간 복제
ffmpeg -f v4l2 -i /dev/video0 \
  -map 0:v -codec copy -f v4l2 /dev/video10 \
  -map 0:v -codec copy -f v4l2 /dev/video11
2) 가상 채널별 역할 배분
/dev/video10 (RTSP 전용 채널): 네트워크 송출 성능 확보를 위해 GStreamer 파이프라인 및 H.264 하드웨어 가속을 활용합니다.

Bash
gst-launch-1.0 v4l2src device=/dev/video10 ! video/x-raw,width=640,height=480,framerate=30/1 ! \
  videoconvert ! x264enc tune=zerolatency bitrate=2000 speed-preset=ultrafast key-int-max=30 ! \
  rtspclientsink location=rtsp://192.168.111.112:8554/test protocols=tcp
/dev/video11 (AI Framework 채널): OpenCV VideoCapture 스레드가 디바이스 인덱스 11을 수신하여 캡처를 수행합니다.

🧑‍🤝‍🧑 4. AI Engine Details (인공지능 모델 세부 정보)
1) InsightFace (Mode 1 - 얼굴 식별)
환경 구축:

Bash
# 가상환경 구성 및 패키지 설치
cd ~/rp5/AI/insight_project
python3 -m venv insight_env
source insight_env/bin/activate
pip install --upgrade pip setuptools wheel
pip install insightface
임베딩 알고리즘: faces/ 디렉터리에 타깃 인물의 원본 이미지(.jpg, .png)를 저장해두면, 앱 로드 시점에 추론을 실행하여 고유 얼굴 임베딩 벡터(normed_embedding)를 추출하고 메모리에 캐싱합니다.

유사도 매핑: 카메라 입력 실시간 프레임과 사전에 저장된 임베딩 벡터 간의 내적 연산(Dot Product)을 통한 코사인 유사도를 계산하여 유효 인물을 식별합니다. (기준 임계값 Threshold: 0.45)

지연 시간(Latency): ARM64 CPU 기반 환경에서 약 0.3s ~ 0.6s 수준의 실시간 반응 속도를 보장합니다.

2) YOLOv8n NCNN (Mode 2 - 객체 감지)
NCNN 모델 컴파일 및 내보내기 (Windows / Desktop PC 환경 권장):
PyTorch 포맷(.pt)을 에지 기기에 최적화된 C++ 친화적 NCNN 구조로 포팅하여 실행 파일의 크기와 연산 가벼움을 극대화합니다.

Python
from ultralytics import YOLO
model = YOLO("yolov8n.pt")
model.export(format="ncnn") # 'yolov8n_ncnn_model' 디렉터리 빌드 완료
라즈베리파이 가속 환경 패키지 구성:

Bash
pip install ultralytics --break-system-packages
pip install ncnn --break-system-packages
네트워크 자원 최적화 (Edge Trigger): 지속적인 소켓 I/O 부하 및 대역폭 낭비를 예방하기 위해, 매 프레임 결과를 전송하지 않고 감지 상태의 변화가 생길 때(검출 유무 0 ↔ 1)에만 이더넷 패킷을 송출합니다.

지연 시간(Latency): NCNN 포팅 및 가속화 작업을 통해 최적화되어 단 0.15s ~ 0.3s 내외의 초고속 추론 사이클 속도로 연산이 수행됩니다.

🏃 5. How to Run (메인 구동 제어)
가상환경 로드 및 메인 어플리케이션 실행 (Terminal 2)
Bash
# 디렉터리 이동 및 가상환경 활성화
cd ~/rp5/AI/firstproject/
source insight_env/bin/activate

# 메인 프로세서 데몬 스레드 구동
python3 main.py
