#ifndef _RTC_FUNCTIONS
#define _RTC_FUNCTIONS
#include "stdint.h"

/* Number of days in 4 years: (365 * 4) + 1 */
#define DAYSIN4YEARS 1461

struct s_tm {

  uint8_t tm_sec; //seconds after the minute: 0 - 59
  uint8_t tm_min; //minutes after the hour: 0 - 59
  uint8_t tm_hour;//hours since midnight: 0 - 23
  uint8_t tm_mday;//day of the month: 1 - 31
  uint8_t tm_mon;//months since January: 0 - 11
  uint8_t tm_year;//years since 1900
  uint8_t tm_wday;//days since Sunday: 0 - 6
  uint16_t tm_yday;//days since Jan 1st: 0 - 365
  int8_t tm_isdst;//daylight savings flag: -1 unknown, 0 not in DST, 1 in DST
};

typedef struct s_tm RTCTM; 

typedef enum
{
  NO_INIT = 0,   //ѕеред инициализацией
  RTC_INIT_FAIL, //RTC неправильно инициализирован
  NO_TIME_SET,   //»нициализаци€ прошла нормально, но врем€ пока неправильное
  TIME_NO_SYNC,  //давно не было синхронизации
  RTC_OK
} RTC_State_Type;

void init_sntp_module(void);
void rtc_update_handler(void);
void rtc_init(void);
void rtc_init_hardware_clk(void);

//SPL functions
void RTC_SetCounter(uint32_t CounterValue);
uint32_t RTC_GetCounter(void);

void Rtc_RawLocalTime( RTCTM *aExpand, uint32_t time );
void print_current_time(char* buffer);

void update_reset_time(void);
void rtc_time_from_reset_to_buffer(char* buffer);
void rtc_total_time_to_buffer(char* buffer);
uint8_t rtc_is_time_good(void);
RTCTM rtc_get_current_time(void);

uint16_t rtc_read_16_bit_backup_value(uint32_t register_number);
uint32_t rtc_read_32_bit_backup_value(uint32_t register_number);

void rtc_write_16_bit_backup_value(uint32_t register_number, uint16_t value);
void rtc_write_32_bit_backup_value(uint32_t register_number, uint32_t value);

void rtc_change_sntp_ip(uint8_t* ip_source);

#endif