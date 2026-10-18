#include "sl_main_init.h"
#include "sl_main_process_action.h"
#include "bridge.h"

int main(void) {
    sl_main_init();
    bridge_init();
    for (;;) {
        sl_main_process_action();
        bridge_process();
    }
}
