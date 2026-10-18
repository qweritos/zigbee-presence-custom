#ifndef BRIDGE_UART_H
#define BRIDGE_UART_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void mcu_uart_init(void);
bool mcu_uart_read(uint8_t *);
bool mcu_uart_write(const uint8_t *, size_t);
extern volatile uint32_t mcu_uart_rx_bytes, mcu_uart_overflows, mcu_uart_errors;
#endif
