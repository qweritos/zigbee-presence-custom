#include "tuya_protocol.h"
#include <string.h>

static void discard(tuya_parser *p, size_t n) {
    p->used -= n;
    memmove(p->bytes, p->bytes + n, p->used);
}
void tuya_parser_timeout(tuya_parser *p) {
    if (p->used) {p->used=0;p->timeouts++;}
}

void tuya_parser_feed(tuya_parser *p, uint8_t byte, tuya_frame_fn cb, void *ctx) {
    if (p->used == sizeof(p->bytes)) {
        p->bad_lengths++;
        discard(p, 1);
    }
    p->bytes[p->used++] = byte;
    for (;;) {
        if (p->used < 2) return;
        if (p->bytes[0] != 0x55 || p->bytes[1] != 0xaa) {
            discard(p, 1);
            continue;
        }
        if (p->used < 8) return;
        size_t length = ((size_t)p->bytes[6] << 8) | p->bytes[7];
        if (length > TUYA_MAX_PAYLOAD) {
            p->bad_lengths++;
            discard(p, 1);
            continue;
        }
        size_t total = length + 9;
        if (p->used < total) return;
        uint8_t checksum = 0;
        for (size_t i = 0; i < total - 1; i++) checksum += p->bytes[i];
        if (checksum != p->bytes[total - 1]) {
            p->bad_checksums++;
            discard(p, 1);
            continue;
        }
        p->frames++;
        cb(ctx, p->bytes[2], ((uint16_t)p->bytes[3] << 8) | p->bytes[4],
           p->bytes[5], p->bytes + 8, length);
        discard(p, total);
    }
}

size_t tuya_encode(uint8_t *out, size_t capacity, uint8_t version,
                   uint16_t sequence, uint8_t command,
                   const uint8_t *data, size_t length) {
    if (length > TUYA_MAX_PAYLOAD || capacity < length + 9 ||
        (length && data == NULL)) return 0;
    out[0] = 0x55; out[1] = 0xaa; out[2] = version;
    out[3] = sequence >> 8; out[4] = sequence;
    out[5] = command; out[6] = length >> 8; out[7] = length;
    if (length) memcpy(out + 8, data, length);
    uint8_t checksum = 0;
    for (size_t i = 0; i < length + 8; i++) checksum += out[i];
    out[length + 8] = checksum;
    return length + 9;
}

bool tuya_decode_dps(const uint8_t *data, size_t length, tuya_dp_fn cb, void *ctx) {
    /* Validate the whole report before publishing any of its datapoints. */
    for (size_t i = 0; i < length;) {
        if (length - i < 4) return false;
        uint8_t type = data[i + 1];
        size_t n = ((size_t)data[i + 2] << 8) | data[i + 3];
        if (n > length - i - 4 || type > 5 ||
            ((type == 1 || type == 4) && n != 1) ||
            (type == 2 && n != 4) ||
            (type == 5 && n != 1 && n != 2 && n != 4)) return false;
        i += 4 + n;
    }
    for (size_t i = 0; i < length;) {
        size_t n = ((size_t)data[i + 2] << 8) | data[i + 3];
        cb(ctx, data[i], data[i + 1], data + i + 4, n);
        i += 4 + n;
    }
    return true;
}

uint32_t tuya_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
