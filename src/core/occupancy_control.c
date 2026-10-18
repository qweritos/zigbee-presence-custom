#include "occupancy_control.h"
#include <string.h>

static bool due(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}
static void queue(occupancy_control *c, bool on, uint32_t now) {
    c->desired_on = on;
    c->on_latched = on;
    c->desired_known = true;
    c->tx_pending = c->enabled;
    c->attempts = 0;
    c->next_tx = now;
}
static void try_on(occupancy_control *c, uint32_t now) {
    if (!c->enabled || !c->known || !c->occupied || c->on_latched) return;
    if (!c->on_lux_threshold || (c->lux_known && c->lux <= c->on_lux_threshold))
        queue(c, true, now);
}
void occupancy_lux_limit(occupancy_control *c, uint16_t lux, uint32_t now) {
    c->on_lux_threshold = lux;
    try_on(c, now);
}
void occupancy_illuminance(occupancy_control *c, bool known, uint32_t lux, uint32_t now) {
    c->lux_known = known;
    c->lux = lux;
    /* Once On is accepted, the lamp's own light cannot reverse the decision. */
    try_on(c, now);
}
void occupancy_init(occupancy_control *c, bool enabled, uint16_t delay) {
    memset(c, 0, sizeof(*c));
    c->enabled = enabled;
    c->off_delay_seconds = delay;
}
void occupancy_presence(occupancy_control *c, bool occupied, uint32_t now) {
    if (c->known && c->occupied == occupied) return;
    c->known = true;
    c->occupied = occupied;
    c->off_pending = false;
    if (occupied) {
        if (c->on_latched) queue(c, true, now); /* Cancel a pending clear delay. */
        else try_on(c, now);
    } else {
        c->clear_since = now;
        if (c->off_delay_seconds) {
            c->off_pending = true;
            c->off_deadline = now + (uint32_t)c->off_delay_seconds * 1000u;
            /* An old failed ON must not be retried after a clear event. */
            c->tx_pending = false;
        } else queue(c, false, now);
    }
}
void occupancy_enable(occupancy_control *c, bool enabled, uint32_t now) {
    if (c->enabled == enabled) return;
    c->enabled = enabled;
    c->tx_pending = false;
    if (enabled) occupancy_resync(c, now);
}
void occupancy_delay(occupancy_control *c, uint16_t seconds, uint32_t now) {
    c->off_delay_seconds = seconds;
    if (c->off_pending) {
        c->off_deadline = c->clear_since + (uint32_t)seconds * 1000u;
        if (due(now, c->off_deadline)) {
            c->off_pending = false;
            queue(c, false, now);
        }
    }
}
void occupancy_resync(occupancy_control *c, uint32_t now) {
    if (!c->enabled || !c->known) return;
    /* Retain a running local clear delay across a network reconnect. */
    if (c->off_pending && !due(now, c->off_deadline)) return;
    if (!c->occupied || c->on_latched) queue(c, c->occupied, now);
    else try_on(c, now);
}
void occupancy_tick(occupancy_control *c, uint32_t now,
                    occupancy_send_fn send, void *ctx) {
    if (c->off_pending && due(now, c->off_deadline)) {
        c->off_pending = false;
        queue(c, false, now);
    }
    if (!c->enabled || !c->tx_pending || !due(now, c->next_tx)) return;
    c->attempts++;
    if (send(ctx, c->desired_on) || c->attempts >= 3) c->tx_pending = false;
    else c->next_tx = now + 1000u;
}
void occupancy_delivery_failed(occupancy_control *c, bool sent_on, uint32_t now) {
    if (!c->enabled || !c->desired_known || c->desired_on != sent_on ||
        c->off_pending || c->attempts >= 3 || c->tx_pending) return;
    c->tx_pending = true;
    c->next_tx = now + 1000u;
}
