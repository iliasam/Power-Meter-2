#include "stm32f1xx_hal.h"

void DWT_Init(void)
{
  //if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk))
  if (1)
  {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  }
}

uint32_t DWT_Get(void)
{
  return DWT->CYCCNT;
}

//возвращает 1, если текущее время меньше заданного
uint8_t DWT_Compare(int32_t tp)
{
  return (((int32_t)DWT_Get() - tp) < 0);
}

void DWT_Delay(uint32_t us) // microseconds
{
  int32_t tp = DWT_Get() + us * (SystemCoreClock/1000000);
  while (DWT_Compare(tp));
}