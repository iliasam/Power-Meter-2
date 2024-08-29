#include "power_counting.h"
#include "rtc_functions.h"
#include "stm32f1xx_hal.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "rtc_functions.h"
#include "string.h"

#define POWER_PULSES_QUEUE_SIZE                 16

#define POWER_MS_IN_SECOND                      (1000)

#define POWER_CONV_COEF                         (uint32_t)(3600*1e6)

//Expected pulse length, ms
#define POWER_PULSE_LENGTH_MS                   (80)

// Коэффициент пересета счетчика - написан на нем
#define POWER_COUNTER_COEF                      (uint32_t)(3200)

// Пересчет периода импульсов в мс в Ватты
#define POWER_CONVERT_PERIOD_MS_TO_WATT(x)       (uint32_t)(POWER_CONV_COEF / \
                                                (POWER_COUNTER_COEF * (x)))

// Регистр для хранения общего счета импульсов с электросчетчика
#define POWER_RTC_TOTAL_COUNTER_REG             RTC_BKP_DR4 //4+5

// Регистр для хранения значения энергии в полночь
#define POWER_RTC_DAY_STAMP_REG                 RTC_BKP_DR6 //6+7

// Регистр для хранения значения энергии в момент смены месяца
#define POWER_RTC_MONTH_STAMP_REG               RTC_BKP_DR8 //8+9

// Последняя полученная мощность, Ватт
uint16_t power_last_value_watt = 0;

// Время с последнего получения импульса, с
uint16_t power_delta_time_s = 0;

//Общее количество энергии, квт*час
float power_total_energy = 0.0f;

//Количество энергии за день, квт*час
float power_day_energy = 0.0f;

//Количество энергии за месяц, квт*час
float power_month_energy = 0.0f;

//Количество энергии за предыдущий месяц, квт*час
float power_prev_month_energy = 0.0f;

// Очередь, содержащая время в ms (timestamp), когда приходили импульсы со счетчика
QueueHandle_t power_pulses_queue;


// Время предыдущей проверки midnight счетчиков
RTCTM power_previous_time;

void power_increment_total_backup_counter(void);
uint32_t power_read_total_count(void);
uint32_t power_get_day_count(void);
uint32_t power_get_month_count(void);
void power_update_midnight_counters(void);

//*****************************************************************************

void power_counting_init(void)
{
  memset(&power_previous_time, 0, sizeof(power_previous_time));
  power_pulses_queue = xQueueCreate(POWER_PULSES_QUEUE_SIZE, sizeof(uint32_t));
  
  //Включаем прерывания от линии электросчетчика
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

// Обработчик, вызываемый из задачи StartPowerCountHandler
void power_counting_handler(void)
{
  //Таймстемп ранее принятого импульса, мс
  static uint32_t power_prev_pulse_timestamp_ms = 0;

  power_delta_time_s = (HAL_GetTick() - power_prev_pulse_timestamp_ms) / POWER_MS_IN_SECOND;
  
  if(uxQueueMessagesWaiting(power_pulses_queue) > 0)
  {
    uint32_t timestamp_ms = 0;
    xQueueReceive(power_pulses_queue, &timestamp_ms, 0);
    
    uint32_t pulse_period_ms = timestamp_ms - power_prev_pulse_timestamp_ms;
    if (pulse_period_ms < 1)
    {
      //this pulse is too short
      power_prev_pulse_timestamp_ms = timestamp_ms;
      return;
    }
    
    power_last_value_watt = POWER_CONVERT_PERIOD_MS_TO_WATT(pulse_period_ms);
    
    if (power_last_value_watt > 5000)
    {
      asm("nop");
    }
    
    power_total_energy = power_read_total_count() / (float)POWER_COUNTER_COEF;
    
    if (rtc_is_time_good())
      power_update_midnight_counters();
    
    
    power_day_energy = power_get_day_count() / (float)POWER_COUNTER_COEF;
    power_month_energy = power_get_month_count() / (float)POWER_COUNTER_COEF;
    
    power_prev_pulse_timestamp_ms = timestamp_ms;
  }
}

// Пересчитать значения в backup регистрах, если это нужно
void power_update_midnight_counters(void)
{
  RTCTM curr_time = rtc_get_current_time();
  
  if ((curr_time.tm_hour == 0) && (power_previous_time.tm_hour != 0))
  {
    // Полночь
    // Получаем текущее значение главного счетчика
    uint32_t current_total_count = power_read_total_count();
    // Обновляем значение регистра-защелки
    rtc_write_32_bit_backup_value(POWER_RTC_DAY_STAMP_REG, current_total_count);
    
    // Наступил новый месяц
    if ((curr_time.tm_mday == 1) && (power_previous_time.tm_mday != 1))
    {
      power_prev_month_energy = power_month_energy;
      // Обновляем значение регистра-защелки
      rtc_write_32_bit_backup_value(POWER_RTC_MONTH_STAMP_REG, current_total_count);
    }
  }
  
  power_previous_time = curr_time;
}

//Установить значение главного счетчика импульсов backup ram
//enegy_value - количество энергии в квт/час
void power_set_total_count(float enegy_value)
{
  uint32_t new_total_count = (uint32_t)(enegy_value * (float)POWER_COUNTER_COEF);
  rtc_write_32_bit_backup_value(POWER_RTC_TOTAL_COUNTER_REG, new_total_count);
  power_total_energy = power_read_total_count() / (float)POWER_COUNTER_COEF;
}

// Считывает значение главного счетчика импульсов из backup ram
uint32_t power_read_total_count(void)
{
  return rtc_read_32_bit_backup_value(POWER_RTC_TOTAL_COUNTER_REG);
}

// Определяет общее значение энергии за день
uint32_t power_get_day_count(void)
{
  uint32_t result = rtc_read_32_bit_backup_value(POWER_RTC_TOTAL_COUNTER_REG) - 
                    rtc_read_32_bit_backup_value(POWER_RTC_DAY_STAMP_REG);
  
  return result;
}

// Определяет общее значение энергии за месяц
uint32_t power_get_month_count(void)
{
  uint32_t result = rtc_read_32_bit_backup_value(POWER_RTC_TOTAL_COUNTER_REG) - 
                    rtc_read_32_bit_backup_value(POWER_RTC_MONTH_STAMP_REG);
  return result;
}

// Сбрасывает счетчик энергии за день
void power_reset_day_count(void)
{
  uint32_t current_total_count = power_read_total_count();
  rtc_write_32_bit_backup_value(POWER_RTC_DAY_STAMP_REG, current_total_count);
  power_day_energy = power_get_day_count() / (float)POWER_COUNTER_COEF;
}

// Сбрасывает счетчик энергии за месяц
void power_reset_month_count(void)
{
  uint32_t current_total_count = power_read_total_count();
  rtc_write_32_bit_backup_value(POWER_RTC_MONTH_STAMP_REG, current_total_count);
  power_month_energy = power_get_month_count() / (float)POWER_COUNTER_COEF;
}

// Увеличиваем значение главного счетчика импульсов
void power_increment_total_backup_counter(void)
{
  uint32_t total_count = power_read_total_count();
  total_count++;
  rtc_write_32_bit_backup_value(POWER_RTC_TOTAL_COUNTER_REG, total_count);
}

//Вызывается из обработчика прерывания, когда приходит импульс от счетчика
void power_pulse_notify(void)
{
  //Таймстемп ранее принятого импульса, мс
  static uint32_t power_prev_pulse_timestamp2_ms = 0;
  
  uint32_t timestamp_ms = HAL_GetTick();
  uint32_t pulse_period_ms = timestamp_ms - power_prev_pulse_timestamp2_ms;
  //Protection for short pulses
  if (pulse_period_ms < POWER_PULSE_LENGTH_MS)
  {
    return;
  }
  
  power_prev_pulse_timestamp2_ms = timestamp_ms;
  
  
  BaseType_t xHigherPriorityTaskWoken;
  xQueueSendFromISR(power_pulses_queue, (void*)&timestamp_ms, &xHigherPriorityTaskWoken);
  power_increment_total_backup_counter();
}