#ifndef __WS2812_SPI_H_
#define __WS2812_SPI_H_

#include "stm32f4xx_hal.h"


/* Exported types ------------------------------------------------------------*/

/* Private defines -----------------------------------------------------------*/
#define WS2812_NUM_LEDS 80
#define WS2812_RESET_PULSE 60
#define WS2812_BUFFER_SIZE (WS2812_NUM_LEDS * 24 + WS2812_RESET_PULSE)
/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void WS2812_SPI_Init(SPI_HandleTypeDef *hspi);
void WS2812_Set(uint16_t led_no, uint8_t r, uint8_t g, uint8_t b);
void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b);
void WS2812_Refresh(void);
#endif
