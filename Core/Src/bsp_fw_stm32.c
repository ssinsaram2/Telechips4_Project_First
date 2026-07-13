/**
 ******************************************************************************
 * @file    bsp_fw_stm32.c
 * @brief   STM32F429ZI용 BSP 구현
 ******************************************************************************
 */
#include "bsp_fw.h"
#include "main.h"

extern I2C_HandleTypeDef  hi2c1;
extern UART_HandleTypeDef huart4;
extern CAN_HandleTypeDef  hcan1;
extern TIM_HandleTypeDef  htim3;
extern TIM_HandleTypeDef  htim4;

#ifndef SEAT_IN1_Pin
#define SEAT_IN1_Pin       GPIO_PIN_0
#define SEAT_IN1_GPIO_Port GPIOE
#endif

#ifndef SEAT_IN2_Pin
#define SEAT_IN2_Pin       GPIO_PIN_2
#define SEAT_IN2_GPIO_Port GPIOE
#endif

#define BSP_I2C_TIMEOUT_MS       100u
#define DUST_RX_BUFFER_SIZE      256u
#define DUST_RX_BUFFER_MASK      (DUST_RX_BUFFER_SIZE - 1u)

static volatile uint8_t  dust_rx_buffer[DUST_RX_BUFFER_SIZE];
static volatile uint16_t dust_rx_head = 0u;
static volatile uint16_t dust_rx_tail = 0u;
static volatile uint8_t  dust_rx_started = 0u;
static uint8_t           dust_rx_byte = 0u;

static BSP_Status_t bsp_status_from_hal(HAL_StatusTypeDef status)
{
    switch (status)
    {
    case HAL_OK:      return BSP_OK;
    case HAL_BUSY:    return BSP_BUSY;
    case HAL_TIMEOUT: return BSP_TIMEOUT;
    case HAL_ERROR:
    default:          return BSP_ERROR;
    }
}

static void dust_rx_start(void)
{
    if (dust_rx_started == 0u)
    {
        if (HAL_UART_Receive_IT(&huart4, &dust_rx_byte, 1u) == HAL_OK)
        {
            dust_rx_started = 1u;
        }
    }
}

uint32_t BSP_GetTickMs(void)
{
    return HAL_GetTick();
}

void BSP_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

BSP_Status_t BSP_I2C_Write(uint8_t addr_7bit,
                           const uint8_t *data,
                           uint16_t len)
{
    if ((data == NULL) || (len == 0u))
    {
        return BSP_ERROR;
    }

    return bsp_status_from_hal(HAL_I2C_Master_Transmit(
        &hi2c1, (uint16_t)addr_7bit << 1, (uint8_t *)data, len,
        BSP_I2C_TIMEOUT_MS));
}

BSP_Status_t BSP_I2C_Read(uint8_t addr_7bit, uint8_t *buf, uint16_t len)
{
    if ((buf == NULL) || (len == 0u))
    {
        return BSP_ERROR;
    }

    return bsp_status_from_hal(HAL_I2C_Master_Receive(
        &hi2c1, (uint16_t)addr_7bit << 1, buf, len, BSP_I2C_TIMEOUT_MS));
}

BSP_Status_t BSP_UART_Receive(uint8_t *buf,
                             uint16_t len,
                             uint32_t timeout_ms)
{
    if ((buf == NULL) || (len == 0u))
    {
        return BSP_ERROR;
    }

    return bsp_status_from_hal(
        HAL_UART_Receive(&huart4, buf, len, timeout_ms));
}

uint16_t BSP_UART_DustRead(uint8_t *buf, uint16_t maxlen)
{
    uint16_t count = 0u;

    if ((buf == NULL) || (maxlen == 0u))
    {
        dust_rx_start();
        return 0u;
    }

    dust_rx_start();

    while ((count < maxlen) && (dust_rx_tail != dust_rx_head))
    {
        buf[count] = dust_rx_buffer[dust_rx_tail];
        dust_rx_tail = (uint16_t)((dust_rx_tail + 1u) & DUST_RX_BUFFER_MASK);
        count++;
    }

    return count;
}

BSP_Status_t BSP_CAN_Send(uint32_t id,
                         const uint8_t *data,
                         uint8_t dlc)
{
    CAN_TxHeaderTypeDef header = {0};
    uint32_t mailbox;

    if ((id > 0x7FFu) || (data == NULL) || (dlc > 8u))
    {
        return BSP_ERROR;
    }

    header.StdId = id;
    header.ExtId = 0u;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = dlc;
    header.TransmitGlobalTime = DISABLE;

    return bsp_status_from_hal(
        HAL_CAN_AddTxMessage(&hcan1, &header, (uint8_t *)data, &mailbox));
}

void BSP_PWM_Fan_SetDuty(uint8_t percent)
{
    uint32_t arr;
    uint32_t compare;

    if (percent > 100u)
    {
        percent = 100u;
    }

    arr = __HAL_TIM_GET_AUTORELOAD(&htim3);
    compare = ((arr + 1u) * percent) / 100u;
    if (compare > arr)
    {
        compare = arr;
    }

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, compare);
}

static void seat_set_duty(uint8_t percent)
{
    uint32_t arr;
    uint32_t compare;

    if (percent > 100u)
    {
        percent = 100u;
    }

    arr = __HAL_TIM_GET_AUTORELOAD(&htim4);
    compare = ((arr + 1u) * percent) / 100u;
    if (compare > arr)
    {
        compare = arr;
    }

    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, compare);
}

void BSP_Seat_Forward(uint8_t duty_percent)
{
    if (duty_percent == 0u)
    {
        BSP_Seat_Stop();
        return;
    }

    HAL_GPIO_WritePin(SEAT_IN1_GPIO_Port, SEAT_IN1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SEAT_IN2_GPIO_Port, SEAT_IN2_Pin, GPIO_PIN_RESET);
    seat_set_duty(duty_percent);
}

void BSP_Seat_Backward(uint8_t duty_percent)
{
    if (duty_percent == 0u)
    {
        BSP_Seat_Stop();
        return;
    }

    HAL_GPIO_WritePin(SEAT_IN1_GPIO_Port, SEAT_IN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEAT_IN2_GPIO_Port, SEAT_IN2_Pin, GPIO_PIN_SET);
    seat_set_duty(duty_percent);
}

void BSP_Seat_Stop(void)
{
    seat_set_duty(0u);
    HAL_GPIO_WritePin(SEAT_IN1_GPIO_Port, SEAT_IN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEAT_IN2_GPIO_Port, SEAT_IN2_Pin, GPIO_PIN_RESET);
}

void BSP_GPIO_LED_Cabin_Write(uint8_t state)
{
    HAL_GPIO_WritePin(CABIN_LED_GPIO_Port, CABIN_LED_Pin,
                      (state != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint16_t next;

    if (huart->Instance != UART4)
    {
        return;
    }

    next = (uint16_t)((dust_rx_head + 1u) & DUST_RX_BUFFER_MASK);
    if (next != dust_rx_tail)
    {
        dust_rx_buffer[dust_rx_head] = dust_rx_byte;
        dust_rx_head = next;
    }

    if (HAL_UART_Receive_IT(&huart4, &dust_rx_byte, 1u) != HAL_OK)
    {
        dust_rx_started = 0u;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART4)
    {
        dust_rx_started = 0u;
        __HAL_UART_CLEAR_OREFLAG(huart);
        dust_rx_start();
    }
}
