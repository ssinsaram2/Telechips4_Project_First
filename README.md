# School Bus Safety System - PR 기반 통합 수정본

이 패키지는 `School_Bus_Safety_Operations_System-feature-raspberrypi4-pr (2).zip`의 상태 흐름을 기준으로,
누락되어 있던 실제 통합 로직을 채운 버전입니다.

## 폴더 구성

```text
common/
  ipc_contract.h              # RPi4 Central <-> Qt UI IPC 번호 고정 계약

rpi4_central/
  src/central_app.c            # CAN 수신, 상태머신, 잔류감지, 30초 타이머, RPi5 제어
  scripts/
    send_child_alert_telegram.sh
    send_object_camera_event.py
    send_face_result.py

qt6_ui/
  main.cpp
  centralclient.*              # POSIX mqueue + 얼굴인식 결과 UDP 수신
  scenariowindow.*             # 시나리오 전용 Qt6 UI
```

## 반영된 시나리오

### 1. 전원/인증/시동

1. 시작 시 UI는 검은 화면입니다.
2. RTOS 전원 버튼 CAN `0x012` 1회 입력 시 RPi4 Central이 `IPC_MSG_SHOW_AUTH_SCREEN`을 보내고 UI는 얼굴인식 화면으로 전환합니다.
3. RPi4 Central은 RPi5로 UDP `FACE_ON` 명령을 보냅니다.
4. RPi5 얼굴인식 결과는 UI가 UDP 5000번 포트로 받습니다.
   - `1`, `2`, `3` 같은 양수: 등록 운전자 ID로 처리
   - `0`, `FAIL`, `UNKNOWN`: 미등록자로 처리하고 PIN 화면 표시
5. 등록자/PIN 성공 시 UI가 Central로 `IPC_MSG_FACE_AUTH_SUCCESS`를 보냅니다.
6. Central은 개인화 값을 FW로 전송하고 UI에 “전원 버튼을 한 번 더 누르면 시동 ON” 화면을 띄웁니다.
7. RTOS 전원 버튼 CAN `0x012` 2회째 입력 시 Central이 `0x083 [0x02, 0x01]`을 보내 시동 ON/주행 상태로 전환합니다.

### 2. 승하차/안전벨트

1. 주행 화면 진입 직후 좌석은 전부 회색입니다.
2. UI의 `승하차` 버튼을 누르면 승하차 모드입니다.
3. RTOS 안전벨트 CAN `0x032`:
   - `data[0] = 1번 좌석`, `data[1] = 2번 좌석`
   - `0x00 = 해제`, `0x01 = 체결`
4. 승하차 모드에서는 체결 좌석이 파란색, 해제 좌석이 회색입니다. 빨간 경고와 부저는 없습니다.
5. 승하차 버튼을 다시 눌러 주행 모드로 돌아오면, 그 시점에 체결되어 있던 좌석을 기준 좌석으로 저장합니다.
6. 주행 모드에서 기준 좌석의 벨트가 풀리면 해당 좌석은 빨간색이고 Central이 RTOS 부저 명령 `0x0C3`을 ON합니다.
7. 다시 체결하면 파란색으로 돌아가고 부저가 OFF됩니다.
8. 다시 승하차 모드에서 벨트를 풀면 회색으로 바뀌고 부저는 울리지 않습니다.

### 3. 잔류감지

1. 주행 상태에서 RTOS 전원 버튼 `0x012` 3회째 입력 시 Central은 시동 OFF/주차 상태로 전환합니다.
2. UI는 “잔류감지 모드로 전환”을 잠깐 보여준 뒤 검은 화면이 됩니다.
3. Central은 RPi5로 UDP `OBJECT_ON`을 보냅니다.
4. PIR `0x022` 또는 카메라 객체감지 UDP 5010 포트 중 하나라도 감지되면 1차 경고입니다.
5. 1차 경고 시 Central은 다음을 수행합니다.
   - UI 경고 화면 표시
   - 부저 `0x0C3` ON
   - 외부 LED `0x0A3` ON
   - 창문 `0x063` 목표값 30
   - 상태 `0x083 [0x00, 0x02]` 전송
6. UI의 `경고 해제` 버튼을 30초 이내에 누르면 경고가 꺼지고 검은 화면으로 돌아갑니다.
7. 30초 동안 해제하지 않으면 `CHILD_ALERT_LEVEL2_CMD` 환경변수에 설정한 명령을 실행합니다.
   기본 제공 스크립트는 텔레그램 전송용 `send_child_alert_telegram.sh`입니다.

## RPi4 Central 빌드

```sh
cd rpi4_central
cmake -S . -B build
cmake --build build -j
sudo install -m 0755 build/central_app /opt/central/central_app
```

실행 예:

```sh
export RPI5_AI_HOST=192.168.111.113
export RPI5_AI_CONTROL_PORT=5020
export OBJECT_CAMERA_PORT=5010
export CHILD_ALERT_ESCALATION_SEC=30
export CHILD_ALERT_LEVEL2_CMD="/opt/central/send_child_alert_telegram.sh"
sudo /opt/central/central_app
```

## Qt6 UI 빌드

```sh
cd qt6_ui
cmake -S . -B build
cmake --build build -j
./build/school_bus_ui
```

UI가 `central_app`을 자동 실행하게 하려면:

```sh
export CENTRAL_APP_PATH=/opt/central/central_app
export UI_START_CENTRAL=1
./build/school_bus_ui
```

이미 systemd로 `central_app`을 실행 중이면:

```sh
export UI_START_CENTRAL=0
./build/school_bus_ui
```

## 테스트 명령

### 얼굴인식 결과 테스트

UI가 실행 중인 RPi4로 등록자 ID 1을 보냅니다.

```sh
python3 rpi4_central/scripts/send_face_result.py <RPi4_IP> 1
```

미등록자 테스트:

```sh
python3 rpi4_central/scripts/send_face_result.py <RPi4_IP> FAIL
```

### 객체감지 테스트

```sh
python3 rpi4_central/scripts/send_object_camera_event.py <RPi4_IP> 1
```

객체 미감지:

```sh
python3 rpi4_central/scripts/send_object_camera_event.py <RPi4_IP> 0
```

### CAN 테스트

전원 버튼 1회:

```sh
cansend can0 012#01
```

전원 버튼 릴리즈:

```sh
cansend can0 012#00
```

안전벨트 1,2번 체결:

```sh
cansend can0 032#0101
```

2번만 해제:

```sh
cansend can0 032#0100
```

PIR 감지:

```sh
cansend can0 022#01
```

## RPi5 AI 제어 프로토콜

Central은 `RPI5_AI_HOST:RPI5_AI_CONTROL_PORT`로 UDP 문자열을 보냅니다.

| 명령 | 의미 |
|---|---|
| `FACE_ON` | 얼굴인식 모델 ON |
| `FACE_OFF` | 얼굴인식 모델 OFF |
| `OBJECT_ON` | 객체감지 모델 ON |
| `OBJECT_OFF` | 객체감지 모델 OFF |
| `ALL_OFF` | 모든 모델 OFF |

RPi5 쪽 제어 서버는 위 문자열을 받아서 모델을 켜고 끄면 됩니다.

## FW/RTOS 수정 여부

- FW는 이 패키지 기준으로 수정하지 않았습니다.
- RTOS도 필수 수정 없이 동작하도록 RPi4가 기존 RTOS 명령 CAN ID(`0x063`, `0x0A3`, `0x0C3`)를 직접 사용합니다.
- 카메라 객체감지 결과를 RTOS도 별도로 알고 싶으면 `0x0D3` 수신 처리를 RTOS에 추가하면 됩니다. 이 패키지는 RPi4 직접 명령 경로를 기본으로 합니다.

## 주의

Qt UI와 Central은 반드시 같은 `common/ipc_contract.h` 번호를 사용해야 합니다.
기존 UI 코드와 섞어서 enum 번호가 바뀌면 “CANdump에는 찍히는데 UI가 안 바뀌는” 문제가 다시 생깁니다.
