#include "dht11.h"
#include "dwt_timer.h"
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"
#include "stdio.h"

volatile uint8_t dht11_buf[5];

uint8_t dht11_temperature = 0;
uint8_t dht11_humidity = 0;

void DHT11_handler(void)
{
  
  volatile uint8_t res = 0;
  res = read_DHT11((uint8_t*)dht11_buf);
  
  if (res==DHT11_OK)
  {
    dht11_temperature = dht11_buf[2];
    dht11_humidity = dht11_buf[0];
#ifdef DEBUG
  printf("RH=%02d%% t=%dC  \r\n", dht11_humidity, dht11_temperature);
#endif
  }
  else
  {
    dht11_temperature = 0;
    dht11_humidity = 0;
#ifdef DEBUG
    printf("DHT11 error\r\n");
#endif    
  }
}

//возвращает время до фронта или таймаута
uint16_t dht11_wait_for_rising_edge(void)
{
  int32_t start_time = DWT_Get();
  int32_t timeout = start_time + 100 * (SystemCoreClock/1000000);//timeout time
  while (DWT_Compare(timeout) && (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)){}//если текущее время меньше заданного и низкий уровень
  return ((DWT_Get() - start_time) / (SystemCoreClock/1000000));
}

//возвращает время до фронта или таймаута
uint16_t dht11_wait_for_falling_edge(void)
{
  int32_t start_time = DWT_Get();
  int32_t timeout = start_time + 100 * (SystemCoreClock/1000000);//timeout time
  while (DWT_Compare(timeout) && (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)){}//если текущее время меньше заданного и низкий уровень
  return ((DWT_Get() - start_time) / (SystemCoreClock/1000000));
}

uint8_t dht11_wait_for_bit(void)
{
  static uint16_t time_cnt = 0;
  dht11_wait_for_rising_edge();//всегда около 50 мкс
  time_cnt = dht11_wait_for_falling_edge();
  
  if (time_cnt < (uint16_t)40) 
  {return 0;}
  else {return 1;}
}


uint8_t read_DHT11(uint8_t *buf)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  uint16_t time_cnt;
  uint8_t i, check_sum; 
  uint8_t rx_bit;
  uint8_t position = 0;
  
  GPIO_InitStruct.Pin =  DHT11_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_MEDIUM;
  HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
  
  //reset DHT11
  taskENTER_CRITICAL();
  DWT_Delay(60000);
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
  DWT_Delay(20000);//low level
  
  
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
  DWT_Delay(30);//us
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
  DWT_Delay(30);//us
  
  time_cnt = 0;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
  //2 presence pulses
  time_cnt = dht11_wait_for_rising_edge();
  if ((time_cnt > 95) || (time_cnt < 10)) return DHT11_NO_CONN;
  time_cnt = dht11_wait_for_falling_edge();
  if ((time_cnt > 95) || (time_cnt < 10)) return DHT11_NO_CONN;
  
  //start reading	
  
  for (i = 0; i<5; i++) buf[i]=0;//clean
  
  for(i=0;i<40;i++)
  {
    rx_bit = dht11_wait_for_bit();
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
    
    position = i/8;
    buf[position] = buf[position] << 1;
    buf[position]|= rx_bit;
  }
  taskEXIT_CRITICAL();
  
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);//release line
  
  //calculate checksum
  check_sum = 0;
  for(i=0;i<4;i++)
  {
    check_sum += buf[i];
  }
  
  if (buf[4] != check_sum) return DHT11_CS_ERROR;
  return DHT11_OK;
}
