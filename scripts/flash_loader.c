/* SWD RAM loader using the exact target's vendor MSC programming routines.
 * No startup, interrupts, stock code, protected storage or OTP operations.
 */
#include "em_device.h"
#include "em_msc.h"
#include <stdint.h>
#define PAGE 0x2000u
typedef struct {
    uint32_t magic, command, address, length, status, part, ieee_low, ieee_high;
    uint8_t data[PAGE];
} mailbox_t;
#define BOX ((volatile mailbox_t *)0x20002000u)
__attribute__((section(".entry"),used,noreturn)) void flash_entry(void) {
    __disable_irq();
    BOX->status=0xffffffffu;
    BOX->part=DEVINFO->PART;
    BOX->ieee_low=DEVINFO->EUI64L;
    BOX->ieee_high=DEVINFO->EUI64H;
    if (BOX->magic!=0x464c5348u || BOX->part!=0x01150014u) BOX->status=0x100;
    else if (!BOX->command) BOX->status=0;
    else if (BOX->command>2 || BOX->address>=0xbe000u || (BOX->address & (PAGE-1u)) ||
             BOX->length>PAGE || (BOX->length & 3u)) BOX->status=0x101;
    else {
        MSC_Init();
        MSC_Status_TypeDef result=MSC_ErasePage((uint32_t *)BOX->address);
        if (result==mscReturnOk && BOX->command==1 && BOX->length)
            result=MSC_WriteWord((uint32_t *)BOX->address,(const void *)BOX->data,BOX->length);
        BOX->status=(uint32_t)result;
        MSC_Deinit();
    }
    __DSB(); __ISB();
    __BKPT(0);
    for (;;) {}
}
