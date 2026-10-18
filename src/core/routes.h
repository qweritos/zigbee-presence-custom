#ifndef BRIDGE_ROUTES_H
#define BRIDGE_ROUTES_H
#include "occupancy_control.h"
#include <stddef.h>

#define BRIDGE_ROUTE_COUNT 4u
#define BRIDGE_CONFIG_SIZE 16u
typedef struct {
    uint8_t enabled, dp, type, reserved;
    uint16_t off_delay_seconds;
    uint32_t active_value, clear_value;
} route_config;
typedef struct {
    route_config config;
    occupancy_control control;
    uint32_t input_value, matched_reports, ignored_values;
} bridge_route;

bool route_config_valid(const route_config *);
void route_init(bridge_route *, const route_config *);
bool route_configure(bridge_route *, const route_config *, uint32_t);
void route_report(bridge_route *, uint8_t, uint8_t, const uint8_t *, size_t, uint32_t);
bool route_config_decode(route_config *, const uint8_t *, size_t);
void route_config_encode(const route_config *, uint8_t [BRIDGE_CONFIG_SIZE]);
#endif
