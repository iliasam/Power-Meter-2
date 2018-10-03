#ifndef __DHT11_H
#define __DHT11_H

#include "stdint.h"

#define MAX_TICS 10000

#define DHT11_OK 0
#define DHT11_NO_CONN 1
#define DHT11_CS_ERROR 2

#define DHT11_PORT GPIOA
#define DHT11_PIN GPIO_PIN_12

uint8_t read_DHT11(uint8_t *buf);
uint8_t DHT11_Humidity(uint8_t *buf);
uint8_t DHT11_Temperature(uint8_t *buf);

void DHT11_handler(void);


uint16_t dht11_wait_for_rising_edge(void);
uint16_t dht11_wait_for_falling_edge(void);
uint8_t dht11_wait_for_bit(void);

#endif