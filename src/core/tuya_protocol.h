#ifndef BRIDGE_TUYA_PROTOCOL_H
#define BRIDGE_TUYA_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TUYA_MAX_PAYLOAD 192u
#define TUYA_MAX_FRAME (TUYA_MAX_PAYLOAD + 9u)
typedef struct {
    uint8_t bytes[TUYA_MAX_FRAME];
    size_t used;
    uint32_t frames, bad_checksums, bad_lengths, timeouts;
} tuya_parser;
typedef void (*tuya_frame_fn)(void *, uint8_t, uint16_t, uint8_t,
                              const uint8_t *, size_t);
typedef void (*tuya_dp_fn)(void *, uint8_t, uint8_t, const uint8_t *, size_t);
void tuya_parser_feed(tuya_parser *, uint8_t, tuya_frame_fn, void *);
void tuya_parser_timeout(tuya_parser *);
size_t tuya_encode(uint8_t *, size_t, uint8_t, uint16_t, uint8_t,
                   const uint8_t *, size_t);
bool tuya_decode_dps(const uint8_t *, size_t, tuya_dp_fn, void *);
uint32_t tuya_be32(const uint8_t *);
#endif
