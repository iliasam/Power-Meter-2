#ifndef _POWER_COUNTING
#define _POWER_COUNTING

#include "stdint.h"

void receive_byte_handler(uint8_t data);
void power_counting_handler(void);
void power_counting_init(void);
void power_pulse_notify(void);
void power_set_total_count(float enegy_value);

#endif