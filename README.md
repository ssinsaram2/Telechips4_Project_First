# 어린이 통학차량 안전관리 시스템

Qt6 UI, Raspberry Pi 4 Central, RTOS/F/W CAN 노드, Raspberry Pi 5 AI 모듈을 연동하는 어린이 통학차량 안전관리 시스템 UI 및 코드 통합본입니다.

RPi4 Central이 시스템 상태를 판단하는 중심 프로세스입니다. Central은 SocketCAN으로 RTOS/F/W와 통신하고, POSIX message queue로 Qt6 UI를 제어하며, UDP로 RPi5 AI 모델 선택과 객체 감지 결과를 주고받습니다.

## 주요 기능

- 전원 버튼 기반 상태 전환: OFF -> 운전자 인증 -> 시동 대기 -> 주행 -> 시동 OFF/잔류 감지
- 운전자 얼굴 인증 및 PIN 대체 인증
- 운전자별 좌석/온도 개인화 프로필 적용
- 승하차 모드와 주행 모드의 안전벨트 표시/경고 분리
- PIR 센서와 RPi5 객체 감지 기반 잔류 아동 감지
- 1차 운전자 알림, 2차 경찰 알림 Telegram 전송
- 환경 센서, 미세먼지, F/W 상태 UI 표시
- CAN 재연결, IPC 큐 초기화, 중복 실행 방지 PID 파일 처리

## 프로젝트 구조

```text
.
|-- CMakeLists.txt
|-- common/
|   `-- ipc_contract.h                  # Central <-> Qt UI IPC 메시지 번호 계약
|-- driver_profile.json                 # 운전자 개인화 프로파일 예시
|-- qt6_ui/
|   |-- main.cpp                        # Qt 앱 진입점, central_app 자동 실행 옵션
|   |-- centralclient.*                 # POSIX mqueue, 얼굴 인증 결과 UDP 수신
|   |-- faceauthscreen.*                # 얼굴 인증/PIN 진입 화면
|   |-- scenariowindow.*                # 전체 시나리오 화면 전환, Telegram PIN/1차 알림
|   `-- mainwindow.*                    # 주행/승하차 화면, 좌석/센서/FW 상태 표시
|-- rpi4_central/
|   |-- src/central_app.c               # CAN, 상태머신, RPi5 제어, 잔류감지, IPC
|   `-- scripts/
|       |-- send_child_alert_telegram.sh
|       |-- send_face_result.py
|       `-- send_object_camera_event.py
`-- scripts/
    |-- can_setup.sh
    |-- start_system.sh
    |-- stop_system.sh
    |-- clean_runtime.sh
    `-- reset_ipc.sh
```

## 하드웨어/네트워크 구성

| 구성 요소 | 역할 | 연결/프로토콜 |
| --- | --- | --- |
| Raspberry Pi 4 | Central 제어기, Qt6 UI 실행 | SocketCAN, POSIX mqueue, UDP |
| RTOS 보드 | 전원 버튼, PIR, 안전벨트, 부저/LED/창문 제어 | CAN 500 kbit/s |
| F/W 보드 | 환경/미세먼지 센서, 좌석/공조/내부등 제어 | CAN 500 kbit/s |
| Raspberry Pi 5 | 얼굴 인증 및 잔류 아동 객체 감지 AI | UDP |
| Telegram Bot API | PIN, 1차/2차 경고 알림 | HTTPS |

기본 네트워크 포트:

| 포트 | 수신 주체 | 용도 |
| --- | --- | --- |
| UDP 5000 | Qt UI | RPi5 얼굴 인증 결과 수신 |
| UDP 5010 | RPi4 Central | RPi5 객체 감지 결과 수신 |
| UDP 5020 | RPi5 | Central의 AI 모델 선택 명령 수신 |

현재 Central의 RPi5 제어 대상은 `192.168.111.111:5020`으로 코드에 고정되어 있습니다. 변경이 필요하면 `rpi4_central/src/central_app.c`의 `rpi5_send()`를 수정한 뒤 재빌드합니다.

## 동작 흐름

### 1. 전원, 인증, 시동

1. 초기 상태는 검은 화면입니다. Central은 `0x083 [0x00, 0x00]`을 송신해 시동 OFF/주차 상태를 알립니다.
2. RTOS 전원 버튼 CAN `0x012`의 rising edge가 들어오면 Central은 운전자 인증 상태로 전환합니다.
3. Central은 Qt UI에 `IPC_MSG_SHOW_AUTH_SCREEN`을 보내고 RPi5에 AI 모델 `1`을 UDP로 전송합니다.
4. Qt UI는 얼굴 인증 결과를 UDP 5000번 포트에서 수신합니다. driverId payload는 등록 운전자 인증 성공으로 처리됩니다.
5. 얼굴 인증 성공 시 UI는 `IPC_MSG_FACE_AUTH_SUCCESS`와 driverId를 Central로 보냅니다.
6. 얼굴 인증 화면에서 10초 동안 성공 결과가 없으면 PIN 입력 버튼이 표시됩니다. PIN은 Telegram으로 전송되는 6자리 코드이며, 3회 실패하면 인증 차단 화면으로 전환됩니다.
7. PIN 인증 성공 시 Central은 공용 driverId `0` 프로필을 사용합니다.
8. 인증 성공 후 Central은 `driver_profile.json`에서 운전자 프로파일을 읽고 좌석/온도 CAN 명령을 보낸 뒤 UI를 시동 대기 화면으로 전환합니다.
9. 전원 버튼 `0x012`가 다시 입력되면 Central은 주행 상태로 전환하고 `0x083 [0x02, 0x01]`을 송신합니다.

주의: 현재 UI의 얼굴 결과 UDP 처리에서는 `FAIL`, `UNKNOWN`, `0` payload를 인증 성공으로 처리하지 않지만, 즉시 실패 이벤트를 발생시키지는 않습니다. PIN 진입은 10초 초과 후 화면의 PIN 입력 버튼으로 진행됩니다.

### 2. 승하차와 안전벨트

현재 Central의 안전벨트 판단 활성 좌석은 1번과 2번입니다. UI는 8개 좌석을 표시하지만 CAN 입력으로 실제 갱신되는 좌석은 1번 좌석과 2번 좌석으로 CAN ID는 `0x032 data[0]`, `data[1]`입니다.

1. 주행 화면 진입 직후 좌석은 모두 회색입니다.
2. UI의 `승하차` 버튼을 누르면 Central은 `STATE_PASSENGER_TRANSFER`로 전환하고 `0x083 [0x02, 0x04]`를 송신합니다.
3. 승하차 모드에서는 안전벨트 체결 좌석을 파란색, 해제 좌석을 회색으로 표시합니다. 이 모드에서는 부저 경고를 울리지 않습니다.
4. `승하차` 버튼을 다시 누르면 주행 모드로 복귀하며, 이 시점에 체결되어 있던 좌석을 기준 좌석으로 저장합니다.
5. 주행 모드에서 기준 좌석의 벨트가 풀리면 해당 좌석이 빨간색으로 표시되고 부저 `0x0C3`이 ON 됩니다.
6. 다시 체결하면 파란색으로 돌아가고, 다른 경보가 없으면 부저가 OFF 됩니다.

### 3. 잔류 아동 감지

1. 주행 상태에서 전원 버튼 `0x012`가 입력되면 Central은 시동 OFF/주차 상태로 전환합니다.
2. UI는 잠시 `잔류감지 모드로 전환` 문구를 보여준 뒤 검은 화면으로 돌아갑니다.
3. Central은 RPi5에 AI 모델 `2`를 UDP로 보내 객체 감지 모드를 켭니다.
4. PIR CAN `0x022` 또는 RPi5 객체 감지 UDP 5010 포트에서 감지가 들어오면 1차 경고가 시작됩니다.
5. 1차 경고 시 Central은 다음 명령을 수행합니다.
   - UI 경고 화면 표시
   - 운전자 Telegram 알림 1회 전송
   - 부저 `0x0C3` ON
   - 외부 LED `0x0A3` ON
   - 창문 `0x063` 목표값 30
   - 상태 `0x083 [0x00, 0x02]` 송신
6. `CHILD_ALERT_ESCALATION_SEC` 동안 확인이 없으면 2차 경고로 경찰 Telegram 알림을 1회 전송합니다. 기본값은 30초입니다.
7. UI의 `차량 확인 완료` 버튼 또는 PIR CAN `0x022 data[0]=2`가 들어오면 현재 경고 사건만 종료합니다.
8. 확인 후에도 잔류 감시는 계속 유지됩니다. 기본 30초 쿨다운과 센서 clear 관측이 모두 끝난 뒤 재감지되면 새 1차 경고부터 다시 시작합니다.
9. 잔류 감시 세션은 내부 타이머로 자동 종료되지 않습니다. 다음 운행 인증/주행 사이클이 시작될 때 종료됩니다.

## 빌드

### 요구 환경

- Linux 또는 Raspberry Pi OS 계열 (프로젝트에서는 Yocto Scarthgap을 사용하였습니다.)
- CMake 3.16 이상
- C/C++ 컴파일러
- Qt6 Core, Network, Widgets (Version 6.9.3)
- POSIX message queue 지원
- SocketCAN 및 `can-utils`
- Telegram 전송용 `curl`

Ubuntu/Raspberry Pi OS 예시:

```sh
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev can-utils curl
```

### 통합 빌드

루트 CMake는 `rpi4_central`과 `qt6_ui`를 함께 빌드합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

빌드 산출물:

```text
build/rpi4_central/central_app
build/qt6_ui/school_bus_ui
```

### 배포 예시

`scripts/start_system.sh`의 기본 경로는 `/opt/central/central_app`, `/opt/central/school_bus_ui`입니다.

```sh
sudo mkdir -p /opt/central
sudo install -m 0755 build/rpi4_central/central_app /opt/central/central_app
sudo install -m 0755 build/qt6_ui/school_bus_ui /opt/central/school_bus_ui
sudo install -m 0755 rpi4_central/scripts/send_child_alert_telegram.sh /opt/central/send_child_alert_telegram.sh
sudo install -m 0644 driver_profile.json /opt/central/driver_profile.json
```

## 설정 파일

### 운전자 프로파일

기본 파일명은 `driver_profile.json`입니다. `DRIVER_PROFILE_PATH`를 지정하면 해당 파일만 사용합니다. 미지정 시 Central은 다음 순서로 찾습니다.

```text
./driver_profile.json
driver_profile.json
/root/driver_profile.json
/opt/central/driver_profile.json
```

스키마:

```json
{
  "drivers": [
    {
      "driverId": 1,
      "seatPosition": 1000,
      "temperature": 24
    }
  ]
}
```

- `driverId`: 얼굴 인증 결과 driverId와 매칭
- `seatPosition`: 좌석 이동 시간(ms), 허용 범위 0~5000
- `temperature`: 목표 실내 온도, 허용 범위 16~30
- 프로파일 파일이 없거나 매칭 실패 시 기본값은 seatPosition `1000`, temperature `25`

### Telegram

실제 토큰은 README나 커밋에 넣지 마십시오. 이 저장소의 `.gitignore`는 `telegram.txt`, `police.txt`를 로컬 설정 파일로 취급합니다.

운전자/PIN/1차 알림용 `telegram.txt`:

```text
GROUP_ID='<driver_or_group_chat_id>'
API_TOKEN='<telegram_bot_token>'
```

경찰 2차 알림용 `police.txt`:

```text
CHAT_ID='<police_or_group_chat_id>'
API_TOKEN='<telegram_bot_token>'
```

환경 변수로도 설정할 수 있습니다. 하지만 위의 방법을 권장합니다.

```sh
export TELEGRAM_BOT_TOKEN='<telegram_bot_token>'
export TELEGRAM_CHAT_ID_DRIVER='<driver_chat_id>'
export TELEGRAM_CHAT_ID_POLICE='<police_chat_id>'
```

2차 알림 기본 문구:

```text
아동용 승하차 차량에 잔류 아동이 감지되었습니다! 해당 차량의 위치로 출동하십시오!
```

## 실행

### CAN 설정

기본 CAN 인터페이스는 `can0`, 기본 비트레이트는 500 kbit/s입니다.

```sh
sudo scripts/can_setup.sh
```

변경이 필요하면 환경변수를 사용합니다.

```sh
sudo CAN_INTERFACE=can0 CAN_BITRATE=500000 scripts/can_setup.sh
```

### 전체 실행

```sh
export CENTRAL_APP_PATH=/opt/central/central_app
export UI_APP_PATH=/opt/central/school_bus_ui
export DRIVER_PROFILE_PATH=/opt/central/driver_profile.json
export TELEGRAM_CONFIG=/opt/central/telegram.txt
export POLICE_TELEGRAM_CONFIG=/opt/central/police.txt
export CHILD_ALERT_LEVEL2_CMD="/opt/central/send_child_alert_telegram.sh police"
export CHILD_ALERT_ESCALATION_SEC=30
export CHILD_ACK_COOLDOWN_SEC=30

sudo -E scripts/start_system.sh
```

1차 운전자 Telegram 알림은 UI가 `telegram.txt`로 전송합니다. Central 경로로 1차 알림을 별도 전송하려면 `CHILD_ALERT_LEVEL1_CMD="/opt/central/send_child_alert_telegram.sh driver"`를 추가할 수 있지만, UI 알림과 중복될 수 있습니다.

`start_system.sh`는 다음 순서로 실행합니다.

1. `can0` 설정
2. 런타임 정리
3. `central_app` 백그라운드 실행
4. `school_bus_ui` 백그라운드 실행

로그 기본 경로:

```text
/tmp/school_bus_logs/central.log
/tmp/school_bus_logs/ui.log
```

### 수동 실행

Central:

```sh
sudo -E build/rpi4_central/central_app
```

UI:

```sh
UI_START_CENTRAL=0 build/qt6_ui/school_bus_ui
```

UI가 Central을 자동 실행하게 하려면 `UI_START_CENTRAL`을 생략하거나 `1`로 둡니다.

```sh
export CENTRAL_APP_PATH=/opt/central/central_app
build/qt6_ui/school_bus_ui
```

### 종료와 초기화

```sh
scripts/stop_system.sh
scripts/clean_runtime.sh
```

`clean_runtime.sh`는 프로세스를 종료하고 POSIX message queue와 PID 파일을 정리합니다.

### 자동 실행(systemd) 예시

운영 장비에서 부팅 후 자동 실행이 필요하면 systemd unit으로 분리해 등록할 수 있습니다. 아래 예시는 `/opt/central`에 배포가 끝났고, RPi 데스크톱 세션에서 Qt UI를 띄우는 구성을 가정합니다. 사용자명, 디스플레이 서버, 설정 파일 경로는 장비에 맞게 조정해야 합니다.

Central 서비스 예시:

```ini
[Unit]
Description=School Bus RPi4 Central
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStartPre=/opt/central/can_setup.sh
ExecStart=/opt/central/central_app
WorkingDirectory=/opt/central
Environment=DRIVER_PROFILE_PATH=/opt/central/driver_profile.json
Environment=TELEGRAM_CONFIG=/opt/central/telegram.txt
Environment=POLICE_TELEGRAM_CONFIG=/opt/central/police.txt
Environment="CHILD_ALERT_LEVEL2_CMD=/opt/central/send_child_alert_telegram.sh police"
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Qt UI 서비스 예시:

```ini
[Unit]
Description=School Bus Qt6 UI
After=graphical.target school-bus-central.service
Requires=school-bus-central.service

[Service]
Type=simple
User=pi
WorkingDirectory=/opt/central
Environment=DISPLAY=:0
Environment=UI_START_CENTRAL=0
ExecStart=/opt/central/school_bus_ui
Restart=on-failure

[Install]
WantedBy=graphical.target
```

등록 예시:

```sh
sudo install -m 0755 scripts/can_setup.sh /opt/central/can_setup.sh
sudo systemctl daemon-reload
sudo systemctl enable school-bus-central.service
sudo systemctl enable school-bus-ui.service
sudo systemctl start school-bus-central.service
sudo systemctl start school-bus-ui.service
```

Qt UI가 화면을 열지 못하면 `DISPLAY`, `XAUTHORITY`, `QT_QPA_PLATFORM` 설정과 데스크톱 자동 로그인 상태를 먼저 확인합니다.

## RPi5 AI 연동

현재 Central은 `rpi4_central/src/central_app.c`의 `rpi5_send()`에 지정된 주소로 UDP payload를 전송합니다.

```text
host: 192.168.111.111
port: 5020
```

RPi5 쪽 수신 서버는 문자열이 아니라 숫자 payload를 처리해야 합니다.

| Payload | 의미 |
| --- | --- |
| `0` | AI 모델 미사용 |
| `1` | 얼굴 인증 모델 |
| `2` | 잔류 아동 객체 감지 모델 |

객체 감지 결과는 RPi5가 RPi4 Central의 UDP 5010 포트로 보냅니다. Central은 다음 payload를 감지로 처리합니다.

```text
1
true
PERSON=1
detected=true
{"person":1}
```

`0`, `false`, `PERSON=0`, `detected=false` 등은 미감지로 처리합니다. 양수 숫자 토큰이 있으면 감지로 판단합니다.

## IPC 계약

Qt UI와 Central은 `common/ipc_contract.h`의 `IpcMessage` 구조체와 enum 값을 공유합니다. 메시지큐 이름 기본값은 다음과 같습니다.

```text
Central -> Qt UI: /central_to_qt
Qt UI -> Central: /qt_to_central
```

환경변수:

```text
CENTRAL_TO_QT_QUEUE
QT_TO_CENTRAL_QUEUE
```

주요 IPC:

| 방향 | 메시지 | 용도 |
| --- | --- | --- |
| UI -> Central | `IPC_MSG_BOARDING_BUTTON` | 승하차/주행 모드 토글 |
| UI -> Central | `IPC_MSG_FACE_AUTH_SUCCESS` | 얼굴 인증 성공 driverId 전달 |
| UI -> Central | `IPC_MSG_DRIVER_ID_RECEIVED` | PIN 인증 성공 |
| UI -> Central | `IPC_MSG_FACE_AUTH_FAIL` | 인증 실패 화면 요청 |
| UI -> Central | `IPC_MSG_ALARM_ACK` | 잔류 경고 확인 완료 |
| Central -> UI | `IPC_MSG_SHOW_AUTH_SCREEN` | 얼굴 인증 화면 표시 |
| Central -> UI | `IPC_MSG_SHOW_READY_SCREEN` | 시동 대기 화면 표시 |
| Central -> UI | `IPC_MSG_SHOW_OPERATION_SCREEN` | 주행 화면 표시 |
| Central -> UI | `IPC_MSG_SHOW_BOARDING_SCREEN` | 승하차 화면 표시 |
| Central -> UI | `IPC_MSG_SCREEN_OFF` | 검은 화면 표시 |
| Central -> UI | `IPC_MSG_SEATBELT_UPDATE` | 좌석 색상 bitmap 전달 |
| Central -> UI | `IPC_MSG_ENV_UPDATE` | 온습도/조도 표시 |
| Central -> UI | `IPC_MSG_DUST_UPDATE` | 미세먼지 표시 |
| Central -> UI | `IPC_MSG_FW_STATUS_UPDATE` | F/W 팬/LED/상태 표시 |
| Central -> UI | `IPC_MSG_CHILD_DETECTED_WARNING` | 잔류 아동 경고 화면 표시 |
| Central -> UI | `IPC_MSG_PROFILE_APPLIED` | 개인화 적용 표시 |

주의: enum 숫자 값은 raw integer로 송수신됩니다. Central과 Qt UI가 서로 다른 `ipc_contract.h`를 사용하면 CAN 로그는 정상이어도 UI가 바뀌지 않습니다.

## CAN 계약

### Central 수신

| CAN ID | 방향 | 데이터 | 의미 |
| --- | --- | --- | --- |
| `0x012` | RTOS -> Central | `data[0]=0/1` | 전원 버튼 release/press. Central은 rising edge만 처리 |
| `0x022` | RTOS -> Central | `data[0]=0/1/2` | PIR 미감지/감지/경고 확인 |
| `0x032` | RTOS -> Central | `data[0]`, `data[1]` | 1번/2번 좌석 안전벨트 상태 |
| `0x041` | F/W -> Central | tempX10, humidityX10, luxX10, valid | 환경 센서 |
| `0x051` | F/W -> Central | PM1.0, PM2.5, PM10, valid | 미세먼지 |
| `0x061` | F/W -> Central | fan, led, ignition, operation, pir, valid, alert | F/W 상태 |

### Central 송신

| CAN ID | 방향 | 의미 |
| --- | --- | --- |
| `0x063` | Central -> RTOS/F/W | 창문 목표값 |
| `0x073` | Central -> F/W | 목표 온도와 팬 AUTO |
| `0x083` | Central -> RTOS/F/W | 시동/운행 상태 |
| `0x093` | Central -> F/W | 좌석 기준 이동/개인화 이동/정지 |
| `0x0A3` | Central -> RTOS | 외부 LED ON/OFF |
| `0x0B3` | Central -> F/W | 내부등 AUTO |
| `0x0C3` | Central -> RTOS | 부저 ON/OFF |
| `0x0D3` | Central -> RTOS/F/W | 카메라 객체 감지 결과 |

`0x083` 데이터 규약:

```text
data[0] ignition
  0 = OFF
  1 = DISPLAY
  2 = ON

data[1] operation
  0 = PARKING
  1 = RUNNING
  2 = ALARM
  4 = BOARDING
```

## 테스트 명령

아래 예시는 RPi4에서 `central_app`, Qt UI, `can0`가 실행 중인 상태를 기준으로 합니다.

### 전원 버튼

```sh
cansend can0 012#01
cansend can0 012#00
```

### 얼굴 인증 성공

UI가 실행 중인 장비로 driverId `1`을 보냅니다.

```sh
python3 rpi4_central/scripts/send_face_result.py <RPi4_IP> 1
```

PIN 대체 인증은 얼굴 인증 화면에서 10초 대기 후 PIN 입력 버튼으로 진입합니다.

### 안전 벨트

1번, 2번 좌석 체결:

```sh
cansend can0 032#0101
```

1번 체결, 2번 해제:

```sh
cansend can0 032#0100
```

모두 해제:

```sh
cansend can0 032#0000
```

### 환경 센서

25.0도, 습도 45.0%, 조도 800.0 lux, valid=1:

```sh
cansend can0 041#FA00C201401F01
```

### 미세먼지

PM1.0=10, PM2.5=25, PM10=40, valid=1:

```sh
cansend can0 051#0A001900280001
```

### F/W 상태

```sh
cansend can0 061#02010201010101
```

### 잔류 감지

PIR 감지:

```sh
cansend can0 022#01
```

PIR 경고 확인:

```sh
cansend can0 022#02
```

RPi5 객체 감지 이벤트:

```sh
python3 rpi4_central/scripts/send_object_camera_event.py <RPi4_IP> 1
```

객체 미감지 이벤트:

```sh
python3 rpi4_central/scripts/send_object_camera_event.py <RPi4_IP> 0
```

### 2차 경고 빠른 테스트

실행 전에 에스컬레이션 시간을 짧게 설정합니다.

```sh
export CHILD_ALERT_ESCALATION_SEC=5
sudo -E scripts/start_system.sh
```

## 시연 절차

아래 절차는 실제 장비 시연에서 전체 기능을 한 번에 확인하는 순서입니다. 시작 전 `central_app`, `school_bus_ui`, `can0`, Telegram 설정, RPi5 UDP 서버가 준비되어 있어야 합니다.

### A. 전원 및 운전자 인증

1. 시스템 시작 후 UI가 검은 화면인지 확인합니다.
2. 전원 버튼 입력을 보냅니다.

```sh
cansend can0 012#01
cansend can0 012#00
```

3. UI가 얼굴 인증 화면으로 전환되고, RPi5 로그에서 AI 모델 `1` 수신을 확인합니다.
4. 얼굴 인증 성공을 시뮬레이션합니다.

```sh
python3 rpi4_central/scripts/send_face_result.py <RPi4_IP> 1
```

5. UI가 운전자 인증 완료/시동 대기 화면으로 바뀌고, Central 로그에 프로파일 적용 로그가 출력되는지 확인합니다.
6. 전원 버튼을 다시 입력합니다.

```sh
cansend can0 012#01
cansend can0 012#00
```

7. UI가 주행 화면으로 전환되고 Central이 `0x083 [0x02, 0x01]`을 송신하는지 확인합니다.

### B. 승하차 및 안전벨트 경고

1. UI의 `승하차` 버튼을 눌러 승하차 모드로 전환합니다.
2. 1번, 2번 좌석 안전벨트 체결을 보냅니다.

```sh
cansend can0 032#0101
```

3. 두 좌석이 파란색으로 표시되는지 확인합니다.
4. UI의 `승하차` 버튼을 다시 눌러 주행 모드로 복귀합니다. 이때 현재 체결 좌석이 기준 좌석으로 저장됩니다.
5. 2번 좌석 벨트 해제를 보냅니다.

```sh
cansend can0 032#0100
```

6. 2번 좌석이 빨간색으로 바뀌고 부저 `0x0C3` ON 명령이 나가는지 확인합니다.
7. 다시 체결합니다.

```sh
cansend can0 032#0101
```

8. 경고가 해제되고 부저가 OFF 되는지 확인합니다.

### C. 잔류 아동 감지

1. 주행 상태에서 전원 버튼을 입력해 시동 OFF/잔류 감지 상태로 전환합니다.

```sh
cansend can0 012#01
cansend can0 012#00
```

2. UI에 `잔류 감지 모드로 전환` 문구가 잠시 표시되고 검은 화면으로 돌아가는지 확인합니다.
3. RPi5 로그에서 AI 모델 `2` 수신을 확인합니다.
4. PIR 또는 객체 감지 이벤트를 발생시킵니다.

```sh
cansend can0 022#01
```

또는:

```sh
python3 rpi4_central/scripts/send_object_camera_event.py <RPi4_IP> 1
```

5. UI 경고 화면, 운전자 Telegram 1차 알림, 부저/외부 LED/창문 명령을 확인합니다.
6. `CHILD_ALERT_ESCALATION_SEC`가 지난 뒤 경찰 Telegram 2차 알림이 1회만 전송되는지 확인합니다.
7. UI의 `차량 확인 완료` 버튼을 누르거나 다음 CAN을 보냅니다.

```sh
cansend can0 022#02
```

8. 경고 화면이 꺼지고 부저/외부 LED/창문 명령이 해제되는지 확인합니다. 잔류 감시는 계속 유지됩니다.

## 주요 환경변수

| 변수 | 기본값 | 설명 |
| --- | --- | --- |
| `CAN_INTERFACE` | `can0` | Central과 `can_setup.sh`가 사용할 CAN 인터페이스 |
| `CAN_BITRATE` | `500000` | `can_setup.sh` 비트레이트 |
| `CAN_DEBUG_LOG` | unset | `1`이면 Central이 수신 CAN raw frame 로그 출력 |
| `OBJECT_CAMERA_PORT` | `5010` | RPi5 객체 감지 UDP 수신 포트 |
| `DRIVER_ID_LISTEN_PORT` | `5000` | Qt UI 얼굴 인증 결과 UDP 수신 포트 |
| `DRIVER_PROFILE_PATH` | fallback search | 운전자 프로파일 JSON 경로 |
| `CENTRAL_APP_PATH` | `/opt/central/central_app` | UI 자동 실행 또는 start script가 사용할 Central 경로 |
| `UI_APP_PATH` | `/opt/central/school_bus_ui` | start script가 사용할 UI 경로 |
| `UI_START_CENTRAL` | `1` | UI 실행 시 Central 자동 실행 여부 |
| `CENTRAL_PID_FILE` | `/tmp/central_app.pid` | Central 중복 실행 방지 PID 파일 |
| `CENTRAL_KEEP_QUEUES` | unset | `1`이면 Central 종료 시 mqueue unlink 생략 |
| `CENTRAL_TO_QT_QUEUE` | `/central_to_qt` | Central -> UI mqueue 이름 |
| `QT_TO_CENTRAL_QUEUE` | `/qt_to_central` | UI -> Central mqueue 이름 |
| `CHILD_ALERT_LEVEL1_CMD` | unset | 1차 운전자 Telegram 명령 |
| `CHILD_ALERT_LEVEL2_CMD` | auto fallback | 2차 경찰 Telegram 명령 |
| `CHILD_ALERT_ESCALATION_SEC` | `30` | 1차 -> 2차 경고 대기 시간 |
| `CHILD_ACK_COOLDOWN_SEC` | `30` | 확인 완료 후 재경고 억제 시간 |
| `TELEGRAM_CONFIG` | fallback search | `telegram.txt` 경로 |
| `POLICE_TELEGRAM_CONFIG` | fallback search | `police.txt` 경로 |

## 문제 해결

### UI가 CAN 이벤트에 반응하지 않음

1. Central과 UI가 같은 `common/ipc_contract.h`를 기준으로 빌드되었는지 확인합니다.
2. 오래된 message queue를 정리합니다.

```sh
scripts/clean_runtime.sh
```

3. Central 로그에서 `send UI success`가 출력되는지 확인합니다.

### `central_app`이 이미 실행 중이라고 나옴

정상 종료 후 다시 실행합니다.

```sh
scripts/stop_system.sh
scripts/clean_runtime.sh
```

PID 파일만 남은 경우 기본 경로는 `/tmp/central_app.pid`입니다.

### CAN 수신이 안 됨

```sh
ip -details link show can0
sudo scripts/can_setup.sh
```

Central raw 로그가 필요하면 실행 전에 설정합니다.

```sh
export CAN_DEBUG_LOG=1
```

### Telegram 전송 실패

- `telegram.txt`, `police.txt`의 키 이름과 경로를 확인합니다.
- `GROUP_ID` 또는 `CHAT_ID`, `API_TOKEN`이 모두 있어야 합니다.
- 네트워크가 Telegram Bot API에 접근 가능한지 확인합니다.
- 실제 토큰은 커밋하지 마십시오.

### RPi5 모델이 바뀌지 않음

- 현재 Central의 RPi5 대상은 `central_app.c`의 `rpi5_send()`에 있는 `192.168.111.111:5020`입니다.
- RPi5 수신 서버가 문자열 명령이 아니라 `0`, `1`, `2` 숫자 payload를 처리하는지 확인합니다.
- IP/포트를 바꾸려면 `rpi5_send()`를 수정한 뒤 재빌드합니다.

## 제약 사항

- 안전벨트 경고 판단은 현재 1번, 2번 좌석만 활성입니다. UI는 8좌석을 표시하지만 Central의 `ACTIVE_SEAT_MASK`는 `0x03`입니다.
- RPi5 제어 대상 IP/포트는 환경변수가 아니라 `central_app.c`의 `rpi5_send()`에 하드코딩되어 있습니다.
- 얼굴 인증 결과 UDP에서 `FAIL`, `UNKNOWN`, `0`은 성공으로 처리되지 않지만 즉시 실패 화면을 강제하지는 않습니다. PIN 대체 인증은 얼굴 인증 화면의 10초 타임아웃 후 진입합니다.
- PIN 인증 성공은 현재 공용 driverId `0` 프로파일을 사용합니다.
- Telegram 1차 알림은 UI와 Central 양쪽 경로가 모두 존재합니다. 운용 시 중복 알림 정책이 필요하면 `CHILD_ALERT_LEVEL1_CMD` 사용 여부와 UI의 `telegram.txt` 설정을 함께 점검해야 합니다.
- `police.txt`가 git 추적 대상에 포함되어 있습니다. 실제 Bot token 또는 chat id가 들어 있다면 저장소 이력에서 제거하고 토큰을 재발급하는 것이 안전합니다.
- systemd UI 자동 실행은 디스플레이 서버 설정에 영향을 받습니다. 장비가 X11이 아닌 Wayland를 쓰거나 자동 로그인이 꺼져 있으면 unit 예시를 그대로 사용할 수 없습니다.

## 개발 메모

- `telegram.txt`, `police.txt`는 로컬 비밀 설정 파일입니다.
- `build/`, `cmake-build-*`, `build-rpi4-yocto/`는 빌드 산출물입니다.
- `rpi4_central/src/central_app.c:Zone.Identifier`는 Windows에서 넘어온 메타데이터 파일이며 빌드에는 사용되지 않습니다.
- Central은 정상 종료 시 mqueue를 unlink합니다. UI를 띄운 상태에서 Central만 재시작해야 하는 개발 상황에서는 `CENTRAL_KEEP_QUEUES=1`을 사용할 수 있습니다.
