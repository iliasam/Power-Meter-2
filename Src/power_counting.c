#include "power_counting.h"
#include "rtc_functions.h"

#define TWO_MINUTES_POINTS_CNT 12

uint8_t rx_position = 0;//rx data phase
uint16_t tmp_power_value = 0;

uint8_t new_data_pending_flag = 0;

uint16_t last_power_value = 0;//last received rx power data
uint16_t power_delta_time = 0;//time from last data rx
uint16_t two_min_power = 0;//2 minute average power

float total_energy = 1234.5;//Общее количество энергии, квт/час

uint16_t two_min_buffer[TWO_MINUTES_POINTS_CNT]; //2 minutes power readings


//callsed from uart1 handler - processing received data
//packet - 65+67+byte1+byte2+13+10
void receive_byte_handler(uint8_t data)
{
  switch (rx_position)
  {
    case 0:
    {
      if (data == 65) rx_position = 1; else rx_position = 0;
      break;
    }
    case 1:
    {
      if (data == 67) rx_position = 2; else rx_position = 0;
      break;
    }
    case 2:
    {
      tmp_power_value = ((uint16_t)data << 8);//byte1
      rx_position = 3;
      break;
    }
    case 3:
    {
      tmp_power_value|= ((uint16_t)data);//byte2
      rx_position = 4;
      break;
    }
    case 4:
    {
      if (data == 13) rx_position = 5; else rx_position = 0;
      break;
    }
    case 5:
    {
      if (data == 10) 
      {
        last_power_value = tmp_power_value; 
        new_data_pending_flag = 1;
      }
      rx_position = 0;
      break;
    }
    default: break;
  
  }
}

void power_counting_handler(void)
{
  static uint32_t old_time = 0;
  static uint8_t two_min_buf_position = 0;
  uint8_t i;
  uint16_t tmp_two_min_value = 0;
  
  if (new_data_pending_flag != 0)//new power data appear
  {
    new_data_pending_flag = 0;
    old_time = RTC_GetCounter();
    
    two_min_buffer[two_min_buf_position] = last_power_value;
    two_min_buf_position++;
    if (two_min_buf_position>=TWO_MINUTES_POINTS_CNT) two_min_buf_position = 0;
    
    for (i=0;i<TWO_MINUTES_POINTS_CNT;i++)
    {
      tmp_two_min_value+=two_min_buffer[i];
    }
    tmp_two_min_value = tmp_two_min_value / TWO_MINUTES_POINTS_CNT;
    two_min_power = tmp_two_min_value;
  }
  
  uint32_t tmp_time_delta = RTC_GetCounter() - old_time;
  if (tmp_time_delta > (3600*24)) tmp_time_delta = 3600*24;//overflow protection
  
  power_delta_time = (uint16_t)tmp_time_delta;
}