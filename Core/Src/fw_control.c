/**
 ******************************************************************************
 * @file    fw_control.c
 * @brief   제어 앱 레이어 구현 (팬 / 내부 LED / 좌석 모터)
 *
 *          [아키텍처 규칙]
 *          - HAL 직접 호출 금지. BSP 함수만 사용 (시간도 BSP_GetTickMs).
 *          - g_fw는 ISR과 공유(volatile) → 판단에 쓰는 필드는 함수 시작 시
 *            지역 변수로 스냅샷 후 사용 (실행 중 값 변경으로 인한 꼬임 방지).
 *
 *          [제어 우선순위]
 *          팬/LED 모두: 1순위 안전 제어
 *                       2순위 사용자 수동 설정
 *                       3순위 자동 제어
 *          [MOD] 안전 제어(alert_latched)는 RPi4가 0x083 op=OP_ALARM(2)으로
 *          시작하고 op!=OP_ALARM으로 해제한다 (fw_can.c). F/W는 경보 여부를
 *          스스로 판단하지 않으며, 경보 중의 팬 단계/내부 LED ON·OFF만
 *          로컬 센서(온도 25/28/30°C, 조도 700lux) 기준으로 실행한다.
 *          → OP_ALARM 상태에서는 PIR 값이 0으로 내려가도 안전 동작을 유지한다.
 *
 *          [포팅 노트]
 *          본 파일은 하드웨어 완전 비의존 — BSP + g_fw만 사용.
 ******************************************************************************
 */
#include "fw_control.h"
#include "fw_common.h"
#include "bsp_fw.h"

/* ==========================================================================
 * 팬 자동 제어 상수
 * ========================================================================== */
/* 주차감시/비상 모드 환기 보조 절대온도 경계 (×10) */
#define FAN_PARK_TEMP_L1_X10    250   /* 25.0°C : 1단 시작 */
#define FAN_PARK_TEMP_L2_X10    280   /* 28.0°C : 2단 시작 */
#define FAN_PARK_TEMP_L3_X10    300   /* 30.0°C : 3단 시작 */

/* 팬 단계 → 듀티(%) 변환: 0/33/66/100 */
static const uint8_t fan_level_duty[4] = { 0u, 33u, 66u, 100u };

/**
 * @brief 안전 제어 활성 조건
 *
 * [MOD] alert_latched는 RPi4의 0x083 op=OP_ALARM 명령으로만 1이 되고,
 * op!=OP_ALARM(경보 해제/상태 전환) 수신 시 0으로 내려간다 (fw_can.c).
 */
static inline uint8_t safety_active(uint8_t alert_latched)
{
    return (alert_latched != 0u) ? 1u : 0u;
}

/** 0x083의 시동/운행 두 필드를 같은 CAN 프레임 기준으로 읽는다. */
static void vehicle_state_snapshot(IgnitionState_t *ignition,
                                   OperationState_t *operation)
{
    uint32_t seq_before;
    uint32_t seq_after;

    do
    {
        seq_before = g_fw.state_cmd_seq;
        *ignition  = g_fw.ignition_state;
        *operation = g_fw.operation_state;
        seq_after  = g_fw.state_cmd_seq;
    } while ((seq_before != seq_after) || ((seq_after & 1u) != 0u));
}

static inline uint8_t normal_cabin_control_allowed(IgnitionState_t ignition,
                                                   OperationState_t operation)
{
    return (uint8_t)((ignition == IGN_ON) &&
                     ((operation == OP_RUNNING) ||
                      (operation == OP_STOPPED_BOARDING)));
}

/* ==========================================================================
 * 팬 제어 — 500ms 주기 권장
 *
 *   1순위: 안전 제어 → 절대온도 기반 환기 보조
 *          (온습도 센서 invalid 시 fail-safe: 최소 1단 환기)
 *   2순위: 수동 모드 → CAN 지정 단계
 *   3순위: 자동 + 운행/승하차 중 → 목표온도 diff 기반
 *   그 외: 정지
 * ========================================================================== */
void FW_FanControl_Update(void)
{
    /* ---- 스냅샷 (ISR 갱신 필드) ---- */
    IgnitionState_t  ignition;
    OperationState_t op;
    uint8_t          alert       = g_fw.alert_latched;
    FanMode_t        mode        = g_fw.fan_mode;
    uint8_t          manual_lv   = g_fw.fan_manual_level;
    int16_t          temp        = g_fw.temperature_x10;
    int16_t          target      = g_fw.temp_target_x10;
    uint8_t          temp_valid  = g_fw.temp_humi_valid;

    uint8_t level = 0u;

    vehicle_state_snapshot(&ignition, &op);

    if (safety_active(alert))
    {
        /* ---- 1순위: 잔류 감지 안전 제어 (수동 설정보다 우선) ----
         * 에어컨이 아니므로 목표온도 무관, 차내 과열 방지 목적       */
        if (temp_valid == 0u)
        {
            level = 1u;   /* fail-safe: 센서 실패 시 최소 환기 */
        }
        else if (temp < FAN_PARK_TEMP_L1_X10)            /* < 25.0°C */
        {
            level = 0u;
        }
        else if (temp < FAN_PARK_TEMP_L2_X10)            /* < 28.0°C */
        {
            level = 1u;
        }
        else if (temp < FAN_PARK_TEMP_L3_X10)            /* < 30.0°C */
        {
            level = 2u;
        }
        else                                             /* ≥ 30.0°C */
        {
            level = 3u;
        }
    }
    else if (!normal_cabin_control_allowed(ignition, op))
    {
        /* 시동 OFF/감시 종료 후 남은 수동 명령으로 팬이 계속 도는 것을 방지. */
        level = 0u;
    }
    else if (mode == FAN_MANUAL)
    {
        /* ---- 2순위: 수동 — CAN(0x073 data[3]) 지정 단계 ---- */
        level = (manual_lv > 3u) ? 3u : manual_lv;   /* 방어적 포화 */
    }
    else if (temp_valid == 0u)
    {
        /* 일반 운행에서는 오류 센서의 오래된 값으로 자동 제어하지 않는다. */
        level = 0u;
    }
    else
    {
        /* ---- 3순위: 자동 + 운행/승하차 — 목표 온도 diff 기반 ---- */
        int16_t diff = temp - target;

        if (diff <= 0)
        {
            level = 0u;
        }
        else if (diff < FAN_DIFF_SMALL_X10)        /* < 1.0°C */
        {
            level = 1u;
        }
        else if (diff < FAN_DIFF_MEDIUM_X10)       /* < 3.0°C */
        {
            level = 2u;
        }
        else                                        /* ≥ 3.0°C */
        {
            level = 3u;
        }
    }
    BSP_PWM_Fan_SetDuty(fan_level_duty[level]);
    g_fw.fan_level = level;
}

/* ==========================================================================
 * 내부 LED 제어 — 500ms 주기
 *
 *   1순위: 안전 제어 (수동/자동 무관 — 잔류 어린이 안전 우선)
 *          - ALARM은 조도와 무관하게 ON 유지
 *          - 주차감시+PIR에서 invalid 또는 어두움이면 ON
 *   2순위: 수동 모드 → CAN ON/OFF 그대로
 *   그 외(자동, 비안전 상황): OFF
 *
 *   TODO: 0x0B3 data[2] 밝기 — 현재 GPIO ON/OFF라 미사용.
 *         PWM LED로 교체 시 BSP API 확장하여 적용 예정.
 * ========================================================================== */
void FW_LightControl_Update(void)
{
    /* ---- 스냅샷 ---- */
    IgnitionState_t  ignition;
    OperationState_t op;
    uint8_t          alert     = g_fw.alert_latched;
    uint8_t          mode      = g_fw.light_mode;
    uint8_t          manual_on = g_fw.light_on;
    uint32_t         lux       = g_fw.lux_x10;
    uint32_t         threshold = g_fw.light_lux_threshold_x10;
    uint8_t          lux_valid = g_fw.lux_valid;

    uint8_t led = 0u;

    vehicle_state_snapshot(&ignition, &op);

    if (safety_active(alert))
    {
        /*
         * ---- 1순위: 잔류 감지 안전 조명 (RPi4의 OP_ALARM 명령 기준) ----
         * [MOD] 아래에 중복되어 있던 동일 조건의 도달 불가(dead) 분기를 제거.
         *  - 조도 < 기준(기본 700.0 lux) → ON
         *  - 조도 센서 invalid → fail-safe ON
         *  - 밝으면 OFF (주간에는 조명 불필요)
         */
        if (lux_valid == 0u)
        {
            led = 1u;        /* 조도 센서가 죽었으면 fail-safe로 ON */
        }
        else if (lux < threshold)
        {
            led = 1u;        /* 어두우면 ON */
        }
        else
        {
            led = 0u;        /* 밝으면 OFF */
        }
    }
    else if (normal_cabin_control_allowed(ignition, op) && (mode == 0u))
    {
        /* ---- 2순위: 수동 ---- */
        led = (manual_on != 0u) ? 1u : 0u;
    }
    else
    {
        /* 자동 모드 + 비안전 상황 → OFF (자동 점등 조건은 안전 상황 한정) */
        led = 0u;
    }

    BSP_GPIO_LED_Cabin_Write(led);
    g_fw.cabin_led_state = led;
}

/* ==========================================================================
 * 좌석 모터 제어 — 100ms 주기, 비블로킹
 *
 *   0) 상태 제한: 시동 ON + 운행/승하차 상태에서만 구동 허용
 *   1) CAN(0x093) odd/even mailbox snapshot → 새 명령 적용
 *      (동일 명령 재전송도 sequence 변경으로 재실행됨)
 *   2) 안전 제한: duty ≤100, duration ≤ SEAT_DURATION_MAX_MS,
 *      duration=0 → 무제한이 아니라 정지 처리
 *   3) 구동 중 duration 경과 → 자동 정지 (BSP_GetTickMs 비교)
 *
 *   TODO: 0x093 data[4] 목표 위치 — 피드백 센서 추가 시 사용 예정.
 * ========================================================================== */
void FW_SeatControl_Update(void)
{
    static uint8_t  seat_running    = 0u;
    static uint32_t seat_start_tick = 0u;
    static uint16_t seat_run_ms     = 0u;
    static uint32_t consumed_seq    = 0u;

    IgnitionState_t ignition;
    OperationState_t op;
    uint32_t seq_before;
    uint32_t seq_after;
    uint8_t  dir;
    uint8_t  duty;
    uint16_t dur;

    vehicle_state_snapshot(&ignition, &op);

    /* ---- 0) 상태 제한: 시동 ON + 운행/승하차에서만 좌석 구동 ---- */
    if (!normal_cabin_control_allowed(ignition, op))
    {
        if (seat_running)
        {
            BSP_Seat_Stop();
            seat_running = 0u;
        }
        /* 비허용 상태에서 받은 명령은 나중에 갑자기 실행되지 않도록 폐기. */
        consumed_seq = g_fw.seat_cmd_seq;
        return;
    }

    /* ---- 1) 새 명령의 일관된 snapshot 확보 ---- */
    seq_before = g_fw.seat_cmd_seq;
    dir         = g_fw.seat_direction;
    duty        = g_fw.seat_pwm_duty;
    dur         = g_fw.seat_duration_ms;
    seq_after  = g_fw.seat_cmd_seq;

    if ((seq_before == seq_after) &&
        ((seq_after & 1u) == 0u) &&
        (seq_after != consumed_seq))
    {
        consumed_seq = seq_after;

        /* ---- 2) 안전 제한 ---- */
        if (duty > 100u)
        {
            duty = 100u;
        }
        if (dur > SEAT_DURATION_MAX_MS)
        {
            dur = SEAT_DURATION_MAX_MS;
        }

        if (dur == 0u)
        {
            /* duration 0 = 무제한 구동 금지 → 정지 처리 */
            BSP_Seat_Stop();
            seat_running = 0u;
        }
        else
        {
            switch (dir)
            {
            case 0x01u:                               /* 전진 */
                BSP_Seat_Forward(duty);
                seat_running    = 1u;
                seat_start_tick = BSP_GetTickMs();
                seat_run_ms     = dur;
                break;

            case 0x02u:                               /* 후진 */
                BSP_Seat_Backward(duty);
                seat_running    = 1u;
                seat_start_tick = BSP_GetTickMs();
                seat_run_ms     = dur;
                break;

            case 0x00u:                               /* 정지 */
            default:                                  /* 정의 외 값 → 안전 정지 */
                BSP_Seat_Stop();
                seat_running = 0u;
                break;
            }
        }
    }

    /* ---- 3) duration 경과 후 자동 정지 (unsigned 비교 — tick wrap 안전) ---- */
    if (seat_running)
    {
        if ((BSP_GetTickMs() - seat_start_tick) >= (uint32_t)seat_run_ms)
        {
            BSP_Seat_Stop();
            seat_running = 0u;
        }
    }
}
