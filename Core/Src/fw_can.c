/**
 ******************************************************************************
 * @file    fw_can.c
 * @brief   CAN 앱 레이어 구현
 *
 *          [아키텍처 규칙]
 *          - 앱 코드는 BSP 함수만 사용. 단, 아래 두 곳은 예외:
 *            1) FW_CAN_Init : 필터 설정 + Start + Notification (초기화 한정 허용)
 *            2) HAL_CAN_RxFifo0MsgPendingCallback : HAL 콜백 오버라이드
 *          - ISR 내에서는 g_fw 갱신만 수행. BSP_CAN_Send 등 송신 호출 금지.
 *          - 모든 수신 필드는 ISR에서 DLC 검사 + 범위 방어 후 저장.
 *
 *          [포팅 노트]
 *          TOPST VCP-G 포팅 시 본 파일의 HAL 예외 구간
 *          (FW_CAN_Init, RxFifo0 콜백)만 타깃 CAN 드라이버로 교체하면 됨.
 *          파싱/방어 로직(switch 내부)과 송신 함수는 그대로 재사용 가능.
 ******************************************************************************
 */
#include "fw_can.h"
#include "fw_common.h"
#include "bsp_fw.h"

#include "stm32f4xx_hal.h"   /* 예외 허용 구간(Init/RX 콜백) 전용 */
#include "main.h"            /* Error_Handler() 선언 */

/* CubeMX 생성 CAN 핸들 (main.c 또는 can.c에 정의됨) */
extern CAN_HandleTypeDef hcan1;

static int16_t clamp_target_temperature(int16_t value)
{
    if (value < FAN_TARGET_TEMP_MIN_X10) return FAN_TARGET_TEMP_MIN_X10;
    if (value > FAN_TARGET_TEMP_MAX_X10) return FAN_TARGET_TEMP_MAX_X10;
    return value;
}

/* ==========================================================================
 * 초기화 — 필터 설정 → Start → RX 인터럽트 활성화 (초기화 한정 HAL 허용)
 *
 *   호출 위치: MX_CAN1_Init() 이후. HAL_CAN_Start()는 본 함수가 수행하므로
 *   main에서 별도 호출 불필요.
 * ========================================================================== */
void FW_CAN_Init(void)
{
    /* ---- 수신 필터: 전체 수신(accept-all), FIFO0 연결 ----
     * 필터를 하나도 설정하지 않으면 어떤 프레임도 FIFO에 들어오지 않아
     * RX 인터럽트가 영원히 발생하지 않는다 (흔한 함정).
     * MVP에서는 전체 수신으로 열고, 추후 ID mask로 좁힐 수 있음.       */
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank           = 0;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = 0x0000;
    filter.FilterIdLow          = 0x0000;
    filter.FilterMaskIdHigh     = 0x0000;      /* mask 0 = 전체 통과 */
    filter.FilterMaskIdLow      = 0x0000;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ==========================================================================
 * CAN RX 인터럽트 콜백 (HAL weak 함수 오버라이드)
 *   - DLC 검사 → 범위 방어 → g_fw 갱신만 수행
 *   - 송신, 지연, 무거운 연산 금지 (ISR 컨텍스트)
 * ========================================================================== */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t             data[8];

    if (hcan->Instance != CAN1)
    {
        return;
    }

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, data) != HAL_OK)
    {
        return;   /* 수신 실패 시 조용히 리턴 (ISR에서 에러 처리 최소화) */
    }

    /* 본 프로젝트는 11-bit Standard Data Frame만 사용한다. */
    if (rx_header.IDE != CAN_ID_STD || rx_header.RTR != CAN_RTR_DATA)
    {
        return;
    }

    switch (rx_header.StdId)
    {
    case CAN_ID_PIR:                            /* 0x022 PIR 원시값 (RTOS 브로드캐스트) */
        if (rx_header.DLC < CAN_DLC_MIN_PIR) break;

        /*
         * [MOD] 잔류 감지 최종 판단 주체는 RPi4다 (요구사항 5).
         * 기존 코드는 PIR=1 수신만으로 alert_latched를 스스로 켰는데,
         * RTOS는 dbg_pir 원시값을 100ms 주기로 항상 브로드캐스트하므로
         * 운행 중 사람이 움직이기만 해도 F/W가 영구 안전모드(팬+LED)에
         * 빠지고, 해제값(2)은 아무도 보내지 않아 풀리지도 않았다.
         * 이제 0x022는 상태 보고용(pir_detected, 0x061 회신)으로만 쓰고,
         * 안전 latch는 0x083 OP_ALARM 명령으로만 켜고 끈다.
         */
        if (data[0] == 1u)
        {
            if (g_fw.pir_detected == 0u)
            {
                g_fw.pir_event_seq++;   /* OFF→ON 전환 횟수 (디버그용) */
            }
            g_fw.pir_detected = 1u;
        }
        else
        {
            g_fw.pir_detected = 0u;     /* 0 또는 구형 해제값(2) 모두 미감지 처리 */
        }
        break;

    case CAN_ID_TEMP_TARGET:                    /* 0x073 TEMPERATURE_TARGET_VALUE */
        if (rx_header.DLC < CAN_DLC_MIN_TEMP_TARGET) break;

        g_fw.temp_target_x10  = clamp_target_temperature(
            (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8)));
        g_fw.fan_mode         = (data[2] == (uint8_t)FAN_AUTO) ? FAN_AUTO : FAN_MANUAL;
        g_fw.fan_manual_level = (data[3] > 3u) ? 3u : data[3];
        break;

    case CAN_ID_IGNITION:                       /* 0x083 DISPLAY_IGNITION_STATE */
        if (rx_header.DLC < CAN_DLC_MIN_IGNITION) break;

        /* 두 필드를 하나의 일관된 snapshot으로 읽을 수 있도록 odd/even 갱신. */
        g_fw.state_cmd_seq++;

        /* 정의 외 값 방어: 시동 → OFF, 운행 → 주차 (안전측) */
        g_fw.ignition_state  = (data[0] <= (uint8_t)IGN_ON)
                               ? (IgnitionState_t)data[0] : IGN_OFF;
        g_fw.operation_state = (data[1] <= (uint8_t)OP_STOPPED_BOARDING)
                               ? (OperationState_t)data[1] : OP_PARKING;

        /*
         * [MOD] 안전 latch의 유일한 ON/OFF 경로 = 0x083 op 필드 (RPi4 권한).
         *  - op == OP_ALARM(2)  : RPi4가 residual_detected = PIR||AI 를 확정
         *                         → 팬/내부 LED 안전 제어 시작
         *  - op != OP_ALARM     : RPi4가 경보 해제(OP_PARKING) 또는 상태 전환
         *                         → 안전 latch 해제 (팬/LED는 일반 우선순위로 복귀)
         * 별도의 새 CAN ID 없이 기존 상태 브로드캐스트 채널 하나로
         * 시작/해제 권한을 일원화한다 (요구사항 6-1: 기존 ID 재사용).
         */
        if (g_fw.operation_state == OP_ALARM)
        {
            g_fw.alert_latched = 1u;
        }
        else
        {
            g_fw.alert_latched = 0u;
        }
        g_fw.state_cmd_seq++;
        break;

    case CAN_ID_SEAT:                           /* 0x093 SEAT_TARGET_VALUE */
        if (rx_header.DLC < CAN_DLC_MIN_SEAT) break;

        /* 명령 전체가 섞이지 않도록 mailbox를 odd/even sequence로 보호한다. */
        g_fw.seat_cmd_seq++;
        g_fw.seat_direction   = (data[0] <= 0x02u) ? data[0] : 0x00u;  /* 정의 외 → 정지 */
        g_fw.seat_pwm_duty    = (data[1] > 100u) ? 100u : data[1];
        g_fw.seat_duration_ms = (uint16_t)((uint16_t)data[2] | ((uint16_t)data[3] << 8));
        /* data[4] 목표 위치(0~100): 피드백 센서 없음 — 미사용 (TODO) */
        g_fw.seat_cmd_seq++;
        break;

    case CAN_ID_LIGHT:                          /* 0x0B3 INTERIOR_LIGHT_TARGET */
        if (rx_header.DLC < CAN_DLC_MIN_LIGHT) break;

        g_fw.light_mode = (data[0] != 0u) ? 1u : 0u;
        g_fw.light_on   = (data[1] != 0u) ? 1u : 0u;
        /* data[2] 밝기(0~100): 현재 GPIO 제어라 미사용 (TODO) */
        {
            uint16_t threshold =
                (uint16_t)((uint16_t)data[3] | ((uint16_t)data[4] << 8));
            /* 0은 미설정으로 취급한다. 안전 조명 기준이 무효화되는 것을 방지한다. */
            g_fw.light_lux_threshold_x10 =
                (threshold == 0u) ? LIGHT_THRESHOLD_X10 : threshold;
        }
        break;

    default:
        /* 기타 ID 무시 */
        break;
    }
}

/* ==========================================================================
 * 0x041 ENV_SENSOR_VALUE 송신 — 메인루프 2초 주기
 *
 *   data[0~1] : temperature_x10 (int16_t, LE)
 *   data[2~3] : humidity_x10    (uint16_t, LE)
 *   data[4~5] : lux_x10 (uint16_t, LE — 65535 초과 시 포화)
 *   data[6]   : bit0=온습도 valid, bit1=조도 valid
 *   data[7]   : sequence counter (uint8_t 오버플로우 허용)
 * ========================================================================== */
void FW_CAN_SendEnv(void)
{
    uint8_t  buf[8];
    int16_t  t = g_fw.temperature_x10;
    uint16_t t_raw = (uint16_t)t;
    uint16_t h = g_fw.humidity_x10;
    uint32_t l = g_fw.lux_x10;

    /* uint16 필드에 담기 전 포화 (6553.5 lux 초과 시) */
    if (l > 65535u)
    {
        l = 65535u;
    }

    buf[0] = (uint8_t)(t_raw & 0xFFu);
    buf[1] = (uint8_t)((t_raw >> 8) & 0xFFu);
    buf[2] = (uint8_t)(h & 0xFF);
    buf[3] = (uint8_t)((h >> 8) & 0xFF);
    buf[4] = (uint8_t)(l & 0xFF);
    buf[5] = (uint8_t)((l >> 8) & 0xFF);
    buf[6] = (uint8_t)((g_fw.temp_humi_valid & 0x01u) |
                       ((g_fw.lux_valid & 0x01u) << 1));
    buf[7] = g_fw.env_seq;

    if (BSP_CAN_Send(CAN_ID_ENV, buf, 8u) == BSP_OK)
    {
        g_fw.env_seq++;
    }
}

/* ==========================================================================
 * 0x051 DUST_SENSOR_VALUE 송신 — 메인루프 2초 주기
 *
 *   data[0~1] : PM1.0 atm (uint16_t, LE)
 *   data[2~3] : PM2.5 atm (uint16_t, LE)
 *   data[4~5] : PM10  atm (uint16_t, LE)
 *   data[6]   : valid (0 or 1)
 *   data[7]   : sequence counter (uint8_t 오버플로우 허용)
 * ========================================================================== */
void FW_CAN_SendDust(void)
{
    uint8_t buf[8];

    buf[0] = (uint8_t)(g_fw.pm1_0_atm & 0xFF);
    buf[1] = (uint8_t)((g_fw.pm1_0_atm >> 8) & 0xFF);
    buf[2] = (uint8_t)(g_fw.pm2_5_atm & 0xFF);
    buf[3] = (uint8_t)((g_fw.pm2_5_atm >> 8) & 0xFF);
    buf[4] = (uint8_t)(g_fw.pm10_atm & 0xFF);
    buf[5] = (uint8_t)((g_fw.pm10_atm >> 8) & 0xFF);
    buf[6] = g_fw.dust_valid;
    buf[7] = g_fw.dust_seq;

    if (BSP_CAN_Send(CAN_ID_DUST, buf, 8u) == BSP_OK)
    {
        g_fw.dust_seq++;
    }
}

/* ==========================================================================
 * 0x061 FW_ACTUATOR_STATUS 송신 — 메인루프 500ms 주기
 *
 *   data[0] : fan_level (0~3)
 *   data[1] : cabin_led_state (0/1)
 *   data[2] : ignition_state
 *   data[3] : operation_state
 *   data[4] : pir_detected
 *   data[5] : bit0=온습도 valid, bit1=조도 valid, bit2=미세먼지 valid
 *   data[6] : bit0=alert_latched
 *   data[7] : sequence counter
 * ========================================================================== */
void FW_CAN_SendStatus(void)
{
    uint8_t buf[8];
    uint32_t seq_before;
    uint32_t seq_after;

    /* 0x083의 시동/운행 두 필드는 같은 CAN 프레임의 값으로 snapshot한다. */
    do
    {
        seq_before = g_fw.state_cmd_seq;
        buf[2] = (uint8_t)g_fw.ignition_state;
        buf[3] = (uint8_t)g_fw.operation_state;
        seq_after = g_fw.state_cmd_seq;
    } while ((seq_before != seq_after) || ((seq_after & 1u) != 0u));

    buf[0] = g_fw.fan_level;
    buf[1] = g_fw.cabin_led_state;
    buf[4] = g_fw.pir_detected;
    buf[5] = (uint8_t)((g_fw.temp_humi_valid & 0x01u) |
                       ((g_fw.lux_valid & 0x01u) << 1) |
                       ((g_fw.dust_valid & 0x01u) << 2));
    buf[6] = (uint8_t)(g_fw.alert_latched & 0x01u);
    buf[7] = g_fw.status_seq;

    if (BSP_CAN_Send(CAN_ID_STATUS, buf, 8u) == BSP_OK)
    {
        g_fw.status_seq++;
    }
}
