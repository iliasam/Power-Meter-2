#ifndef _GENERATE_JSON
#define _GENERATE_JSON
#include "stdint.h"

extern uint16_t json_data1_size;
extern char json_buffer1[1024];

uint16_t generate_json_data1(void);
void add_str_value_to_json(char* json_buf, char* value_name, char* value, uint8_t is_end);
#endif