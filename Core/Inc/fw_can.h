/**
 ******************************************************************************
 * @file    fw_can.h
 * @brief   CAN 앱 레이어 — RX 인터럽트 파싱 + 주기 송신 (0x041 / 0x051)
 *
 *          수신: HAL_CAN_RxFifo0MsgPendingCallback (fw_can.c에서 오버라이드)
 *                → DLC 검사 + 범위 방어 후 g_fw 갱신. 송신/무거운 처리 금지.
 *          송신: 메인루프에서 2초 주기로 FW_CAN_SendEnv / FW_CAN_SendDust 호출
 ******************************************************************************
 */
#ifndef FW_CAN_H
#define FW_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  CAN 수신 경로 초기화 (필터 accept-all 설정 → Start → RX 인터럽트)
 * @note   초기화 한정으로 HAL 직접 호출 허용 구간.
 *         MX_CAN1_Init() 이후에 호출할 것.
 *         HAL_CAN_Start()를 내부에서 수행하므로 main에서 별도 호출 불필요.
 */
void FW_CAN_Init(void);

/**
 * @brief  0x041 ENV_SENSOR_VALUE 송신 (온도/습도/조도 + valid + seq)
 * @note   메인루프에서 2초 주기 호출. 인터럽트 컨텍스트에서 호출 금지.
 *         lux_x10이 65535를 넘으면 65535로 포화하여 송신.
 */
void FW_CAN_SendEnv(void);

/**
 * @brief  0x051 DUST_SENSOR_VALUE 송신 (PM1.0/PM2.5/PM10 atm + valid + seq)
 * @note   메인루프에서 2초 주기 호출. 인터럽트 컨텍스트에서 호출 금지.
 */
void FW_CAN_SendDust(void);

/**
 * @brief  0x061 FW_ACTUATOR_STATUS 송신
 * @note   팬 단계, 내부 LED, 시동/운행 상태, PIR 및 센서 valid를 전달한다.
 *         시나리오의 F/W 출력 상태 회신용으로 500ms 주기 호출을 권장한다.
 */
void FW_CAN_SendStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_CAN_H */
