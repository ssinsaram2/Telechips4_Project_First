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
    
    <img width="852" height="596" alt="image" src="https://github.com/user-attachments/assets/8af9b4cb-5eb5-4bb2-a150-1434200c8ed6" />
    
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
    
    <img width="688" height="273" alt="image" src="https://github.com/user-attachments/assets/adfd7f5d-f103-479c-8198-038e1de81a35" />
    
- 비디오 디바이스 파일 확인
    
    <img width="688" height="273" alt="image" src="https://github.com/user-attachments/assets/e7c3d9f2-a719-4e2c-b0ca-0adb3034c7c5" />
    
- `v4l-utils` 를 이용해서 디바이스 파일 찾기
    
    ```bash
    install v4l-utils
    v4l2-ctl --list-devices
    ```
    
    <img width="354" height="119" alt="image" src="https://github.com/user-attachments/assets/eb94e456-5472-41f0-84ed-979afd308848" />
    
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

<img width="654" height="588" alt="image" src="https://github.com/user-attachments/assets/eee14c49-add9-4cea-87f2-0265069e00b0" />

- 대략 0.3~0.6 초 정도의 인식 딜레이를 가짐

### 실행
    
- 식별된 사람을 1, 2, 3의 형태의 데이터로 라즈베리파이4의 프토 5000번으로 전송

---

## YOLOv8n을 이용한 객체(사람) 감지

### yolo 환경 세팅

#### 윈도우

- 파이썬 설치
- yolov8n을 ncnn으로 만들기
        
- yolov8n_ncnn_model 파일이 생성
    
    <img width="1408" height="740" alt="image" src="https://github.com/user-attachments/assets/57aa4365-ddcb-475a-9250-729df46a1bcc" />
    
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
        
        <img width="647" height="527" alt="image" src="https://github.com/user-attachments/assets/cde8cf95-4db4-4f5c-8643-5b2a3f314d4c" />
        
    - 인원 수 체크는 너무 변수가 많음
        
        <img width="1475" height="614" alt="image" src="https://github.com/user-attachments/assets/f88a1310-e166-4ca6-8eb0-08e0bea9606a" />
        
        - 손도 개별 객체로 판단해버림
    - 팔꿈치까지 다 보이면 하나로 인식
        
        <img width="1475" height="614" alt="image" src="https://github.com/user-attachments/assets/89870b5a-6768-4cd8-9d24-aaf6bd8b8347" />
        

#### 딜레이 측정

<img width="1513" height="574" alt="image" src="https://github.com/user-attachments/assets/e69789ad-209d-4b66-865c-0b4a61e36fea" />

- 0.15~0.3 초

### 실행
    
- 사람이 있으면 1, 없으면 0을 라즈베리파이4의 포트 5010번으로 전송
