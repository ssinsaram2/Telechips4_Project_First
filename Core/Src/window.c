#include "window.h"

void Window_Open(void)
{
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
}

/* [MOD] 닫힘 방향 구동: Open과 반대 극성 (PC6=SET, PC7=RESET).
 * 호출 측(CAN_Command_Process)에서 300ms 구동 후 Window_Stop()으로 정지한다. */
void Window_Close(void)
{
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
}

void Window_Stop(void)
{
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
}
