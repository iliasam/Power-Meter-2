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

// Коэффициент пересета счетчика - написан на нем
#define POWER_COUNTER_COEF                      (uint32_t)(3200)

// Пересчет периода импульсов в мс в Ватты
#define POWER_CONVERT_PEIOD_MS_TO_WATT(x)       (uint32_t)(POWER_CONV_COEF / \
                                                (POWER_COUNTER_COEF * (x)))

// Регистр для хранения общего счета импульсов с электросчетчика
#define POWER_RTC_TOTAL_COUNTER_REG             RTC_BKP_DR2

// Регистр для хранения значения энергии в полночь
#define POWER_RTC_DAY_STAMP_REG                 RTC_BKP_DR3

// Регистр для хранения значения энергии в момент смены месяца
#define POWER_RTC_MONTH_STAMP_REG               RTC_BKP_DR4

// Последняя полученная мощность, Ватт
uint16_t power_last_value = 0;

// Время с последнего получения импульса, с
uint16_t power_delta_time = 0;

//Общее количество энергии, квт/час
float power_total_energy = 0.0f;

//Количество энергии за день, квт/час
float power_day_energy = 0.0f;

//Количество энергии за месяц, квт/час
float power_month_energy = 0.0f;

// Очередь, содержащая время в ms (timestamp), когда приходили импульсы со счетчика
QueueHandle_t power_pulses_queue;

//Таймстемп ранее принятого импульса, мс
uint32_t power_prev_pulse_timestamp = 0;

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
  
  //Включаем прерыания от линии электросчетчика
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

// Обработчик, вызываемый из задачи StartPowerCountHandler
void power_counting_handler(void)
{
  power_delta_time = (HAL_GetTick() - power_prev_pulse_timestamp) / POWER_MS_IN_SECOND;
  
  if(uxQueueMessagesWaiting(power_pulses_queue) > 0)
  {
    uint32_t timestamp = 0;
    xQueueReceive(power_pulses_queue, &timestamp, 0);
    
    uint32_t pulses_period = timestamp - power_prev_pulse_timestamp;
    
    power_last_value = POWER_CONVERT_PEIOD_MS_TO_WATT(pulses_period);
    
    power_total_energy = power_read_total_count() / (float)POWER_COUNTER_COEF;
    
    if (rtc_is_time_good())
      power_update_midnight_counters();
    
    
    power_day_energy = power_get_day_count() / (float)POWER_COUNTER_COEF;
    power_month_energy = power_get_month_count() / (float)POWER_COUNTER_COEF;
    
    power_prev_pulse_timestamp = timestamp;
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
    rtc_write_backup_value(POWER_RTC_DAY_STAMP_REG, current_total_count);
    
    // Наступил новый месяц
    if ((curr_time.tm_mday == 1) && (power_previous_time.tm_mday != 1))
    {
      // Обновляем значение регистра-защелки
      rtc_write_backup_value(POWER_RTC_MONTH_STAMP_REG, current_total_count);
    }
  }
  
  power_previous_time = curr_time;
}

//Установить значение главного счетчика импульсов backup ram
//enegy_value - количество энергии в квт/час
void power_set_total_count(float enegy_value)
{
  uint32_t new_total_count = (uint32_t)(enegy_value * (float)POWER_COUNTER_COEF);
  rtc_write_backup_value(POWER_RTC_TOTAL_COUNTER_REG, new_total_count);
}

// Считывает значение главного счетчика импульсов из backup ram
uint32_t power_read_total_count(void)
{
  return rtc_read_backup_value(POWER_RTC_TOTAL_COUNTER_REG);
}

// Определяет общее значение энергии за день
uint32_t power_get_day_count(void)
{
  uint32_t result = rtc_read_backup_value(POWER_RTC_TOTAL_COUNTER_REG) - 
                    rtc_read_backup_value(POWER_RTC_DAY_STAMP_REG);
  return result;
}

// Определяет общее значение энергии за месяц
uint32_t power_get_month_count(void)
{
  uint32_t result = rtc_read_backup_value(POWER_RTC_TOTAL_COUNTER_REG) - 
                    rtc_read_backup_value(POWER_RTC_MONTH_STAMP_REG);
  return result;
}

// Увеличиваем значение главного счетчика импульсов
void power_increment_total_backup_counter(void)
{
  uint32_t total_count = power_read_total_count();
  total_count++;
  rtc_write_backup_value(POWER_RTC_TOTAL_COUNTER_REG, total_count);
}

//Вызывается из обработчика прерывания, когда приходит импульс от счетчика
void power_pulse_notify(void)
{
  BaseType_t xHigherPriorityTaskWoken;
  
  uint32_t timestamp = HAL_GetTick();
  xQueueSendFromISR(power_pulses_queue, (void*)&timestamp, &xHigherPriorityTaskWoken);
  power_increment_total_backup_counter();
}