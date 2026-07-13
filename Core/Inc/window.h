#ifndef INC_WINDOW_H_
#define INC_WINDOW_H_

#include "main.h"

void Window_Open(void);
/* [MOD] 잔류 경보 해제 시 창문 원위치(닫힘) 복귀용 — GPIO H-브리지 역방향 구동 */
void Window_Close(void);
void Window_Stop(void);

#endif
