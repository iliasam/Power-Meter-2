#ifndef __NETWORK_LOW_H
#define __NETWORK_LOW_H
#include "stm32f1xx_hal.h"
#include "config.h"

/***************************************
 * SOCKET NUMBER DEFINION
 ***************************************/

typedef enum
{
  ETH_STATE_NO_LINK = 0,
  ETH_STATE_NO_IP,
  ETH_STATE_GOT_IP
} TypeEthState;

#define SCS_PIN  GPIO_PIN_4 //w5500 cs pin
#define SCS_PORT GPIOA

#define INT_PIN  GPIO_PIN_1 //w5500 int pin
#define INT_PORT GPIOA

#define RESET_PIN  GPIO_PIN_11 //w5500 reset pin
#define RESET_PORT GPIOB

uint8_t init_w5500(void);
void config_w5500_stack(void);

void  wizchip_select(void);
void  wizchip_deselect(void);
void  wizchip_write(uint8_t wb);
uint8_t wizchip_read(void);
void w5500_reset(void);

uint8_t init2_w5500(void);

void network_init(void);
void my_ip_assign(void);
void my_ip_conflict(void);
void DHCP_routine(void);
void network_dns_handling(void);
void network_start_dns_update(void);



#endif