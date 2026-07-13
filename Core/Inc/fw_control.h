/**
 ******************************************************************************
 * @file    fw_control.h
 * @brief   제어 앱 레이어 — 팬 / 내부 LED / 좌석 모터
 *
 *          FreeRTOS 없음. 메인루프에서 tick 비교로 주기 호출:
 *            FW_FanControl_Update()   :  500ms 주기 권장
 *            FW_LightControl_Update() :  500ms 주기 (BH1750과 동일)
 *            FW_SeatControl_Update()  :  100ms 주기
 *
 *          공통 우선순위: 안전 제어 > 사용자 수동 설정 > 자동 제어.
 *          입력: g_fw (센서 측정값 + CAN 수신값, 스냅샷 후 사용)
 *          출력: BSP 액추에이터 API + g_fw 현재 상태 필드
 ******************************************************************************
 */
#ifndef FW_CONTROL_H
#define FW_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  팬 제어
 * @note   500ms 주기 호출 권장. g_fw.fan_level 갱신.
 *         우선순위: 안전(OP_ALARM 또는 주차감시+PIR, 센서 실패 시 최소 1단)
 *                   > 시동 ON 수동(CAN 지정 단계) > 자동(온도 diff) > 정지
 */
void FW_FanControl_Update(void);

/**
 * @brief  내부 LED 제어
 * @note   500ms 주기 호출. g_fw.cabin_led_state 갱신.
 *         우선순위: ALARM 강제 ON > 주차감시+PIR(어두움/invalid 시 ON)
 *                   > 시동 ON 수동 ON/OFF > OFF
 */
void FW_LightControl_Update(void);

/**
 * @brief  좌석 모터 제어 (명령 mailbox 소비 + duration 만료 자동 정지)
 * @note   100ms 주기 호출. 비블로킹.
 *         시동 ON + 운행/승하차 상태에서만 구동. duty ≤100, duration ≤5000ms,
 *         duration=0은 정지 처리(무제한 구동 금지).
 */
void FW_SeatControl_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_CONTROL_H */
