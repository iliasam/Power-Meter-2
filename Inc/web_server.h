#ifndef _WEB_SERVER_H
#define _WEB_SERVER_H

#include "stdint.h"

#define HTTP_IDLE 0
#define HTTP_SENDING 1

void web_server_handler(void);
int32_t loopback_web_server(uint8_t sock_num, uint8_t* buf, uint16_t port);


const char *httpd_get_mime_type(char *url);
void HTTP_reset(uint8_t sock_num);

uint32_t url_exists(char* file_name);

uint16_t f_read(char *fp, uint8_t *buff, uint16_t bytes_to_read,	uint32_t offset);

#endif

