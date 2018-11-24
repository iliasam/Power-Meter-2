//RTC хранит время в формате UTC

#include "rtc_functions.h"
#include "network_low.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "sntp.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_rcc_ex.h"

#define RTC_SYNC_PERIOD                 (uint32_t)(1*60) // Период синхронизации RTC, сек

#define RTC_STATUS_REG                  RTC_BKP_DR1  // Регистр для хранения статуса RTC
#define RTC_STATUS_INIT_DONE            0x1234       /* RTC initialised value*/

#define SNTP_SOCKET 			4 //SOCKET_CODE
#define SNTP_DATA_BUF_SIZE   		256

//Hours
#define RTC_TIMEZONE                    (3)

static const uint8_t rtc_days_in_month_info[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

uint8_t sntp_server_ip[4] = {211, 233, 84, 186}; // kr.pool.ntp.org
datetime sntp_time;//Время, полученое от SNTP

uint8_t sntp_data_buf[SNTP_DATA_BUF_SIZE];//Буфер для хранения промежуточных данных SNTP

uint32_t reset_time = 0;//time of reset ????

RTC_State_Type rtc_status = NO_INIT;
uint32_t last_sync_time = 0;// Время последней синхронизации - UTC

extern TypeEthState ethernet_state;



//******************************************************************************

void init_sntp_module(void)
{
  //socket, ip adsress, time zone, buffer
  SNTP_init(SNTP_SOCKET, sntp_server_ip, 29, sntp_data_buf);
}

// Периодически вызывается из задачи StartRTCUpdateHandler()
void rtc_update_handler(void)
{
  uint32_t cur_rtc_time;
  if (ethernet_state != ETH_STATE_GOT_IP)
  {
    //no ip
  }
  else
  {
    //Получаем время от RTC и сравниваваем с временнем последнего обновления
    cur_rtc_time = RTC_GetCounter();
    if (((cur_rtc_time - last_sync_time) > RTC_SYNC_PERIOD) && (rtc_status != RTC_INIT_FAIL))
    {
      if (rtc_status != NO_TIME_SET)
      {
        //Настало время синхронизации и RTC работает нормально
        rtc_status = TIME_NO_SYNC;
      }
      
      // Пытаемся получить время от SNTP
      if (SNTP_run(&sntp_time))
      {
        update_reset_time();
        RTC_SetCounter((uint32_t)sntp_time.raw_value);//в RTC записывается время UTC
        last_sync_time = (uint32_t)sntp_time.raw_value;//refresh sync time
        rtc_status = RTC_OK;
        
#ifdef DEBUG
        printf("SNTP: %d-%d-%d, %d:%d:%d\r\n", sntp_time.yy, sntp_time.mo, sntp_time.dd, sntp_time.hh, sntp_time.mm, sntp_time.ss);
#endif
        update_reset_time();
      }
      else
      {
#ifdef DEBUG
        printf("SNTP fail!\r\n");
#endif
      }
    }//end of internet part

  }
}

// Инициализация RTC
void rtc_init(void)
{
  uint32_t status;
  RTC_HandleTypeDef rtc_handle;

  rtc_init_hardware_clk();
  if (rtc_status == RTC_INIT_FAIL) 
    return;
  
  rtc_handle.Instance = RTC;
  rtc_handle.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  rtc_handle.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  
  if (HAL_RTC_Init(&rtc_handle) != HAL_OK)
    rtc_status = RTC_INIT_FAIL;
  else
    rtc_status = NO_TIME_SET;

  // Получить статус RTC - читаем первый регистр BKUP_RAM
  status = HAL_RTCEx_BKUPRead(&rtc_handle, RTC_STATUS_REG);
  
  RTC_SetCounter(1000);//reset RTC
  
  if (status == RTC_STATUS_INIT_DONE)
  {
    //BKUP содержит полезные данные
  }
  else 
  {
    //Записываем магическое число в BKUP
    HAL_RTCEx_BKUPWrite(&rtc_handle, RTC_STATUS_REG, RTC_STATUS_INIT_DONE);//первое включение
  }
}

//Настройка системы тактирования RTC
void rtc_init_hardware_clk(void)
{
  RCC_OscInitTypeDef        RCC_OscInitStruct;
  RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;
  
  //##-1- Enables the PWR Clock and Enables access to the backup domain ###################################
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_BKP_CLK_ENABLE();
  
  //##-2- Configue LSE as RTC clock soucre ###################################*/
  RCC_OscInitStruct.OscillatorType =  RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    rtc_status = RTC_INIT_FAIL;
  }
  
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  
  if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  { 
    rtc_status = RTC_INIT_FAIL;
  }
  __HAL_RCC_RTC_ENABLE();// Enable RTC Clock
}

// Read uint32_t value from backup data
uint32_t rtc_read_backup_value(uint32_t register_number)
{
  if (register_number >= RTC_BKP_NUMBER)
    return 0; //error
  
  if (register_number == RTC_BKP_DR1)
    return 0; //error
  
  RTC_HandleTypeDef rtc_handle;
  rtc_handle.Instance = RTC;
  return HAL_RTCEx_BKUPRead(&rtc_handle, register_number);
}

//Write uint32_t "value" to "register_number"
void rtc_write_backup_value(uint32_t register_number, uint32_t value)
{
  if (register_number >= RTC_BKP_NUMBER)
    return; //error
  
  if (register_number == RTC_BKP_DR1)
    return; //error
  
  RTC_HandleTypeDef rtc_handle;
  rtc_handle.Instance = RTC;
  
  HAL_RTCEx_BKUPWrite(&rtc_handle, register_number, value);
}

// RTC time is probaly good
uint8_t rtc_is_time_good(void)
{
  if ((rtc_status == TIME_NO_SYNC) || (rtc_status == RTC_OK))
    return 1;
  else
    return 0;
}

//##############################################################################
//SPL functions 

void RTC_EnterConfigMode(void)
{
  /* Set the CNF flag to enter in the Configuration Mode */
  RTC->CRL |= RTC_CRL_CNF;
}

void RTC_ExitConfigMode(void)
{
  /* Reset the CNF flag to exit from the Configuration Mode */
  RTC->CRL &= (uint16_t)~((uint16_t)RTC_CRL_CNF); 
}

void RTC_SetCounter(uint32_t CounterValue)
{ 
  RTC_EnterConfigMode();
  /* Set RTC COUNTER MSB word */
  RTC->CNTH = CounterValue >> 16;
  /* Set RTC COUNTER LSB word */
  RTC->CNTL = (CounterValue & ((uint32_t)0x0000FFFF));
  RTC_ExitConfigMode();
}

uint32_t RTC_GetCounter(void)
{
  uint16_t tmp = 0;
  tmp = RTC->CNTL;
  return (((uint32_t)RTC->CNTH << 16 ) | tmp) ;
}

void Rtc_RawLocalTime( RTCTM *aExpand, uint32_t time )
{
    uint32_t year;
    uint8_t month;

    /* Seconds is time mod 60 */
    aExpand->tm_sec = time % 60;
    time /= 60;
    
    /* Minutes is time mod 60 */
    aExpand->tm_min = time % 60;
    time /= 60;
    
    /* Hours is time mod 24 */
    aExpand->tm_hour = time % 24;
    time /= 24;
    
    /* We now have the number of days since 1970, but need days since 1900 */
    /* There are 17 leap years prior to 1970. Although 1900 was not a leap year */
    /* RTCTIME time cannot represent days before 1970 so make it a leap year to avoid */
    /* the special case (1900 not a leap year, but 2000 is */
    /* 25568 = (70 * 365) + 18 */
    time += 25568;
    
    /* Day of week */
    aExpand->tm_wday = time % 7;
    
    /* Calculate year and day within year */
    year = (time / DAYSIN4YEARS) << 2;
    time %= DAYSIN4YEARS;
    
    /* If day in year is greater than 365 then adjust so it is 365 or less (0 is Jan 1st) */
    if(time > 365)
    {
        year += ((time - 1) / 365);
        time = (time - 1) % 365;
    }
    
    aExpand->tm_year = year;
    
    /* Day of year 1st Jan is 0 */
    aExpand->tm_yday = time;
    
    /* Not a leap year and is feb 29 or greater */
    /* Then add a day to account for feb has 28, not 29 days */
    /* this is important for the month calculation below */
    if((year & 3) && (time >= 59))
    {
        time++;
    }
    
    month = 0;
    
    /* Remove days in month till left with less or equal days than in current month */
    while(time >= rtc_days_in_month_info[month])
    {
        time -= rtc_days_in_month_info[month];
        month++;
    }
    
    aExpand->tm_mon = month;
    
    /* Day of month is what's left, but we want it to start from 1, not 0 */
    aExpand->tm_mday = time + 1;
}



//print current time to buffer
void print_current_time(char* buffer)
{
  uint32_t current_time = RTC_GetCounter() + 3600 * RTC_TIMEZONE;
  RTCTM cur_time;//текущее время - структура
  Rtc_RawLocalTime(&cur_time,current_time);
  sprintf(buffer,"%02u/%02u/%04u %02u:%02u:%02u",
          cur_time.tm_mday,
          (cur_time.tm_mon + 1), 
          (cur_time.tm_year + 1830),
          cur_time.tm_hour,
          cur_time.tm_min, 
          cur_time.tm_sec);
}

RTCTM rtc_get_current_time(void)
{
  uint32_t current_time = RTC_GetCounter() + 3600 * RTC_TIMEZONE;
  RTCTM cur_time;//текущее время - структура
  Rtc_RawLocalTime(&cur_time,current_time);
  return cur_time;
}


void update_reset_time(void)
{
  uint32_t current_time = RTC_GetCounter();//UTC time
  uint32_t reset_delta_time = 0;
  static uint32_t startup_reset_time = 0;
  
  if (reset_time == 0)//no real time received
  {
    reset_delta_time = current_time - reset_time;
    if (reset_delta_time > (3600*24*100))//100 days
    {
      //current_time is real time
      reset_time = current_time - startup_reset_time;
    }
    else
    {
      startup_reset_time = reset_delta_time;
    }
  }
}

//Выводит в буфер buffer число дней и часов с последний перезагрузки
void rtc_time_from_reset_to_buffer(char* buffer)
{
  uint32_t current_time = RTC_GetCounter();//UTC time
  uint32_t reset_delta_time = 0;
  
  reset_delta_time = current_time - reset_time;
  
  if (reset_delta_time > (3600*24*100))//100 days 
  {
    reset_delta_time = 0;//error
  }
  reset_delta_time = reset_delta_time / 3600;//to hours
  
  uint16_t days_from_reset = (uint16_t)(reset_delta_time / 24);
  uint8_t hours_from_rest = (uint8_t)(reset_delta_time % 24);
  
  sprintf(buffer,"%d days %d hr",  days_from_reset, hours_from_rest);
}


