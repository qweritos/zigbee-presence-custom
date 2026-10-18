#ifndef BRIDGE_OCCUPANCY_CONTROL_H
#define BRIDGE_OCCUPANCY_CONTROL_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool enabled, known, occupied, desired_on, desired_known;
    bool off_pending, tx_pending;
    bool lux_known, on_latched;
    uint16_t off_delay_seconds, on_lux_threshold;
    uint32_t lux;
    uint8_t attempts;
    uint32_t clear_since, off_deadline, next_tx;
} occupancy_control;
typedef bool (*occupancy_send_fn)(void *, bool);
void occupancy_init(occupancy_control *, bool, uint16_t);
void occupancy_presence(occupancy_control *, bool, uint32_t);
void occupancy_enable(occupancy_control *, bool, uint32_t);
void occupancy_delay(occupancy_control *, uint16_t, uint32_t);
void occupancy_lux_limit(occupancy_control *, uint16_t, uint32_t);
void occupancy_illuminance(occupancy_control *, bool, uint32_t, uint32_t);
void occupancy_resync(occupancy_control *, uint32_t);
void occupancy_delivery_failed(occupancy_control *, bool, uint32_t);
void occupancy_tick(occupancy_control *, uint32_t, occupancy_send_fn, void *);
#endif
