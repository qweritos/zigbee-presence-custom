#include "uart.h"
#include "em_cmu.h"
#include "em_core.h"
#include "em_gpio.h"
#include "em_usart.h"

/* ZS3L: module pin 16 TX=PA5, pin 15 RX=PA6. No debug console here. */
#define RX_SIZE 512u
#define TX_SIZE 1024u
static uint8_t rx[RX_SIZE], tx[TX_SIZE];
static volatile uint16_t rx_head, rx_tail, tx_head, tx_tail;
volatile uint32_t mcu_uart_rx_bytes, mcu_uart_overflows, mcu_uart_errors;

void USART0_RX_IRQHandler(void) {
    uint32_t flags=USART_IntGet(USART0);
    if (flags & (USART_IF_RXOF | USART_IF_FERR | USART_IF_PERR)) {
        mcu_uart_errors++;
        USART_IntClear(USART0, USART_IF_RXOF | USART_IF_FERR | USART_IF_PERR);
    }
    while (USART0->STATUS & USART_STATUS_RXDATAV) {
        uint8_t b=USART_Rx(USART0);
        uint16_t next=(rx_head+1u)&(RX_SIZE-1u);
        mcu_uart_rx_bytes++;
        if (next==rx_tail) mcu_uart_overflows++;
        else {rx[rx_head]=b; __DMB(); rx_head=next;}
    }
}
void USART0_TX_IRQHandler(void) {
    while ((USART0->STATUS & USART_STATUS_TXBL) && tx_tail!=tx_head) {
        USART_Tx(USART0,tx[tx_tail]);
        tx_tail=(tx_tail+1u)&(TX_SIZE-1u);
    }
    if (tx_tail==tx_head) USART_IntDisable(USART0,USART_IEN_TXBL);
}
void mcu_uart_init(void) {
    CMU_ClockEnable(cmuClock_GPIO,true);
    CMU_ClockEnable(cmuClock_USART0,true);
    GPIO_PinModeSet(gpioPortA,5,gpioModePushPull,1);
    GPIO_PinModeSet(gpioPortA,6,gpioModeInputPull,1);
    USART_InitAsync_TypeDef init=USART_INITASYNC_DEFAULT;
    init.baudrate=115200;
    init.enable=usartDisable;
    USART_InitAsync(USART0,&init);
    GPIO->USARTROUTE[0].TXROUTE=(gpioPortA<<_GPIO_USART_TXROUTE_PORT_SHIFT)|(5<<_GPIO_USART_TXROUTE_PIN_SHIFT);
    GPIO->USARTROUTE[0].RXROUTE=(gpioPortA<<_GPIO_USART_RXROUTE_PORT_SHIFT)|(6<<_GPIO_USART_RXROUTE_PIN_SHIFT);
    GPIO->USARTROUTE[0].ROUTEEN=GPIO_USART_ROUTEEN_TXPEN|GPIO_USART_ROUTEEN_RXPEN;
    USART_IntClear(USART0,_USART_IF_MASK);
    NVIC_ClearPendingIRQ(USART0_RX_IRQn);
    NVIC_ClearPendingIRQ(USART0_TX_IRQn);
    NVIC_SetPriority(USART0_RX_IRQn,6);
    NVIC_SetPriority(USART0_TX_IRQn,6);
    NVIC_EnableIRQ(USART0_RX_IRQn);
    NVIC_EnableIRQ(USART0_TX_IRQn);
    USART_IntEnable(USART0,USART_IEN_RXDATAV|USART_IEN_RXOF|USART_IEN_FERR|USART_IEN_PERR);
    USART_Enable(USART0,usartEnable);
}
bool mcu_uart_read(uint8_t *b) {
    if (rx_tail==rx_head) return false;
    *b=rx[rx_tail]; __DMB(); rx_tail=(rx_tail+1u)&(RX_SIZE-1u);
    return true;
}
bool mcu_uart_write(const uint8_t *p,size_t n) {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    size_t free=(tx_tail-tx_head-1u)&(TX_SIZE-1u);
    if (n>free) {CORE_EXIT_CRITICAL(); return false;}
    for (size_t i=0;i<n;i++) {tx[tx_head]=p[i];tx_head=(tx_head+1u)&(TX_SIZE-1u);}
    __DMB();
    if (n) USART_IntEnable(USART0,USART_IEN_TXBL);
    CORE_EXIT_CRITICAL();
    return true;
}
