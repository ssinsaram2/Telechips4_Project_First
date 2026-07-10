/*
 * ws2812_spi.c
 *
 *  Created on: 2026. 6. 26.
 *      Author: 한국전파진흥협회
 */


// #include "mcb_drv_ws2812_spi.h"
#include "ws2812_spi.h"

#define WS2812_FILL_BUFFER(COLOR) \
for( uint8_t mask = 0x80; mask; mask >>= 1 ) { \
	if( COLOR & mask ) { \
		*ptr++ = 0xF8; \
	} else { \
		*ptr++ = 0xC0; \
	} \
}
static SPI_HandleTypeDef *m_Hspi;

uint8_t ws2812_buffer[WS2812_BUFFER_SIZE];

void WS2812_SPI_Init(SPI_HandleTypeDef *hspi)
{
 	m_Hspi = hspi;
	HAL_SPI_Transmit_DMA(m_Hspi, ws2812_buffer, WS2812_BUFFER_SIZE);
}

void WS2812_Set(uint16_t led_no, uint8_t r, uint8_t g, uint8_t b) {
	if(led_no > WS2812_NUM_LEDS-1) return;
    uint8_t * ptr = &ws2812_buffer[24 * led_no];
    WS2812_FILL_BUFFER(g);
    WS2812_FILL_BUFFER(r);
    WS2812_FILL_BUFFER(b);
}

void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t * ptr = ws2812_buffer;
    for( uint16_t i = 0; i < WS2812_NUM_LEDS; i++) {
        WS2812_FILL_BUFFER(g);
        WS2812_FILL_BUFFER(r);
        WS2812_FILL_BUFFER(b);
    }
}

void WS2812_Refresh(void)
{
	HAL_SPI_Transmit_DMA(m_Hspi, ws2812_buffer, WS2812_BUFFER_SIZE);
}
