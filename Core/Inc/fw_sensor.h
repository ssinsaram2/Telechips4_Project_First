/**
 ******************************************************************************
 * @file    fw_sensor.h
 * @brief   센서 앱 레이어 — DHT22 / BH1750 / PMS7003 주기 업데이트
 *
 *          FreeRTOS 없음. 메인루프에서 tick 비교로 주기 호출:
 *            FW_DHT22_Update()   : 2000ms 주기 (~5ms 블로킹)
 *            FW_BH1750_Update()  :  500ms 주기 (비블로킹 상태머신)
 *            FW_PMS7003_Update() : 2000ms 주기 (비블로킹 스트림 파서)
 *
 *          측정 결과는 g_fw (fw_common.h)의 센서 필드에 기록됨.
 *          실패 시 이전 값 유지 + valid 플래그만 0으로 클리어.
 ******************************************************************************
 */
#ifndef FW_SENSOR_H
#define FW_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  센서 레이어 초기화
 * @note   DWT 사이클 카운터 초기화 (DHT22 us 지연용).
 *         BH1750은 One-Time 모드라 별도 init 명령 없음.
 */
void FW_Sensor_Init(void);

/**
 * @brief  DHT22 온습도 읽기 → g_fw.temperature_x10 / humidity_x10 갱신
 * @note   2000ms 주기 호출 (DHT22 최소 측정 간격 2s). 읽기 중 ~5ms 블로킹.
 *         성공: temp_humi_valid=1 / 실패: 이전 값 유지, valid=0
 */
void FW_DHT22_Update(void);

/**
 * @brief  BH1750 조도 → g_fw.lux_x10 갱신 (비블로킹 2단계 상태머신)
 * @note   500ms 주기 호출. 호출N: One-Time H-Res(0x20) 명령 전송,
 *         호출N+1: 결과 읽기 → 값 갱신 주기는 1000ms.
 *         성공: lux_valid=1 / 실패: 이전 값 유지, valid=0
 */
void FW_BH1750_Update(void);

/**
 * @brief  PMS7003 미세먼지 → g_fw.pm*_atm 갱신 (비블로킹 스트림 파서)
 * @note   2000ms 주기 호출. BSP UART RX 링버퍼에서 회수 후 헤더 재동기,
 *         길이/체크섬 검증. atm 기준값 사용.
 *         유효 프레임: dust_valid=1 / 약 6s 이상 미수신: valid=0
 */
void FW_PMS7003_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_SENSOR_H */
