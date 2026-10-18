#include "core/routes.h"
#include <assert.h>
#include <stdio.h>

static unsigned sends;
static bool last;
static bool send(void *ctx, bool on) {(void)ctx; sends++; last=on; return true;}
int main(void) {
    route_config c = {.enabled=1, .dp=1, .type=4, .active_value=1, .clear_value=0};
    bridge_route a,b;
    route_init(&a,&c);
    c.dp=7; c.type=1; c.off_delay_seconds=10;
    route_init(&b,&c);
    uint8_t one=1,zero=0,two=2;
    route_report(&a,7,1,&one,1,0);
    assert(!a.control.known);
    route_report(&a,1,4,&two,1,0);
    assert(!a.control.known && a.ignored_values==1);
    route_report(&a,1,4,&one,1,10);
    route_report(&b,7,1,&one,1,10);
    occupancy_tick(&a.control,10,send,0);
    occupancy_tick(&b.control,10,send,0);
    assert(sends==2 && last);
    route_report(&a,1,4,&zero,1,20);
    route_report(&b,7,1,&zero,1,20);
    occupancy_tick(&a.control,20,send,0);
    occupancy_tick(&b.control,20,send,0);
    assert(sends==3 && !last && b.control.off_pending);
    occupancy_tick(&b.control,10020,send,0);
    assert(sends==4 && !last);
    /* Input mapping changes discard old state until a new matching report. */
    c.dp=8;
    assert(route_configure(&b,&c,20000));
    assert(!b.control.known && !b.control.tx_pending);
    route_report(&b,7,1,&one,1,20000);
    assert(!b.control.known);
    route_report(&b,8,1,&two,1,20000);
    assert(!b.control.known);
    c.type=2;c.active_value=0x12345678;c.clear_value=42;c.off_delay_seconds=65535;
    assert(route_configure(&b,&c,0));
    uint8_t value[]={0x12,0x34,0x56,0x78};
    route_report(&b,8,2,value,4,1);
    assert(b.control.known && b.control.occupied);
    uint8_t serialized[BRIDGE_CONFIG_SIZE];route_config restored;
    route_config_encode(&c,serialized);
    assert(route_config_decode(&restored,serialized,sizeof(serialized)));
    assert(restored.active_value==0x12345678 && restored.clear_value==42 && restored.off_delay_seconds==65535);
    assert(serialized[0]==2 && restored.on_lux_threshold==0);
    c.on_lux_threshold=12345;route_config_encode(&c,serialized);
    assert(route_config_decode(&restored,serialized,sizeof(serialized)) && restored.on_lux_threshold==12345);
    serialized[0]=3;assert(!route_config_decode(&restored,serialized,sizeof(serialized)));
    serialized[0]=1;serialized[15]=1;assert(!route_config_decode(&restored,serialized,sizeof(serialized)));
    serialized[14]=serialized[15]=0;
    assert(route_config_decode(&restored,serialized,sizeof(serialized)) && restored.on_lux_threshold==0);
    c.enabled=2;assert(!route_config_valid(&c));c.enabled=1;
    c.dp=0;assert(!route_config_valid(&c));c.dp=8;
    c.type=1;assert(!route_config_valid(&c));
    c.active_value=1;c.clear_value=1;assert(!route_config_valid(&c));
    /* Enabling and setting a limit atomically cannot leak an unfiltered On. */
    c=(route_config){.enabled=0,.dp=1,.type=4,.active_value=1};
    route_init(&a,&c);
    occupancy_illuminance(&a.control,true,500,0);
    route_report(&a,1,4,&one,1,0);
    c.enabled=1;c.on_lux_threshold=100;
    assert(route_configure(&a,&c,1) && !a.control.tx_pending);
    c.dp=2;
    assert(route_configure(&a,&c,2) && a.control.lux_known && a.control.lux==500);
    route_report(&a,2,4,&one,1,3);assert(!a.control.tx_pending);
    occupancy_illuminance(&a.control,true,100,4);assert(a.control.tx_pending);
    /* APS delivery failures retry a queued state at most three times. */
    occupancy_control control;occupancy_init(&control,true,0);
    occupancy_presence(&control,true,0);
    occupancy_tick(&control,0,send,0);assert(control.attempts==1 && !control.tx_pending);
    occupancy_delivery_failed(&control,true,10);assert(control.tx_pending);
    occupancy_tick(&control,1009,send,0);assert(control.attempts==1);
    occupancy_tick(&control,1010,send,0);assert(control.attempts==2);
    occupancy_delivery_failed(&control,true,1020);
    occupancy_tick(&control,2020,send,0);assert(control.attempts==3);
    occupancy_delivery_failed(&control,true,2030);assert(!control.tx_pending);
    occupancy_presence(&control,false,2040);
    occupancy_tick(&control,2040,send,0);
    occupancy_delivery_failed(&control,true,2050);assert(!control.tx_pending);
    puts("route tests: PASS");
}
