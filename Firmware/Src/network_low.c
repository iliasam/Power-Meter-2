#include "network_low.h"
#include <stdio.h>
#include <stdlib.h>
#include "stm32f1xx_hal_spi.h"
#include "dns.h"
#include "rtc_functions.h"
#include "config.h"

#define DATA_BUF_SIZE                   2048
#define DNS_DATA_BUF_SIZE   		256

uint8_t gDATABUF[DATA_BUF_SIZE];
uint8_t dns_data_buf[DNS_DATA_BUF_SIZE];

uint8_t network_need_sntp_dns_update = 1;

uint8_t domain_ip[4]  = {0, };  

#include "socket.h"
#include "Internet/DHCP/dhcp.h"
extern SPI_HandleTypeDef hspi1;//w5500 SPI

TypeEthState ethernet_state = ETH_STATE_NO_LINK;

//HAL does not have such function in public
void SPI_WaitOnFlag(SPI_HandleTypeDef *hspi, uint32_t Flag, FlagStatus Status)
{
  /* Wait until flag is set */
  if(Status == RESET)
  {
    while(__HAL_SPI_GET_FLAG(hspi, Flag) == RESET)
    {
      asm ("nop");
    }
  }
  else
  {
    while(__HAL_SPI_GET_FLAG(hspi, Flag) != RESET)
    {
      asm ("nop");
    }
  }
}



// Default Network Configuration //
wiz_NetInfo gWIZNETINFO = { .mac = {0x00, 0x08, 0xdc,0x00, 0xab, 0xcd},
                            .ip = {192, 168, 1, 180},
                            .sn = {255,255,255,0},
                            .gw = {192, 168, 1, 1},
                            .dns = {8,8,4,4},
                            .dhcp = NETINFO_DHCP};

void config_w5500_stack(void)
{
  //declare low-level callbac functions
  reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
  reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);
}


uint8_t init2_w5500(void)
{
  
  
  setSHAR(gWIZNETINFO.mac);
#ifdef DEBUG
  printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],
         gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
#endif
  
  reg_dhcp_cbfunc(my_ip_assign, my_ip_assign, my_ip_conflict);//callbacks
  DHCP_init(SOCKET_DHCP, gDATABUF);
  
  DNS_init(DNS_SOCKET, dns_data_buf);
  return 0;
}

//used for sntp
void network_dns_handling(void)
{
  if (network_need_sntp_dns_update)
  {
                 		// Translated IP address by DNS Server
    int8_t ret = DNS_run(gWIZNETINFO.dns, SNTP_DOMAIN_NAME, domain_ip);
    if (ret > 0)
    {
      network_need_sntp_dns_update = 0;
      rtc_change_sntp_ip(domain_ip);
    }
  }
}

//Called if SNTP is not working
void network_start_dns_update(void)
{
  network_need_sntp_dns_update = 1;
}

//called by dhcp
void network_init(void)
{
  // Set Network information from netinfo structure
  ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);
#ifdef DEBUG
  printf("SIP: %d.%d.%d.%d\r\n", gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
  printf("DHCP LEASED TIME : %ld Sec.\r\n", getDHCPLeasetime());
  printf("DNS IP: %d.%d.%d.%d\r\n", gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
#endif
}

//also check link state
void DHCP_routine(void)
{
  uint8_t tmp = 0;
  uint8_t dhcp_result  = 0;
  ctlwizchip(CW_GET_PHYLINK, (void*)&tmp);//check link state
  if (tmp == PHY_LINK_OFF)
  {
    if (ethernet_state!= ETH_STATE_NO_LINK)//link was lost
    {
#ifdef DEBUG
      printf("Link was LOST!\r\n");
#endif
    }
    ethernet_state = ETH_STATE_NO_LINK;
    return;
  }
  else
  {
    if (ethernet_state == ETH_STATE_NO_LINK)//ethernet switched to linked state
    {
      ethernet_state = ETH_STATE_NO_IP;//still do not have ip
      DHCP_stop();//restart dhcp
      DHCP_init(SOCKET_DHCP, gDATABUF);
      dhcp_result = DHCP_run();
    }
    else
    {
      dhcp_result = DHCP_run();
    }
  }
  
  switch(dhcp_result)
  {
    case DHCP_IP_ASSIGN:
    case DHCP_IP_CHANGED: 
      break;
    case DHCP_IP_LEASED:  
    {
      ethernet_state = ETH_STATE_GOT_IP;
      break;
    }
      
    case DHCP_FAILED: 
    {
      ethernet_state = ETH_STATE_NO_IP;//still do not have ip
      DHCP_stop();
      DHCP_init(SOCKET_DHCP, gDATABUF);
      break;
    }
    default:	break;
  }
}


/////////////////////////////////////////////////////////////////
// SPI Callback function for accessing WIZCHIP                 //
// WIZCHIP user should implement with your host spi peripheral //
/////////////////////////////////////////////////////////////////
void  wizchip_select(void)
{
  HAL_GPIO_WritePin(SCS_PORT, SCS_PIN, GPIO_PIN_RESET);
}

void  wizchip_deselect(void)
{
  SPI_WaitOnFlag(&hspi1, SPI_FLAG_BSY, SET);//wait for tx really ends - without it last byte can be damaged by CS
  HAL_GPIO_WritePin(SCS_PORT, SCS_PIN, GPIO_PIN_SET);
}

void  wizchip_write(uint8_t wb)
{
  SPI_WaitOnFlag(&hspi1, SPI_FLAG_TXE, RESET);//ожидаем, пока SPI станет доступным для передачи (это не значит, что предыдущая завершилась)
  hspi1.Instance->DR = (uint16_t)wb;
}

uint8_t wizchip_read(void)
{
  SPI_WaitOnFlag(&hspi1, SPI_FLAG_BSY, SET);//wait for tx really ends - without it last byte can be damaged by CS
  volatile uint16_t tmp = hspi1.Instance->DR;//clear any exciting data
  hspi1.Instance->DR = (uint16_t)0xFF;
  SPI_WaitOnFlag(&hspi1, SPI_FLAG_RXNE, RESET);//wait for data tx and rx
  return (uint8_t)(hspi1.Instance->DR);
}

void w5500_reset(void)
{
  HAL_GPIO_WritePin(RESET_PORT, RESET_PIN, GPIO_PIN_RESET);//0 - reset
  HAL_Delay(1);
  HAL_GPIO_WritePin(RESET_PORT, RESET_PIN, GPIO_PIN_SET);//1 - no reset
  HAL_Delay(10);
}

//callback from dhcp
void my_ip_assign(void)
{
   getIPfromDHCP(gWIZNETINFO.ip);
   getGWfromDHCP(gWIZNETINFO.gw);
   getSNfromDHCP(gWIZNETINFO.sn);
   getDNSfromDHCP(gWIZNETINFO.dns);
   gWIZNETINFO.dhcp = NETINFO_DHCP;
   network_init();
}

//callback from dhcp
void my_ip_conflict(void)
{
   //halt or reset or any...
   while(1); // this example is halt.
}



/*
uint8_t init_w5500(void)
{  
  ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);
  ctlnetwork(CN_GET_NETINFO, (void*)&gWIZNETINFO);
  
  // Display Network Information
  printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],
         gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
  printf("SIP: %d.%d.%d.%d\r\n", gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
  printf("GAR: %d.%d.%d.%d\r\n", gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
  printf("SUB: %d.%d.%d.%d\r\n", gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);

  return 0;
}
*/




