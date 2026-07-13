/**
 ******************************************************************************
 * @file    bsp_fw.h
 * @brief   BSP (Board Support Package) interface — 포팅 경계 레이어
 *
 *          앱 코드(app/)는 반드시 이 헤더의 함수만 호출한다.
 *          HAL 직접 호출 금지. (예외: DHT22 1-wire 타이밍 루틴 내부)
 *
 *          본 헤더는 HAL 헤더를 include하지 않는다 (stdint만 의존).
 *          → 앱 레이어가 STM32 HAL 타입에 묶이지 않음.
 *
 *          포팅 방법:
 *            STM32F429ZI : bsp_fw_stm32.c  가 본 인터페이스를 구현
 *            TOPST VCP-G : bsp_fw_topst.c  로 구현 파일만 교체
 ******************************************************************************
 */
#ifndef BSP_FW_H
#define BSP_FW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==========================================================================
 * 공용 상태 코드 (HAL_StatusTypeDef 대체 — 플랫폼 비의존)
 * ========================================================================== */
typedef enum {
    BSP_OK      = 0,
    BSP_ERROR   = 1,
    BSP_BUSY    = 2,
    BSP_TIMEOUT = 3
} BSP_Status_t;

/* ==========================================================================
 * 시간 (HAL_GetTick / HAL_Delay 래퍼 — 앱 레이어는 이것만 사용)
 * ========================================================================== */

/** @brief  부팅 후 경과 ms tick (STM32: HAL_GetTick 매핑) */
uint32_t BSP_GetTickMs(void);

/** @brief  ms 블로킹 지연 (초기화 구간 등 제한적 사용 권장) */
void BSP_DelayMs(uint32_t ms);

/* ==========================================================================
 * I2C  (BH1750 — I2C1, 7-bit addr 0x23)
 * ========================================================================== */

/**
 * @brief  I2C 마스터 송신 (blocking)
 * @param  addr_7bit  7-bit 슬레이브 주소 (내부에서 <<1 처리)
 */
BSP_Status_t BSP_I2C_Write(uint8_t addr_7bit, const uint8_t *data, uint16_t len);

/**
 * @brief  I2C 마스터 수신 (blocking)
 * @param  addr_7bit  7-bit 슬레이브 주소 (내부에서 <<1 처리)
 */
BSP_Status_t BSP_I2C_Read(uint8_t addr_7bit, uint8_t *buf, uint16_t len);

/* ==========================================================================
 * UART  (PMS7003 — UART4, 9600bps, 32-byte frame)
 * ========================================================================== */

/**
 * @brief  UART 수신 (blocking, timeout 지정) — 범용/디버그용
 * @note   PMS7003 주기 수신에는 사용하지 말 것 (메인루프 블로킹 위험).
 *         FW_PMS7003_Update는 BSP_UART_DustRead(비블로킹)를 사용한다.
 */
BSP_Status_t BSP_UART_Receive(uint8_t *buf, uint16_t len, uint32_t timeout_ms);

/**
 * @brief  PMS7003 RX 링버퍼에서 수신 바이트를 꺼냄 (비블로킹)
 * @param  buf     복사 대상 버퍼
 * @param  maxlen  최대 복사 길이
 * @retval 실제 복사된 바이트 수 (0 = 수신 데이터 없음)
 * @note   구현체는 UART RX 인터럽트로 1바이트씩 링버퍼에 적재해야 함.
 *         링버퍼 크기는 128바이트 이상 권장
 *         (PMS7003 32B/s × 폴링주기 2s = 64B + 여유).
 */
uint16_t BSP_UART_DustRead(uint8_t *buf, uint16_t maxlen);

/* ==========================================================================
 * CAN  (송신 전용 API — 수신은 CAN RX 인터럽트에서 처리)
 * ========================================================================== */

/**
 * @brief  CAN 표준 프레임 송신
 * @param  id    11-bit standard ID
 * @param  data  송신 데이터 (최대 8 bytes)
 * @param  dlc   데이터 길이 (0~8)
 */
BSP_Status_t BSP_CAN_Send(uint32_t id, const uint8_t *data, uint8_t dlc);

/* ==========================================================================
 * FAN PWM  (TIM3 CH1, 25kHz, PSC=0 / Period=3359)
 * ========================================================================== */

/**
 * @brief  팬 PWM 듀티 설정
 * @param  percent  듀티 0~100 (%). 100 초과 시 100으로 포화
 */
void BSP_PWM_Fan_SetDuty(uint8_t percent);

/* ==========================================================================
 * SEAT MOTOR  (H브릿지 DC모터, DMD-150 방식)
 *   PWM : TIM4 CH1, 1kHz (PSC=83 / Period=999)
 *   IN1 : SEAT_IN1_Pin / SEAT_IN1_GPIO_Port (PE0)
 *   IN2 : SEAT_IN2_Pin / SEAT_IN2_GPIO_Port (PE2)
 * ========================================================================== */

/** @brief 정방향 구동 (IN1=HIGH, IN2=LOW + PWM), duty 0~100% */
void BSP_Seat_Forward(uint8_t duty_percent);

/** @brief 역방향 구동 (IN1=LOW, IN2=HIGH + PWM), duty 0~100% */
void BSP_Seat_Backward(uint8_t duty_percent);

/** @brief 정지 (IN1=LOW, IN2=LOW + PWM=0) */
void BSP_Seat_Stop(void);

/* ==========================================================================
 * CABIN LED  (CABIN_LED_Pin / CABIN_LED_GPIO_Port, GPIOE)
 * ========================================================================== */

/** @brief 내부(캐빈) LED 제어, 0=OFF 1=ON */
void BSP_GPIO_LED_Cabin_Write(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* BSP_FW_H */
