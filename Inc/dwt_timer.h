#ifndef _DWT_TIMER
#define _DWT_TIMER

void DWT_Init(void);
uint32_t DWT_Get(void);
uint8_t DWT_Compare(int32_t tp);
void DWT_Delay(uint32_t us); // microseconds

#endif