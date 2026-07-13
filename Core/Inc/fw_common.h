/**
 ******************************************************************************
 * @file    fw_common.h
 * @brief   F/W 공용 정의 — CAN ID, 상태 enum, 제어 기준값, 공유 데이터 구조체
 *
 *          모든 앱 모듈(app/)이 include 하는 공용 헤더.
 *          하드웨어 의존 코드 없음 → 포팅 시 수정 불필요.
 ******************************************************************************
 */
#ifndef FW_COMMON_H
#define FW_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==========================================================================
 * CAN Message ID
 * ========================================================================== */
/* ---- 수신 (RCU/디스플레이 → F/W) ---- */
#define CAN_ID_PIR          0x022   /* PIR_DETECT_VALUE          */
#define CAN_ID_TEMP_TARGET  0x073   /* TEMPERATURE_TARGET_VALUE  */
#define CAN_ID_IGNITION     0x083   /* DISPLAY_IGNITION_STATE    */
#define CAN_ID_SEAT         0x093   /* SEAT_TARGET_VALUE         */
#define CAN_ID_LIGHT        0x0B3   /* INTERIOR_LIGHT_TARGET     */

/* ---- 송신 (F/W → CAN bus) ---- */
#define CAN_ID_ENV          0x041   /* ENV_SENSOR_VALUE          */
#define CAN_ID_DUST         0x051   /* DUST_SENSOR_VALUE         */
#define CAN_ID_STATUS       0x061   /* FW_ACTUATOR_STATUS         */

/* ---- 수신 메시지 최소 DLC (이보다 짧은 프레임은 무시) ---- */
#define CAN_DLC_MIN_PIR          1u
#define CAN_DLC_MIN_TEMP_TARGET  4u
#define CAN_DLC_MIN_IGNITION     2u
#define CAN_DLC_MIN_SEAT         5u
#define CAN_DLC_MIN_LIGHT        5u

/* ==========================================================================
 * 상태 enum
 * ========================================================================== */

/** 0x083 data[0] : 화면/시동 상태 */
typedef enum {
    IGN_OFF     = 0x00,   /* 화면 OFF + 시동 OFF */
    IGN_DISPLAY = 0x01,   /* 화면 ON  + 시동 OFF */
    IGN_ON      = 0x02    /* 화면 ON  + 시동 ON  */
} IgnitionState_t;

/** 0x083 data[1] : 운행 상태
 *
 *  [MOD] RPi4(central_app.c) / RTOS(main.c)와 3보드 공통 규약으로 통일.
 *  기존 F/W 정의(0=대기, 2=주차감시, 3=비상)는 송신측(RPi4)의 실제 값
 *  (0=주차, 1=운행, 2=경보)과 어긋나 있었다. 그 결과:
 *   - RPi4가 보낸 OP_ALARM(2)이 F/W에서는 '주차감시'로 해석되어
 *     잔류 경보 시 안전 팬/LED 분기(OP_ALARM==3)가 절대 실행되지 않았고,
 *   - 0x061 상태 회신의 data[3]도 UI에서 다른 의미로 읽혔다.
 *  canonical 값:
 *   0 = OP_PARKING  : 시동 OFF / 주차 (잔류 감시 대기 포함)
 *   1 = OP_RUNNING  : 운행 중
 *   2 = OP_ALARM    : 잔류 감지 경보 (RPi4가 확정했을 때만 수신됨)
 *   3 = (reserved)
 *   4 = OP_STOPPED_BOARDING : 승하차 중
 *  OP_STANDBY는 기존 코드 호환용 별칭(=OP_PARKING)으로 유지한다.
 */
typedef enum {
    OP_PARKING = 0,       /* 시동 OFF / 주차·감시 대기 */
    OP_STANDBY = 0,       /* (deprecated alias) 기존 코드 호환용 */
    OP_RUNNING = 1,       /* 운행                       */
    OP_ALARM   = 2,       /* 잔류 감지 경보 (RPi4 확정)  */
    OP_RESERVED_3 = 3,    /* 예약                        */
    OP_STOPPED_BOARDING = 4  /* 승하차 중                */
} OperationState_t;

/** 0x073 data[2] : 팬 제어 모드 */
typedef enum {
    FAN_MANUAL = 0,
    FAN_AUTO   = 1
} FanMode_t;

/* ==========================================================================
 * 제어 기준값 (초기값 — CAN 수신 시 덮어씀)
 * ========================================================================== */
#define FAN_TARGET_TEMP_X10      250    /* 25.0 °C  : 초기 목표 온도        */
#define FAN_TARGET_TEMP_MIN_X10  150    /* 15.0 °C  : CAN 입력 하한          */
#define FAN_TARGET_TEMP_MAX_X10  350    /* 35.0 °C  : CAN 입력 상한          */
#define FAN_DIFF_SMALL_X10        10    /*  1.0 °C  : 1단/2단 경계          */
#define FAN_DIFF_MEDIUM_X10       30    /*  3.0 °C  : 2단/3단 경계          */
#define LIGHT_THRESHOLD_X10     7000    /* 700.0 lux: 초기 조도 기준값
                                           NOTE: 실내 기준 다소 민감할 수 있음.
                                           실측 후 CAN(0x0B3)으로 튜닝 권장  */

/* ---- 좌석 모터 안전 제한 ---- */
#define SEAT_DURATION_MAX_MS    5000u   /* duration 상한 (무제한 구동 방지) */

/* ==========================================================================
 * 공유 데이터 구조체
 *   - 센서 태스크가 쓰고, CAN 송신/제어 태스크가 읽음
 *   - CAN RX ISR이 수신값 필드를 갱신 → volatile 필수
 *   - 제어 함수에서는 사용할 필드를 지역 변수로 복사(snapshot) 후 판단할 것
 * ========================================================================== */
typedef struct {
    /* ---- 센서 측정값 (모두 ×10 단위) ---- */
    int16_t  temperature_x10;       /* DHT22 온도  (예: 253 = 25.3°C)      */
    uint16_t humidity_x10;          /* DHT22 습도  (예: 605 = 60.5%)       */
    uint32_t lux_x10;               /* BH1750 조도 (예: 7345 = 734.5 lux)  */
    uint16_t pm1_0_atm;             /* PMS7003 PM1.0 (atm 기준)            */
    uint16_t pm2_5_atm;             /* PMS7003 PM2.5 (atm 기준)            */
    uint16_t pm10_atm;              /* PMS7003 PM10  (atm 기준)            */
    uint8_t  temp_humi_valid;       /* 온습도 유효 (0/1)                   */
    uint8_t  lux_valid;             /* 조도 유효   (0/1)                   */
    uint8_t  dust_valid;            /* 미세먼지 유효 (0/1)                 */
    uint8_t  env_seq;               /* 0x041 sequence counter (오버플로우 허용) */
    uint8_t  dust_seq;              /* 0x051 sequence counter (오버플로우 허용) */

    /* ---- CAN 수신값 (ISR이 갱신 — 범위 방어는 ISR에서 수행) ---- */
    uint8_t          pir_detected;              /* 0x022 data[0]   : 0=미감지 1=감지    */
    uint32_t         pir_event_seq;             /* 경고 OFF→ON 전환 횟수                */
    uint8_t          alert_latched;             /* UI 해제 전까지 유지되는 안전 경고     */
    int16_t          temp_target_x10;           /* 0x073 data[0~1] : 목표온도 ×10 (LE)  */
    FanMode_t        fan_mode;                  /* 0x073 data[2]   : 수동/자동          */
    uint8_t          fan_manual_level;          /* 0x073 data[3]   : 수동 팬 단계 0~3   */
    IgnitionState_t  ignition_state;            /* 0x083 data[0]                        */
    OperationState_t operation_state;           /* 0x083 data[1]                        */
    uint8_t          seat_direction;            /* 0x093 data[0]   : 0=정지 1=전진 2=후진 */
    uint8_t          seat_pwm_duty;             /* 0x093 data[1]   : 0~100              */
    uint16_t         seat_duration_ms;          /* 0x093 data[2~3] : 동작 시간 ms (LE)  */
    uint32_t         seat_cmd_seq;              /* 0x093 mailbox sequence.
                                                   작성 중 odd, 완료 후 even.
                                                   동일 명령도 새 seq로 처리          */
    uint8_t          light_mode;                /* 0x0B3 data[0]   : 0=수동 1=자동      */
    uint8_t          light_on;                  /* 0x0B3 data[1]   : 0=OFF 1=ON (수동)  */
    uint32_t         light_lux_threshold_x10;   /* 0x0B3 data[3~4] : 조도 기준 ×10 (LE) */

    /* ---- 현재 상태 (제어 출력) ---- */
    uint8_t  fan_level;             /* 현재 팬 단계 0~3                    */
    uint8_t  cabin_led_state;       /* 현재 LED 상태 0=OFF 1=ON            */
    uint8_t  status_seq;            /* 0x061 sequence counter              */
    uint32_t state_cmd_seq;         /* 0x083 상태 snapshot sequence        */
} FW_SharedData_t;

/** 전역 공유 데이터 — 정의는 main.c USER CODE PV 한 곳에서만.
 *  ISR/메인루프 공유이므로 volatile. */
extern volatile FW_SharedData_t g_fw;

#ifdef __cplusplus
}
#endif

#endif /* FW_COMMON_H */
