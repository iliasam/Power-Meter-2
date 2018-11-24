#include "generate_json.h"
#include "rtc_functions.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "stm32f1xx_hal.h"

char json_buffer1[1024];

uint16_t json_data1_size = 0;
extern uint8_t dht11_temperature;
extern uint8_t dht11_humidity;

extern uint16_t power_last_value;
extern uint16_t power_delta_time;
extern float power_total_energy;

uint16_t generate_json_data1(void)
{
  char str[64];
  memset(json_buffer1, 0, sizeof(json_buffer1));//clear json
  json_data1_size = 0;
  
  strcat((char*)json_buffer1, "{\r\n");//start
  
  print_current_time((char*)str);
  add_str_value_to_json(json_buffer1, "dev_time", str, 0);

  sprintf(str, "\"cur_power\": %d,\r\n", power_last_value);
  strcat((char*)json_buffer1,str);
  
  sprintf(str, "\"power_total_energy\": %f,\r\n", power_total_energy);
  strcat((char*)json_buffer1,str);
  
  sprintf(str, "\"day_energy\": %d,\r\n", (uint32_t)1234);
  strcat((char*)json_buffer1,str);
  
  sprintf(str, "\"month_energy\": %d,\r\n", (uint32_t)3210);
  strcat((char*)json_buffer1,str);
  
  sprintf(str, "\"last_rx_time\": %d sec,\r\n", power_delta_time);
  strcat((char*)json_buffer1,str);
  
  //%%%%
  
  rtc_time_from_reset_to_buffer((char*)str);
  add_str_value_to_json(json_buffer1, "time_from_reset", str, 0);
  
  add_str_value_to_json(json_buffer1, "total_time", "10d 2h", 0);
  
  sprintf(str, "\"temperature\": %d,\r\n", (uint8_t)dht11_temperature);
  strcat((char*)json_buffer1,str);
  
  sprintf(str, "\"humidity\": %d\r\n", (uint8_t)dht11_humidity);
  strcat((char*)json_buffer1,str);
  
  strcat((char*)json_buffer1, "}");//end
  
  json_data1_size = strlen((char*)json_buffer1);
  return json_data1_size;
}

void add_str_value_to_json(char* json_buf, char* value_name, char* value, uint8_t is_end)
{
  char loc_str[64];
  memset(loc_str, 0, sizeof(loc_str));//clear json
  
  strcat((char*)loc_str, "\"");//begin value_name
  strcat((char*)loc_str, value_name);//value_name body
  strcat((char*)loc_str, "\": \"");//end value_name + begin value (---": "--)
  strcat((char*)loc_str, value);//value body
  strcat((char*)loc_str, "\"");//end value
  
  if (is_end != 1)
  {
    strcat((char*)loc_str, ",\r\n");//end value
  }
  else
  {
    strcat((char*)loc_str, "\r\n");//end value
  }
  
  strcat(json_buf, loc_str);//copy to main buffer
}

