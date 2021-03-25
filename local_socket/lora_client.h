#ifndef ITC_LORA_CLIENT_H
#define ITC_LORA_CLIENT_H

#include <stdint.h>

void init_client(void);

void insert_data_to_list(uint8_t *data, uint16_t data_len);

#endif //ITC_LORA_CLIENT_H
