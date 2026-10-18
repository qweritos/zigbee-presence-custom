#ifndef BRIDGE_APP_H
#define BRIDGE_APP_H
#include <stdbool.h>
#include <stdint.h>
void bridge_init(void);
void bridge_process(void);
/* Used by the dynamic HAL before committing a remote attribute write. */
uint8_t bridge_attribute_write(uint8_t, uint16_t, uint16_t, const uint8_t *, uint8_t);
void bridge_attribute_read(void);
#endif
