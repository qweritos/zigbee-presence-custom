#include "routes.h"
#include "tuya_protocol.h"
#include <string.h>

bool route_config_valid(const route_config *c) {
    if (c->enabled > 1 || c->reserved || (c->enabled && !c->dp)) return false;
    uint32_t max;
    switch (c->type) {
    case 1: max = 1; break;
    case 4: max = 255; break;
    case 2: case 5: max = UINT32_MAX; break;
    default: return false;
    }
    return c->active_value != c->clear_value && c->active_value <= max && c->clear_value <= max;
}
void route_init(bridge_route *r, const route_config *c) {
    memset(r, 0, sizeof(*r));
    r->config = *c;
    occupancy_init(&r->control, c->enabled, c->off_delay_seconds);
}
bool route_configure(bridge_route *r, const route_config *c, uint32_t now) {
    if (!route_config_valid(c)) return false;
    const route_config *old = &r->config;
    if (old->dp != c->dp || old->type != c->type || old->active_value != c->active_value || old->clear_value != c->clear_value) {
        /* A report for the old mapping cannot establish the new mapping's state. */
        occupancy_init(&r->control, c->enabled, c->off_delay_seconds);
    } else {
        occupancy_delay(&r->control, c->off_delay_seconds, now);
        occupancy_enable(&r->control, c->enabled, now);
    }
    r->config = *c;
    return true;
}
void route_report(bridge_route *r, uint8_t dp, uint8_t type, const uint8_t *v, size_t n, uint32_t now) {
    if (!dp || dp != r->config.dp || type != r->config.type) return;
    if (!((type == 1 && n == 1 && v[0] <= 1) || (type == 4 && n == 1) ||
          (type == 2 && n == 4) || (type == 5 && (n == 1 || n == 2 || n == 4)))) return;
    uint32_t value = 0;
    for (size_t i = 0; i < n; i++) value = (value << 8) | v[i];
    r->input_value = value;
    r->matched_reports++;
    if (value == r->config.active_value) occupancy_presence(&r->control, true, now);
    else if (value == r->config.clear_value) occupancy_presence(&r->control, false, now);
    else r->ignored_values++;
}
void route_config_encode(const route_config *c, uint8_t out[BRIDGE_CONFIG_SIZE]) {
    /* Explicit versioned wire format, independent of compiler padding/endian. */
    memset(out, 0, BRIDGE_CONFIG_SIZE);
    out[0] = 1; out[1] = c->enabled; out[2] = c->dp; out[3] = c->type;
    out[4] = c->off_delay_seconds; out[5] = c->off_delay_seconds >> 8;
    for (unsigned i = 0; i < 4; i++) {
        out[6+i] = c->active_value >> (8*i);
        out[10+i] = c->clear_value >> (8*i);
    }
}
bool route_config_decode(route_config *c, const uint8_t *in, size_t n) {
    if (n != BRIDGE_CONFIG_SIZE || in[0] != 1 || in[14] || in[15]) return false;
    route_config x = {.enabled=in[1], .dp=in[2], .type=in[3],
                      .off_delay_seconds=(uint16_t)(in[4] | ((uint16_t)in[5] << 8))};
    for (unsigned i = 0; i < 4; i++) {
        x.active_value |= (uint32_t)in[6+i] << (8*i);
        x.clear_value |= (uint32_t)in[10+i] << (8*i);
    }
    if (!route_config_valid(&x)) return false;
    *c = x;
    return true;
}
