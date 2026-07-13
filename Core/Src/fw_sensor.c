/**
 ******************************************************************************
 * @file    fw_sensor.c
 * @brief   센서 앱 레이어 구현 (DHT22 / BH1750 / PMS7003)
 *
 *          [아키텍처 규칙]
 *          - 원칙: BSP 함수만 사용. 블로킹 지연 없음.
 *          - 예외: DHT22 1-wire GPIO 타이밍 함수 내부(DHT22_Read 및 하위
 *            static 함수)에서만 HAL_GPIO_* 직접 호출 허용
 *          - us 지연은 DWT 사이클 카운터 사용
 *
 *          [비블로킹 설계]
 *          - BH1750 : 2단계 상태머신 (호출N: 측정 명령 / 호출N+1: 결과 읽기)
 *                     → HAL_Delay(180) 제거. 값 갱신 주기는 1000ms가 됨.
 *          - PMS7003: BSP측 UART RX 인터럽트 링버퍼에서 비블로킹으로 꺼내
 *                     스트림 파서로 프레임 재동기/검증 → 블로킹 0ms.
 *          - DHT22  : 읽기 자체 ~5ms 블로킹 (프로토콜 특성상 불가피, 2s 주기)
 *
 *          [포팅 노트]
 *          TOPST VCP-G 포팅 시:
 *          - DHT22_Read 내부 GPIO/us지연 구간만 타깃 GPIO API로 교체
 *          - BH1750/PMS7003은 BSP 경유이므로 그대로 재사용 가능
 ******************************************************************************
 */
#include "fw_sensor.h"
#include "fw_common.h"
#include "bsp_fw.h"

#include <string.h>          /* memmove (PMS 스트림 파서) */

#include "stm32f4xx_hal.h"   /* DHT22 1-wire 예외 허용 구간 + DWT 레지스터 */
#include "main.h"            /* DHT_DATA_Pin / DHT_DATA_GPIO_Port (CubeMX) */

/* ==========================================================================
 * BH1750 상수
 * ========================================================================== */
#define BH1750_ADDR_7BIT        0x23u   /* 7-bit 주소 (8-bit = 0x46)        */
#define BH1750_CMD_ONE_TIME_H   0x20u   /* One Time H-Resolution Mode       */
/* 변환 시간 max 180ms (datasheet tHR) — 다음 500ms 주기 호출에서 읽으므로
 * 대기 시간은 자동 충족됨 (500ms > 180ms)                                  */

/* ==========================================================================
 * PMS7003 상수
 * ========================================================================== */
#define PMS_FRAME_LEN           32u
#define PMS_START1              0x42u   /* 'B' */
#define PMS_START2              0x4Du   /* 'M' */
#define PMS_BODY_LEN            28u     /* frame[2~3]에 실리는 길이 필드 값  */
#define PMS_ACC_LEN             96u     /* 스트림 누적 버퍼 (2s분 64B + 여유) */
#define PMS_MISS_LIMIT          3u      /* 연속 미수신 한계 (~6s) → invalid  */

/* ==========================================================================
 * DHT22 타이밍 상수 (datasheet 기준)
 * ========================================================================== */
#define DHT_START_LOW_US        2000u   /* Start: LOW 2ms (min 1ms)         */
#define DHT_START_HIGH_US       30u     /* Start: HIGH 30us 후 INPUT 전환   */
#define DHT_BIT_THRESHOLD_US    40u     /* HIGH > 40us → '1' (26~28us='0')  */
#define DHT_LEVEL_TIMEOUT_US    100u    /* 레벨 대기 타임아웃 (80us + 여유)  */
#define DHT_TEMP_MIN_X10       (-400)   /* DHT22 사양: -40.0°C               */
#define DHT_TEMP_MAX_X10         800    /* DHT22 사양:  80.0°C               */
#define DHT_HUMI_MAX_X10        1000u   /* 100.0% RH                          */

/* ==========================================================================
 * DWT 기반 마이크로초 지연  (FW_Sensor_Init에서 활성화)
 * ========================================================================== */
static inline void delay_us(uint32_t us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000u);
    while ((DWT->CYCCNT - start) < cycles) { }
}

/* ==========================================================================
 * DHT22 1-wire 로우레벨 — HAL 직접 호출 허용 예외 구간
 * ========================================================================== */

/** DATA 핀 → 출력(Open-Drain + Pull-up) 전환 */
static void dht_pin_output(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = DHT_DATA_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT_DATA_GPIO_Port, &gpio);
}

/** DATA 핀 → 입력(Pull-up) 전환 */
static void dht_pin_input(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = DHT_DATA_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT_DATA_GPIO_Port, &gpio);
}

/**
 * @brief  DATA 핀이 지정 레벨이 될 때까지 대기
 * @retval 대기에 걸린 시간(us, 근사) / 타임아웃 시 0xFFFFFFFF
 */
static uint32_t dht_wait_level(GPIO_PinState level, uint32_t timeout_us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cyc_us = SystemCoreClock / 1000000u;
    uint32_t limit  = timeout_us * cyc_us;

    while (HAL_GPIO_ReadPin(DHT_DATA_GPIO_Port, DHT_DATA_Pin) != level)
    {
        if ((DWT->CYCCNT - start) > limit)
        {
            return 0xFFFFFFFFu;   /* timeout */
        }
    }
    return (DWT->CYCCNT - start) / cyc_us;
}

/**
 * @brief  DHT22 온습도 읽기 (blocking, 약 5ms 소요)
 * @retval 0=성공, -1=응답 없음, -2=비트 수신 타임아웃, -3=체크섬 오류
 * @note   비트 타이밍 보호를 위해 40bit 수신 동안 인터럽트를 잠시 막는다
 *         (~4ms — CAN RX FIFO(3-deep)가 흡수, 2초 주기이므로 허용).
 */
static int DHT22_Read(uint16_t *humi_x10, int16_t *temp_x10)
{
    uint8_t data[5] = {0};

    /* ---- Start 신호: OUTPUT, LOW 2ms → HIGH 30us → INPUT 전환 ---- */
    dht_pin_output();
    HAL_GPIO_WritePin(DHT_DATA_GPIO_Port, DHT_DATA_Pin, GPIO_PIN_RESET);
    delay_us(DHT_START_LOW_US);
    HAL_GPIO_WritePin(DHT_DATA_GPIO_Port, DHT_DATA_Pin, GPIO_PIN_SET);

    /* 센서 응답 시작 전부터 timing window를 보호한다. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    delay_us(DHT_START_HIGH_US);
    dht_pin_input();

    /* ---- 센서 응답: LOW 80us → HIGH 80us → LOW(데이터 시작) ---- */
    if (dht_wait_level(GPIO_PIN_RESET, DHT_LEVEL_TIMEOUT_US) == 0xFFFFFFFFu ||
        dht_wait_level(GPIO_PIN_SET,   DHT_LEVEL_TIMEOUT_US) == 0xFFFFFFFFu ||
        dht_wait_level(GPIO_PIN_RESET, DHT_LEVEL_TIMEOUT_US) == 0xFFFFFFFFu)
    {
        if (primask == 0u) { __enable_irq(); }
        return -1;
    }

    /* ---- 40bit 수신: 각 비트 = LOW ~50us + HIGH(26~28us='0' / 70us='1') ---- */
    for (uint8_t i = 0; i < 40u; i++)
    {
        if (dht_wait_level(GPIO_PIN_SET, DHT_LEVEL_TIMEOUT_US) == 0xFFFFFFFFu)
        {
            if (primask == 0u) { __enable_irq(); }
            return -2;
        }
        uint32_t high_us = dht_wait_level(GPIO_PIN_RESET, DHT_LEVEL_TIMEOUT_US);
        if (high_us == 0xFFFFFFFFu)
        {
            if (primask == 0u) { __enable_irq(); }
            return -2;
        }

        data[i / 8u] <<= 1;
        if (high_us > DHT_BIT_THRESHOLD_US)
        {
            data[i / 8u] |= 1u;
        }
    }

    if (primask == 0u) { __enable_irq(); }

    /* ---- 체크섬 ---- */
    uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (sum != data[4])
    {
        return -3;
    }

    /* ---- 변환 (raw가 이미 ×10 단위) ---- */
    *humi_x10 = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);

    int16_t t = (int16_t)((((uint16_t)data[2] & 0x7Fu) << 8) | data[3]);
    if (data[2] & 0x80u)     /* bit7=1 → 음수 */
    {
        t = -t;
    }
    *temp_x10 = t;

    return 0;
}

/* ==========================================================================
 * 공개 API
 * ========================================================================== */

void FW_Sensor_Init(void)
{
    /* DWT 사이클 카운터 활성화 (delay_us / dht_wait_level 용) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT       = 0u;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    /* BH1750: One-Time 모드는 매 읽기마다 명령 전송 → 별도 init 없음 */
}

/* --------------------------------------------------------------------------
 * DHT22 — 2000ms 주기
 * -------------------------------------------------------------------------- */
void FW_DHT22_Update(void)
{
    uint16_t humi;
    int16_t  temp;

    if ((DHT22_Read(&humi, &temp) == 0) &&
        (humi <= DHT_HUMI_MAX_X10) &&
        (temp >= DHT_TEMP_MIN_X10) &&
        (temp <= DHT_TEMP_MAX_X10))
    {
        g_fw.humidity_x10    = humi;
        g_fw.temperature_x10 = temp;
        g_fw.temp_humi_valid = 1u;
    }
    else
    {
        /* 실패: 이전 값 유지 */
        g_fw.temp_humi_valid = 0u;
    }
}

/* --------------------------------------------------------------------------
 * BH1750 — 500ms 주기, 비블로킹 2단계 상태머신
 *
 *   호출 N   : One-Time H-Res(0x20) 측정 명령 전송
 *   호출 N+1 : 결과 2바이트 읽기 (500ms 경과 > 변환시간 max 180ms 충족)
 *
 *   → 블로킹 delay 없음. 값 갱신 주기는 1000ms.
 * -------------------------------------------------------------------------- */
void FW_BH1750_Update(void)
{
    static uint8_t bh_state = 0u;   /* 0=측정 명령 전송, 1=결과 읽기 */

    if (bh_state == 0u)
    {
        uint8_t cmd = BH1750_CMD_ONE_TIME_H;

        if (BSP_I2C_Write(BH1750_ADDR_7BIT, &cmd, 1u) == BSP_OK)
        {
            bh_state = 1u;          /* 다음 호출에서 읽기 */
        }
        else
        {
            g_fw.lux_valid = 0u;    /* 명령 실패: 이전 값 유지, invalid */
        }
    }
    else
    {
        uint8_t rx[2];

        bh_state = 0u;              /* 성공/실패 무관 다음엔 재측정 */

        if (BSP_I2C_Read(BH1750_ADDR_7BIT, rx, 2u) == BSP_OK)
        {
            uint32_t raw = ((uint32_t)rx[0] << 8) | rx[1];
            g_fw.lux_x10   = (raw * 100u) / 12u;   /* ÷1.2 = ×10/12, 정수 연산 */
            g_fw.lux_valid = 1u;
        }
        else
        {
            g_fw.lux_valid = 0u;
        }
    }
}

/* --------------------------------------------------------------------------
 * PMS7003 — 2000ms 주기, 비블로킹 스트림 파서
 *
 *   BSP UART RX 인터럽트 링버퍼 → 누적 버퍼 → 헤더(0x42 0x4D) 탐색 재동기
 *   → 길이/체크섬 검증 → 최신 유효 프레임 반영.
 *
 *   프레임 중간부터 수신을 시작해도 다음 헤더에서 자동 재동기됨.
 *   연속 PMS_MISS_LIMIT회(약 6s) 유효 프레임이 없으면 invalid 처리.
 *
 *   frame[0]     0x42 ('B')
 *   frame[1]     0x4D ('M')
 *   frame[2~3]   프레임 길이 = 28 (BE)
 *   frame[10~11] PM1.0 atm (BE)
 *   frame[12~13] PM2.5 atm (BE)
 *   frame[14~15] PM10  atm (BE)
 *   frame[30~31] 체크섬 = frame[0~29] 합 (BE)
 * -------------------------------------------------------------------------- */
void FW_PMS7003_Update(void)
{
    static uint8_t  acc[PMS_ACC_LEN];
    static uint16_t acc_len = 0u;
    static uint8_t  miss    = 0u;

    uint8_t found = 0u;

    /* ---- 링버퍼에서 신규 수신분 회수 (비블로킹) ---- */
    acc_len += BSP_UART_DustRead(&acc[acc_len], (uint16_t)(PMS_ACC_LEN - acc_len));

    /* ---- 프레임 탐색: 헤더 재동기 + 검증. 유효 프레임은 모두 파싱(최신 우선) ---- */
    uint16_t i = 0u;
    while ((uint16_t)(acc_len - i) >= PMS_FRAME_LEN)
    {
        const uint8_t *f = &acc[i];

        if (f[0] == PMS_START1 && f[1] == PMS_START2)
        {
            uint16_t body_len = (uint16_t)(((uint16_t)f[2] << 8) | f[3]);

            if (body_len == PMS_BODY_LEN)
            {
                uint16_t sum = 0u;
                for (uint8_t k = 0u; k < 30u; k++)
                {
                    sum += f[k];
                }
                uint16_t chksum = (uint16_t)(((uint16_t)f[30] << 8) | f[31]);

                if (sum == chksum)
                {
                    /* 유효 프레임 — atm 기준값 파싱 (big-endian) */
                    g_fw.pm1_0_atm = (uint16_t)(((uint16_t)f[10] << 8) | f[11]);
                    g_fw.pm2_5_atm = (uint16_t)(((uint16_t)f[12] << 8) | f[13]);
                    g_fw.pm10_atm  = (uint16_t)(((uint16_t)f[14] << 8) | f[15]);
                    found = 1u;

                    i += PMS_FRAME_LEN;   /* 프레임 통째로 소비 */
                    continue;
                }
            }
        }

        i++;   /* 헤더 불일치/검증 실패 → 1바이트 밀며 재동기 */
    }

    /* ---- 미소비 잔여분(부분 프레임)을 버퍼 앞으로 이동 ---- */
    if (i > 0u)
    {
        memmove(acc, &acc[i], (size_t)(acc_len - i));
        acc_len = (uint16_t)(acc_len - i);
    }

    /* ---- valid 판정 ---- */
    if (found)
    {
        g_fw.dust_valid = 1u;
        miss = 0u;
    }
    else
    {
        if (miss < PMS_MISS_LIMIT)
        {
            miss++;
        }
        if (miss >= PMS_MISS_LIMIT)
        {
            g_fw.dust_valid = 0u;   /* 약 6s 이상 유효 프레임 없음 */
        }
    }
}
