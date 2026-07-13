/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "string.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ws2812_spi.h"
#include "window.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  IGNITION_OFF = 0,
  DRIVER_AUTH,
  WAIT_IGNITION,
  OPERATIONAL,
  /* [MOD] 이름 정리 (요구사항 8): 기존 MONITORING은 실제로 "RPi4가 잔류 감지를
     확정해 0x083 op=OP_ALARM(2)을 내린 상태"를 의미했다. 감시(모니터링)와
     경보(알람)를 혼동하지 않도록 RESIDUAL_ALARM으로 개명. 열거값(4)은 유지. */
  RESIDUAL_ALARM,
  ALARM_STATE,   /* (reserved — 사용처 없음, 열거값 유지 목적) */
  /* RPi4가 승하차(BOARDING) 상태를 통지할 때 사용. OPERATIONAL이 아니므로
     이 상태에서는 안전벨트 해제가 부저 경고로 이어지지 않는다. (요구사항 2-4/시나리오3)
     맨 뒤에 추가하여 기존 열거값(0~5)은 그대로 유지한다. */
  STOPPED_BOARDING
} SystemState_t;

typedef struct
{
  uint8_t pir;
  uint8_t button;
  uint8_t belt1;
  uint8_t belt2;
} SafetyInputMsg_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

ETH_TxPacketConfig TxConfig;
ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

CAN_HandleTypeDef hcan1;

ETH_HandleTypeDef heth;

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

UART_HandleTypeDef huart3;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* Definitions for SafetyInputTask */
osThreadId_t SafetyInputTaskHandle;
const osThreadAttr_t SafetyInputTask_attributes = {
  .name = "SafetyInputTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for AlarmTask */
osThreadId_t AlarmTaskHandle;
const osThreadAttr_t AlarmTask_attributes = {
  .name = "AlarmTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for SafetyLogicTask */
osThreadId_t SafetyLogicTaskHandle;
const osThreadAttr_t SafetyLogicTask_attributes = {
  .name = "SafetyLogicTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for CanTask */
osThreadId_t CanTaskHandle;
const osThreadAttr_t CanTask_attributes = {
  .name = "CanTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */


/* Queue handle */
osMessageQueueId_t safetyInputQueueHandle;
osEventFlagsId_t safetyEventHandle;
SystemState_t current_state = IGNITION_OFF;

/* Debug용 Live Expression 변수 */
volatile uint8_t dbg_pir = 0;
volatile uint8_t dbg_button = 0;
volatile uint8_t dbg_belt1 = 0;
volatile uint8_t dbg_belt2 = 0;
volatile uint8_t can_button_event = 0;
volatile uint32_t dbg_button_event_until_tick = 0;

uint8_t window_opened = 0;
uint8_t button_logic_prev = 0;
uint8_t child_alarm_active = 0;
uint8_t ignition_state = 0; // 나중에 RPI4가 CAN으로 보내줄 값
uint32_t button_event_until_tick = 0;

volatile uint8_t can_window_cmd = 0;
volatile uint8_t can_ignition_cmd = 0;
volatile uint8_t can_operation_cmd = 0;
volatile uint8_t can_led_cmd = 0;
volatile uint8_t can_buzzer_cmd = 0;

volatile uint8_t can_window_cmd_updated = 0;
volatile uint8_t can_ignition_cmd_updated = 0;
volatile uint8_t can_led_cmd_updated = 0;
volatile uint8_t can_buzzer_cmd_updated = 0;

volatile uint32_t dbg_can_rx_count = 0;
volatile uint32_t dbg_can_rx_id = 0;
volatile uint8_t dbg_can_rx_dlc = 0;
volatile uint8_t dbg_can_rx_data0 = 0;
volatile uint8_t dbg_can_rx_data1 = 0;
volatile uint32_t dbg_can_rx_083_count = 0;
volatile uint8_t dbg_rx_ignition = 0;
volatile uint8_t dbg_rx_operation = 0;


#define EVT_CHILD_DETECTED   (1 << 0)
#define EVT_SEATBELT_WARNING  (1 << 2)


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ETH_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
static void MX_CAN1_Init(void);
void StartSafetyInputTask(void *argument);
void StartAlarmTask(void *argument);
void StartSafetyLogicTask(void *argument);
void StartCanTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define CAN_ID_POWER_BUTTON     0x012
#define CAN_ID_PIR_STATE        0x022
#define CAN_ID_SEATBELT_STATE   0x032

#define CAN_ID_WINDOW_TARGET    0x063
#define CAN_ID_IGNITION_STATE   0x083
#define CAN_ID_EXT_LED_CMD      0x0A3
#define CAN_ID_BUZZER_TARGET    0x0C3

void CAN_Filter_Config(void)
{
  CAN_FilterTypeDef sFilterConfig = {0};

  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

  /* 모든 Standard ID 수신 허용 */
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;

  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ETH_Init();
  MX_SPI1_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
  CAN_Filter_Config();

  if (HAL_CAN_Start(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    Error_Handler();
  }

  WS2812_SPI_Init(&hspi1);

  WS2812_SetAll(0, 0, 0);   // 시작 시 OFF
  WS2812_Refresh();
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */

  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  safetyInputQueueHandle = osMessageQueueNew(8, sizeof(SafetyInputMsg_t), NULL);

  if (safetyInputQueueHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_QUEUES */

  /* USER CODE BEGIN RTOS_EVENTS */
  safetyEventHandle = osEventFlagsNew(NULL);

  if (safetyEventHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_EVENTS */

  /* Create the thread(s) */
  /* creation of SafetyInputTask */
  SafetyInputTaskHandle = osThreadNew(StartSafetyInputTask, NULL, &SafetyInputTask_attributes);

  /* creation of AlarmTask */
  AlarmTaskHandle = osThreadNew(StartAlarmTask, NULL, &AlarmTask_attributes);

  /* creation of SafetyLogicTask */
  SafetyLogicTaskHandle = osThreadNew(StartSafetyLogicTask, NULL, &SafetyLogicTask_attributes);

  /* creation of CanTask */
  CanTaskHandle = osThreadNew(StartCanTask, NULL, &CanTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 6;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  /* 부팅 순서 문제(코드 관점) 완화:
     - AutoBusOff=ENABLE : STM32가 RPi4보다 먼저 켜져 ACK 없이 송신 → TX 에러 누적으로
       bus-off에 빠져도, 자동으로 bus-off에서 복구한다. (예전 DISABLE에서는 한번 bus-off되면
       재초기화 전까지 통신 불가 → "RPi를 먼저 켜야 통신" 증상)
     - [MOD] AutoRetransmission=DISABLE 로 원복.
       ENABLE 상태에서 버스에 ACK 상대가 없으면 실패 프레임이 TX mailbox 3개를
       모두 점유한 채 무한 재전송 → GetTxMailboxesFreeLevel()==0 → 이후 모든
       송신이 막히는 완전 통신 불능이 실측으로 확인됨.
       ACK 유실 복구는 AutoBusOff=ENABLE + 100ms 주기 재송신 구조로 충분하다. */
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief ETH Initialization Function
  * @param None
  * @retval None
  */
static void MX_ETH_Init(void)
{

  /* USER CODE BEGIN ETH_Init 0 */

  /* USER CODE END ETH_Init 0 */

   static uint8_t MACAddr[6];

  /* USER CODE BEGIN ETH_Init 1 */

  /* USER CODE END ETH_Init 1 */
  heth.Instance = ETH;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
  heth.Init.TxDesc = DMATxDscrTab;
  heth.Init.RxDesc = DMARxDscrTab;
  heth.Init.RxBuffLen = 1524;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }

  memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfig));
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
  /* USER CODE BEGIN ETH_Init 2 */

  /* USER CODE END ETH_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 4;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */

  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD1_Pin|LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : USER_Btn_Pin */
  GPIO_InitStruct.Pin = USER_Btn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PF0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD1_Pin|LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PE9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_OverCurrent_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PC6 PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : child_belt1_Pin child_belt2_Pin */
  GPIO_InitStruct.Pin = child_belt1_Pin|child_belt2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PE0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void ChildAlarm_Clear(void)
{
    child_alarm_active = 0;

    osEventFlagsClear(safetyEventHandle, EVT_CHILD_DETECTED);

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);

    WS2812_SetAll(0, 0, 0);
    WS2812_Refresh();
}
void CAN_Command_Process(void)
{
    if (can_window_cmd_updated)
    {
        can_window_cmd_updated = 0;

        /*
         * 0x063 data[0] = 목표 개방률(%).
         *  >0 : 잔류 경보 — 창문을 잠깐 열어 환기 (300ms 구동 후 정지)
         *  [MOD] 0 : 경보 해제 — 이전에 열었던 경우 원위치(닫힘)로 복귀.
         *        기존 코드는 0에서 Stop만 수행해 창문이 열린 채 방치되었다.
         */
        if (can_window_cmd > 0)
        {
            Window_Open();
            osDelay(300);
            Window_Stop();
            window_opened = 1;
        }
        else
        {
            if (window_opened != 0)
            {
                Window_Close();
                osDelay(300);
                window_opened = 0;
            }
            Window_Stop();
        }
    }

    if (can_ignition_cmd_updated)
    {
        can_ignition_cmd_updated = 0;

        ignition_state = can_ignition_cmd;
        child_alarm_active = 0;
        osEventFlagsClear(safetyEventHandle,
                          EVT_CHILD_DETECTED | EVT_SEATBELT_WARNING);

        if (can_operation_cmd == 0)
        {
            if (can_ignition_cmd == 1)
            {
                current_state = WAIT_IGNITION;
            }
            else
            {
                current_state = IGNITION_OFF;
                /* [MOD] window_opened 리셋 제거.
                   RPi4의 경보 해제 시퀀스는 0x083(op=0) → 0x063(0%) 순서로
                   도착한다. 여기서 플래그를 지우면 뒤따라오는 창문 닫힘 명령이
                   "이미 닫힘"으로 오판되어 창문이 열린 채 남는다.
                   창문 상태 추적은 0x063 처리부에서만 갱신한다. */
            }
        }
        else if (can_operation_cmd == 1)
        {
            current_state = OPERATIONAL;
        }
        else if (can_operation_cmd == 2)
        {
            /* [MOD] op=2 = OP_ALARM : RPi4가 잔류 감지를 확정한 상태.
               부저/외부LED/창문은 별도 명령(0x0C3/0x0A3/0x063)으로 내려온다.
               RTOS는 상태 추종만 하고 스스로 경보를 시작하지 않는다. */
            current_state = RESIDUAL_ALARM;
        }
        else if (can_operation_cmd == 4)
        {
            /* op=4 = OP_BOARDING : 승하차 중 (RPi4 enter_passenger_transfer) */
            current_state = STOPPED_BOARDING;
            osEventFlagsClear(safetyEventHandle, EVT_SEATBELT_WARNING);
        }
    }
    if (can_led_cmd_updated)
    {
        can_led_cmd_updated = 0;

        if (can_led_cmd == 0)
        {
            WS2812_SetAll(0, 0, 0);
        }
        else if (can_led_cmd == 1)
        {
            WS2812_SetAll(255, 0, 0);
        }
        else if (can_led_cmd == 2)
        {
            WS2812_SetAll(0, 255, 0);
        }
        else if (can_led_cmd == 3)
        {
            WS2812_SetAll(0, 0, 255);
        }

        WS2812_Refresh();
    }

    if (can_buzzer_cmd_updated)
    {
        can_buzzer_cmd_updated = 0;

        if (can_buzzer_cmd == 0)
        {
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
        }
    }
}
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8];

    if (hcan->Instance != CAN1)
    {
        return;
    }

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) != HAL_OK)
    {
        return;
    }

    if (rxHeader.IDE != CAN_ID_STD)
    {
        return;
    }

    dbg_can_rx_count++;
    dbg_can_rx_id = rxHeader.StdId;
    dbg_can_rx_dlc = rxHeader.DLC;
    dbg_can_rx_data0 = (rxHeader.DLC >= 1U) ? rxData[0] : 0U;
    dbg_can_rx_data1 = (rxHeader.DLC >= 2U) ? rxData[1] : 0U;

    switch (rxHeader.StdId)
    {
        case CAN_ID_WINDOW_TARGET:
            if (rxHeader.DLC >= 1)
            {
                can_window_cmd = rxData[0];
                can_window_cmd_updated = 1;
            }
            break;

        case CAN_ID_IGNITION_STATE:
            if (rxHeader.DLC >= 2)
            {
                can_ignition_cmd = rxData[0];
                can_operation_cmd = rxData[1];
                can_ignition_cmd_updated = 1;
                dbg_can_rx_083_count++;
                dbg_rx_ignition = rxData[0];
                dbg_rx_operation = rxData[1];
            }
            break;

        case CAN_ID_EXT_LED_CMD:
            if (rxHeader.DLC >= 1)
            {
                can_led_cmd = rxData[0];
                can_led_cmd_updated = 1;
            }
            break;

        case CAN_ID_BUZZER_TARGET:
            if (rxHeader.DLC >= 1)
            {
                can_buzzer_cmd = rxData[0];
                can_buzzer_cmd_updated = 1;
            }
            break;

        default:
            break;
    }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartSafetyInputTask */
/**
  * @brief  Function implementing the SafetyInputTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSafetyInputTask */
void StartSafetyInputTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */

	 SafetyInputMsg_t msg;
	 uint8_t pir_raw;
	 uint8_t pir_stable = 0;
	 uint8_t pir_high_count = 0;
	 uint8_t pir_low_count = 0;

	 uint8_t button_raw;
	 uint8_t button_stable = 0;
	 uint8_t button_high_count = 0;
	 uint8_t button_low_count = 0;

	 for (;;)
	   {
	     memset(&msg, 0, sizeof(msg));

	     pir_raw = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_0);

	     if (pir_raw == GPIO_PIN_SET)
	     {
	       pir_high_count++;
	       pir_low_count = 0;

	       if (pir_high_count >= 2)
	       {
	         pir_stable = 1;
	         pir_high_count = 2;
	       }
	     }
	     else
	     {
	       pir_low_count++;
	       pir_high_count = 0;

	       if (pir_low_count >= 1)
	       {
	         pir_stable = 0;
	         pir_low_count = 1;
	       }
	     }

	     msg.pir = pir_stable;

	     button_raw = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

	     if (button_raw == GPIO_PIN_SET)
	     {
	       button_high_count++;
	       button_low_count = 0;

	       if (button_high_count >= 2)
	       {
	         button_stable = GPIO_PIN_SET;
	         button_high_count = 2;
	       }
	     }
	     else
	     {
	       button_low_count++;
	       button_high_count = 0;

	       if (button_low_count >= 2)
	       {
	         button_stable = GPIO_PIN_RESET;
	         button_low_count = 2;
	       }
	     }

	     msg.button = button_stable;
	     msg.belt1  = HAL_GPIO_ReadPin(child_belt1_GPIO_Port, child_belt1_Pin);
	     msg.belt2  = HAL_GPIO_ReadPin(child_belt2_GPIO_Port, child_belt2_Pin);

	     /* Live Expression 확인용 */
	     dbg_pir = msg.pir;
	     dbg_button = msg.button;
	     dbg_belt1 = msg.belt1;
	     dbg_belt2 = msg.belt2;

	     if (osMessageQueuePut(safetyInputQueueHandle, &msg, 0, 10) != osOK)
	     {
	       /* 큐가 꽉 찼으면 이번 입력은 버림 */
	     }

	     osDelay(20);
	   }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartAlarmTask */
/**
* @brief Function implementing the AlarmTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartAlarmTask */
void StartAlarmTask(void *argument)
{
  /* USER CODE BEGIN StartAlarmTask */
  for (;;)
  {
    osDelay(100);
  }
  /* USER CODE END StartAlarmTask */
}

/* USER CODE BEGIN Header_StartSafetyLogicTask */
/**
* @brief Function implementing the SafetyLogicTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSafetyLogicTask */
void StartSafetyLogicTask(void *argument)
{
  /* USER CODE BEGIN StartSafetyLogicTask */
  /* Infinite loop */
	  SafetyInputMsg_t msg;

	  for (;;)
	  {
		if (osMessageQueueGet(safetyInputQueueHandle,
							  &msg,
							  NULL,
							  osWaitForever) == osOK)
		{
		  if (msg.button == GPIO_PIN_SET &&
			  button_logic_prev == GPIO_PIN_RESET)
		  {
			/* RPi4 owns the state machine. RTOS only reports the button event. */
			can_button_event = 1;
			button_event_until_tick = osKernelGetTickCount() + 1000U;
			dbg_button_event_until_tick = button_event_until_tick;
		  }

		  button_logic_prev = msg.button;
		}
	}
  /* USER CODE END StartSafetyLogicTask */
}

/* USER CODE BEGIN Header_StartCanTask */
/**
* @brief Function implementing the CanTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCanTask */

void StartCanTask(void *argument)
{
  /* USER CODE BEGIN StartCanTask */
  /* Infinite loop */

  CAN_TxHeaderTypeDef txHeader;
  uint8_t txData[8];
  uint32_t txMailbox;
  uint32_t lastTxTick = 0;
  uint32_t nowTick;

  txHeader.IDE = CAN_ID_STD;
  txHeader.RTR = CAN_RTR_DATA;
  txHeader.DLC = 1;
  txHeader.TransmitGlobalTime = DISABLE;

  for(;;)
  {
    CAN_Command_Process();

    nowTick = osKernelGetTickCount();

    if (can_button_event != 0U &&
        (int32_t)(nowTick - button_event_until_tick) >= 0)
    {
      can_button_event = 0;
    }

    if (nowTick - lastTxTick >= 100)
    {
      lastTxTick = nowTick;

      /* 0x012: 전원 버튼 상태 */
      txHeader.StdId = CAN_ID_POWER_BUTTON;
      txHeader.DLC = 1;
      txData[0] = ((can_button_event != 0U) || (dbg_button != 0U)) ? 1U : 0U;

      if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
      {
        HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &txMailbox);
      }

      osDelay(10);

      /* 0x022: PIR 감지값 */
      txHeader.StdId = CAN_ID_PIR_STATE;
      txHeader.DLC = 1;
      txData[0] = dbg_pir;

      if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
      {
        HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &txMailbox);
      }

      /* 0x032: 안전벨트 상태 */
      txHeader.StdId = CAN_ID_SEATBELT_STATE;
      txHeader.DLC = 2;
      txData[0] = dbg_belt1;
      txData[1] = dbg_belt2;

      if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
      {
        HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &txMailbox);
      }
    }

    osDelay(10);
  }
  /* USER CODE END StartCanTask */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
