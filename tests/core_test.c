#include "core/tuya_protocol.h"
#include "core/occupancy_control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned frames, dps;
static uint8_t command, dp_value;
static void frame(void *ctx, uint8_t version, uint16_t seq, uint8_t cmd,
                  const uint8_t *data, size_t length) {
    (void)ctx; (void)version; (void)seq; (void)data; (void)length;
    frames++; command = cmd;
}
static void dp(void *ctx, uint8_t id, uint8_t type,
               const uint8_t *data, size_t length) {
    (void)ctx; (void)id; (void)type;
    dps++; if (length) dp_value = data[0];
}
static void feed(tuya_parser *p, const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; i++) tuya_parser_feed(p, data[i], frame, NULL);
}
static const uint8_t occupied[] = {
    0x55,0xaa,0x02,0x00,0x01,0x06,0x00,0x05,0x01,0x04,0x00,0x01,0x01,0x14
};
static void protocol_tests(void) {
    tuya_parser p = {0};
    frames = 0;
    feed(&p, occupied, 7); assert(frames == 0);
    feed(&p, occupied + 7, sizeof(occupied) - 7);
    assert(frames == 1 && command == 6 && p.frames == 1);
    uint8_t corrupt[sizeof(occupied)]; memcpy(corrupt, occupied, sizeof(corrupt));
    corrupt[sizeof(corrupt)-1] ^= 1;
    feed(&p, corrupt, sizeof(corrupt)); assert(frames == 1);
    feed(&p, occupied, sizeof(occupied)); assert(frames == 2 && p.bad_checksums);
    const uint8_t oversized[] = {0x55,0xaa,2,0,0,6,1,0};
    feed(&p, oversized, sizeof(oversized));
    feed(&p, occupied, sizeof(occupied)); assert(frames == 3 && p.bad_lengths);
    /* A truncated plausible length cannot hold subsequent reports indefinitely. */
    const uint8_t partial[] = {0x55,0xaa,2,0,0,6,0,100,1,2};
    feed(&p,partial,sizeof(partial));assert(p.used==sizeof(partial));
    tuya_parser_timeout(&p);assert(p.used==0 && p.timeouts==1);
    tuya_parser_timeout(&p);assert(p.timeouts==1);
    feed(&p,occupied,sizeof(occupied));assert(frames==4);
    dps = 0;
    assert(tuya_decode_dps(occupied+8,5,dp,NULL));
    assert(dps == 1 && dp_value == 1);
    const uint8_t malformed_tail[] = {1,4,0,1,1,2,2,0,4,0};
    dps = 0;
    assert(!tuya_decode_dps(malformed_tail,sizeof(malformed_tail),dp,NULL));
    assert(dps == 0);
    const uint8_t invalid_integer[] = {2,2,0,3,0,0,1};
    assert(!tuya_decode_dps(invalid_integer,sizeof(invalid_integer),dp,NULL));
    uint8_t encoded[TUYA_MAX_FRAME];
    size_t n = tuya_encode(encoded,sizeof(encoded),2,1,6,occupied+8,5);
    assert(n == sizeof(occupied) && memcmp(encoded,occupied,n) == 0);
    assert(tuya_encode(encoded,3,2,1,6,NULL,0) == 0);
    assert(tuya_encode(encoded,sizeof(encoded),2,1,6,NULL,1) == 0);
    const uint8_t byte_order[] = {0x12,0x34,0x56,0x78};
    assert(tuya_be32(byte_order) == 0x12345678u);
    uint32_t random = 1;
    unsigned before = frames;
    for (unsigned i = 0; i < 100000; i++) {
        random = random * 1664525u + 1013904223u;
        tuya_parser_feed(&p, random >> 24, frame, NULL);
        if (i % 1024 == 0) feed(&p, occupied, sizeof(occupied));
        assert(p.used <= sizeof(p.bytes));
    }
    for (unsigned i = 0; i < TUYA_MAX_FRAME; i++) tuya_parser_feed(&p,0,frame,NULL);
    assert(frames >= before + 90);
}

typedef struct { unsigned count; bool values[32]; bool succeed; } sent_commands;
static bool send_command(void *ctx, bool on) {
    sent_commands *s = ctx; assert(s->count < 32);
    s->values[s->count++] = on; return s->succeed;
}
static void control_tests(void) {
    occupancy_control c;
    sent_commands s = {.succeed=true};
    occupancy_init(&c,true,0);
    occupancy_tick(&c,0,send_command,&s); assert(s.count == 0);
    occupancy_presence(&c,true,100);
    occupancy_tick(&c,100,send_command,&s); assert(s.count == 1 && s.values[0]);
    occupancy_presence(&c,true,200);
    occupancy_tick(&c,200,send_command,&s); assert(s.count == 1);
    occupancy_presence(&c,false,300);
    occupancy_tick(&c,300,send_command,&s); assert(s.count == 2 && !s.values[1]);
    occupancy_init(&c,true,2); s.count = 0;
    occupancy_presence(&c,true,0); occupancy_tick(&c,0,send_command,&s);
    occupancy_presence(&c,false,100);
    occupancy_presence(&c,false,1000); /* periodic MCU repeat cannot extend delay */
    occupancy_tick(&c,2099,send_command,&s); assert(s.count == 1);
    occupancy_tick(&c,2100,send_command,&s); assert(s.count == 2 && !s.values[1]);
    occupancy_presence(&c,true,2200); occupancy_tick(&c,2200,send_command,&s);
    occupancy_presence(&c,false,2300);
    occupancy_presence(&c,true,3000); occupancy_tick(&c,3000,send_command,&s);
    unsigned old = s.count;
    occupancy_tick(&c,5000,send_command,&s); assert(s.count == old);
    occupancy_enable(&c,false,5100);
    occupancy_presence(&c,false,5200); occupancy_tick(&c,8000,send_command,&s);
    assert(s.count == old);
    occupancy_enable(&c,true,9000); occupancy_tick(&c,9000,send_command,&s);
    assert(s.count == old+1 && !s.values[old]);
    occupancy_init(&c,true,2); s.count = 0;
    uint32_t start = UINT32_MAX - 500;
    occupancy_presence(&c,false,start);
    occupancy_tick(&c,start+1999u,send_command,&s); assert(s.count == 0);
    occupancy_tick(&c,start+2000u,send_command,&s); assert(s.count == 1);
    occupancy_init(&c,true,0); s.count = 0; s.succeed = false;
    occupancy_presence(&c,true,0);
    occupancy_tick(&c,0,send_command,&s);
    occupancy_tick(&c,999,send_command,&s); assert(s.count == 1);
    occupancy_tick(&c,1000,send_command,&s);
    occupancy_tick(&c,2000,send_command,&s);
    occupancy_tick(&c,3000,send_command,&s); assert(s.count == 3);
    s.succeed = true;
    occupancy_resync(&c,4000); occupancy_tick(&c,4000,send_command,&s);
    assert(s.count == 4 && s.values[3]);
    /* A clear event cancels a failed pending ON, even with a clear delay. */
    occupancy_init(&c,true,2); s.count = 0; s.succeed = false;
    occupancy_presence(&c,true,0); occupancy_tick(&c,0,send_command,&s);
    occupancy_presence(&c,false,100);
    occupancy_tick(&c,1000,send_command,&s); assert(s.count == 1);
    occupancy_tick(&c,2100,send_command,&s); assert(s.count == 2 && !s.values[1]);
}
static void illuminance_tests(void) {
    occupancy_control c;
    sent_commands s={.succeed=true};
    occupancy_init(&c,true,0);
    occupancy_lux_limit(&c,100,0);
    occupancy_presence(&c,true,1); /* Threshold waits for a known lux input. */
    occupancy_tick(&c,1,send_command,&s);assert(s.count==0);
    occupancy_resync(&c,2);
    occupancy_tick(&c,2,send_command,&s);assert(s.count==0);
    occupancy_illuminance(&c,true,101,3);
    occupancy_tick(&c,3,send_command,&s);assert(s.count==0);
    occupancy_illuminance(&c,true,100,4); /* Equal to the threshold is allowed. */
    occupancy_tick(&c,4,send_command,&s);assert(s.count==1 && s.values[0]);
    occupancy_illuminance(&c,true,5000,5); /* The lamp cannot turn itself Off. */
    occupancy_presence(&c,true,6);
    occupancy_tick(&c,6,send_command,&s);assert(s.count==1);
    occupancy_lux_limit(&c,1,7);
    occupancy_tick(&c,7,send_command,&s);assert(s.count==1);
    occupancy_presence(&c,false,8);
    occupancy_tick(&c,8,send_command,&s);assert(s.count==2 && !s.values[1]);
    occupancy_presence(&c,true,9);occupancy_tick(&c,9,send_command,&s);
    assert(s.count==2); /* New occupancy is blocked while bright. */
    occupancy_lux_limit(&c,0,10); /* Zero bypasses even unknown/bright lux. */
    occupancy_tick(&c,10,send_command,&s);assert(s.count==3 && s.values[2]);
    occupancy_init(&c,true,2);s.count=0;
    occupancy_lux_limit(&c,100,0);occupancy_illuminance(&c,true,10,0);
    occupancy_presence(&c,true,0);occupancy_tick(&c,0,send_command,&s);
    occupancy_illuminance(&c,true,5000,1);
    occupancy_presence(&c,false,100);
    occupancy_presence(&c,true,200); /* Keep On when cancelling delayed Off. */
    occupancy_tick(&c,200,send_command,&s);assert(s.count==2 && s.values[1]);
    occupancy_tick(&c,2100,send_command,&s);assert(s.count==2);
    occupancy_presence(&c,false,2200);
    occupancy_tick(&c,4200,send_command,&s);assert(s.count==3 && !s.values[2]);
    occupancy_presence(&c,true,4300);occupancy_tick(&c,4300,send_command,&s);
    assert(s.count==3);
    occupancy_enable(&c,false,4400);
    occupancy_illuminance(&c,true,10,4500);occupancy_tick(&c,4500,send_command,&s);
    assert(s.count==3);
    occupancy_enable(&c,true,4600);occupancy_tick(&c,4600,send_command,&s);
    assert(s.count==4 && s.values[3]);
    occupancy_init(&c,true,0);s.count=0;
    occupancy_presence(&c,true,0);occupancy_tick(&c,0,send_command,&s);
    assert(s.count==1 && s.values[0]); /* Default zero needs no lux report. */
}
int main(void) {
    protocol_tests(); control_tests(); illuminance_tests();
    puts("PASS: UART framing/checksums/resynchronization/fuzz; atomic DP decoding; occupancy timing/retries/enable/wraparound");
    return 0;
}
