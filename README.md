1. **데이터 수집**: 최적화된 카메라 테스트 모듈을 통해 영상 스트림을 확보합니다.
2. **핵심 비즈니스 로직**: `bus_detect` 모듈이 시동 전/중/후 시나리오에 맞춰 상태 기반 제어(State Machine)를 수행합니다.
3. **독립 기능 검증**: 핵심 로직의 고도화를 위해 인공지능 프레임워크별로 격리된(Isolated) 테스트 환경을 운영합니다.

---

## 📂 Directory Structure & Modules

### 🚀 Core Application

#### 📂 `bus_detect/`
* **설명**: `YOLOv8n`과 `InsightFace`를 결합하여 버스 운행 프로세스 전반을 제어하는 핵심 메인 모듈입니다.
* **주요 기능**:
  * **시동 전**: 운전자 신원 확인
  * **시동 중**: 탑승 어린이 인원 수 확인
  * **시동 후**: 차량 내 잔류 어린이 확인

---

### 🧪 Experimental & Test Modules

#### 📂 `camera_test/`
* **설명**: 영상 입력 장치(하드웨어)의 정상 작동 여부를 확인하기 위한 테스트용 코드입니다.

#### 📂 `insight_project/`
* **설명**: insight face를 테스트하는 코드입니다.

#### 📂 `mediapipe_project/`
* **설명**: Google MediaPipe를 테스트하는 코드입니다.

#### 📂 `yolodetect/`
* **설명**: Ultralytics YOLO 모델을 사용해 잔류 어린이를 탐지하는 경량화 모델 테스트용 코드입니다.

* **InsightFace**
  * 구글 검색어: `InsightFace GitHub Documentation`
  * 링크: [InsightFace GitHub](https://github.com/deepinsight/insightface)
* **Google MediaPipe**
  * 구글 검색어: `Google MediaPipe Solutions Guide`
  * 링크: [MediaPipe 공식 문서](https://ai.google.dev/edge/mediapipe/solutions/guide)
