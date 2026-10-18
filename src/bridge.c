#include "bridge.h"
#include "core/routes.h"
#include "core/tuya_protocol.h"
#include "hal/zigbee.h"
#include "hal/zigbee_ota.h"
#include "platform/uart.h"
#include "app/framework/include/af.h"
#include "sl_sleeptimer.h"
#include "nvm3.h"
#include "nvm3_default.h"
#include <math.h>
#include <string.h>

#define CONFIG_CLUSTER 0xfc10u
#define DIAG_CLUSTER 0xfc11u
#define RADAR_CLUSTER 0xfc20u
#define ROUTE_KEY 0x3000u /* Application namespace; Zigbee tokens use 0x1xxxx. */
#define GLOBAL_KEY 0x3010u
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define ATTR(id,type,flags,ptr) {id,type,flags,sizeof(*(ptr)),(uint8_t *)(ptr)}

static bridge_route routes[BRIDGE_ROUTE_COUNT];
static hal_zigbee_endpoint endpoints[BRIDGE_ROUTE_COUNT];
static hal_zigbee_cluster clusters[BRIDGE_ROUTE_COUNT][8];
static hal_zigbee_attribute route_attrs[BRIDGE_ROUTE_COUNT][12];
static hal_zigbee_attribute occupancy_attrs[BRIDGE_ROUTE_COUNT][3];
static uint8_t occupancy[BRIDGE_ROUTE_COUNT], known[BRIDGE_ROUTE_COUNT];
/* A mapped MCU input does not establish a PIR/ultrasonic sensing technology. */
static uint8_t sensor_type=0xff, sensor_bitmap=0, binding_count[BRIDGE_ROUTE_COUNT];
static uint32_t binding_hash[BRIDGE_ROUTE_COUNT];
static tuya_parser parser;
static uint32_t last_uart_rx;
static uint16_t sequence;
static uint32_t now_ms(void) {return sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());}
static uint8_t zcl_version=8, app_version=1, stack_version=2, hw_version=1, power_source=4;
static uint8_t manufacturer[]="\x0fOpenZigbee-MCU"; /* Fixed below at init using strlen. */
#ifdef BRIDGE_PROFILE_ZTC6GGYL
static uint8_t model[]="\0TuyaMCU-Bridge-ZTC6";
#else
static uint8_t model[]="\0TuyaMCU-Bridge";
#endif
static uint8_t sw_build[]="\x05" "0.2.0";
static uint16_t revision=1;
static uint32_t tx_queued, tx_delivered, tx_failed, serial_tx_dropped, nvm_failed, dp_reports, version_rejected;
static uint8_t last_dp, last_type, mcu_version, pid_seen;
static uint32_t last_value;
static uint16_t illuminance=0xffff;
static uint8_t illuminance_dp;
static uint8_t net_status=0xff;
static uint32_t next_join, next_handshake, next_bind_check;
static uint8_t handshake_attempts;
static bool previous_joined;
#ifdef BRIDGE_ENABLE_DEBUG
/* Only in explicitly requested diagnostic builds. No radio command exposes it. */
volatile struct {
    uint32_t magic, version, command, status;
    uint32_t endpoint, dp, type, value, enabled, delay, active, clear;
    uint32_t ticks, frames, queued, delivered, failed, network;
    uint32_t query_received, query_value, query_source, query_source_ep, query_destination_ep;
    uint32_t send_index;
    struct {uint32_t time,endpoint,state;} sent[8];
} bridge_debug={.magic=0x4d435542,.version=1};
#endif
#ifdef BRIDGE_PROFILE_ZTC6GGYL
static uint8_t radar_dp[]={2,3,4,101,102};
static uint32_t radar_value[5]={UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
static uint8_t radar_led=0xff;
static float distance_cm=NAN;
#endif

static hal_zigbee_attribute basic_attrs[]={
    ATTR(0x0000,0x20,ATTR_READONLY,&zcl_version),
    ATTR(0x0001,0x20,ATTR_READONLY,&app_version),
    ATTR(0x0002,0x20,ATTR_READONLY,&stack_version),
    ATTR(0x0003,0x20,ATTR_READONLY,&hw_version),
    {0x0004,0x42,ATTR_READONLY,sizeof(manufacturer),manufacturer},
    {0x0005,0x42,ATTR_READONLY,sizeof(model),model},
    ATTR(0x0007,0x30,ATTR_READONLY,&power_source),
    {0x4000,0x42,ATTR_READONLY,sizeof(sw_build),sw_build},
    ATTR(0xfffd,0x21,ATTR_READONLY,&revision),
};
static hal_zigbee_attribute lux_attrs[]={ATTR(0x0000,0x21,ATTR_READONLY,&illuminance)};
static hal_zigbee_attribute diag_attrs[]={
    ATTR(0x0000,0x23,ATTR_READONLY,&parser.frames),
    ATTR(0x0001,0x23,ATTR_READONLY,&parser.bad_checksums),
    /* UART volatile counters copied into an ordinary snapshot below. */
    ATTR(0x0003,0x23,ATTR_READONLY,&tx_queued),
    ATTR(0x0004,0x23,ATTR_READONLY,&tx_failed),
    ATTR(0x0005,0x23,ATTR_READONLY,&nvm_failed),
    ATTR(0x0006,0x20,ATTR_READONLY,&last_dp),
    ATTR(0x0007,0x30,ATTR_READONLY,&last_type),
    ATTR(0x0008,0x23,ATTR_READONLY,&last_value),
    ATTR(0x0009,0x20,ATTR_READONLY,&mcu_version),
    ATTR(0x000a,0x23,ATTR_READONLY,&tx_delivered),
    ATTR(0x000b,0x23,ATTR_READONLY,&serial_tx_dropped),
    ATTR(0x000c,0x23,ATTR_READONLY,&dp_reports),
    ATTR(0x000d,0x23,ATTR_READONLY,&version_rejected),
    ATTR(0x000e,0x23,ATTR_READONLY,&parser.timeouts),
    ATTR(0x0010,0x20,ATTR_WRITABLE,&illuminance_dp),
};
static uint32_t rx_snapshot, overflow_snapshot, uart_error_snapshot;
static hal_zigbee_attribute uart_attrs[]={
    ATTR(0x0020,0x23,ATTR_READONLY,&rx_snapshot),
    ATTR(0x0021,0x23,ATTR_READONLY,&overflow_snapshot),
    ATTR(0x0022,0x23,ATTR_READONLY,&uart_error_snapshot),
};
static hal_zigbee_attribute all_diag_attrs[ARRAY_SIZE(diag_attrs)+ARRAY_SIZE(uart_attrs)];
#ifdef BRIDGE_PROFILE_ZTC6GGYL
static hal_zigbee_attribute radar_attrs[]={
    ATTR(0x0002,0x23,ATTR_WRITABLE,&radar_value[0]),
    ATTR(0x0003,0x23,ATTR_WRITABLE,&radar_value[1]),
    ATTR(0x0004,0x23,ATTR_WRITABLE,&radar_value[2]),
    ATTR(0x0065,0x10,ATTR_WRITABLE,&radar_led),
    ATTR(0x0066,0x23,ATTR_WRITABLE,&radar_value[4]),
    ATTR(0x0009,0x39,ATTR_READONLY,&distance_cm),
};
#endif

static bool serial_send(uint16_t seq,uint8_t cmd,const uint8_t *p,size_t n) {
    uint8_t buf[TUYA_MAX_FRAME];
    size_t size=tuya_encode(buf,sizeof(buf),2,seq,cmd,p,n);
    if (!size || !mcu_uart_write(buf,size)) {serial_tx_dropped++; return false;}
    return true;
}
static bool request(uint8_t cmd,const uint8_t *p,size_t n) {
    sequence=(sequence>=0xfff0u)?0:(uint16_t)(sequence+1u);
    return serial_send(sequence,cmd,p,n);
}
static void change(uint8_t ep,uint16_t cluster,uint16_t attr) {
    hal_zigbee_notify_attribute_changed(ep,cluster,attr);
}

/* A valid DP list is decoded atomically by the protocol layer before here. */
static void on_dp(void *ctx,uint8_t dp,uint8_t type,const uint8_t *v,size_t n) {
    (void)ctx;
    uint32_t value=0;
    for (size_t j=0;j<n && j<4;j++) value=(value<<8)|v[j];
    last_dp=dp;last_type=type;last_value=value;dp_reports++;
    uint32_t now=now_ms();
    if (dp && dp==illuminance_dp && type==2 && n==4) {
        for (unsigned i=0;i<BRIDGE_ROUTE_COUNT;i++)
            occupancy_illuminance(&routes[i].control,true,value,now);
        uint16_t measured=value?(uint16_t)fminf(65534.0f,10000.0f*log10f((float)value)+1.0f):0;
        if (illuminance!=measured) {illuminance=measured;change(1,0x0400,0);}
    }
    for (unsigned i=0;i<BRIDGE_ROUTE_COUNT;i++) {
        bool old_known=routes[i].control.known;
        bool old_state=routes[i].control.occupied;
        route_report(&routes[i],dp,type,v,n,now);
        known[i]=routes[i].control.known;
        occupancy[i]=routes[i].control.occupied;
        if (old_known!=known[i] || old_state!=occupancy[i]) change(i+1,0x0406,0);
    }
#ifdef BRIDGE_PROFILE_ZTC6GGYL
    for (unsigned j=0;j<ARRAY_SIZE(radar_dp);j++) {
        if (dp==radar_dp[j] && ((dp==101 && type==1 && n==1 && value<=1) || (dp!=101 && type==2 && n==4))) {
            if (radar_value[j]!=value) {
                radar_value[j]=value;
                if (dp==101) radar_led=value;
                change(1,RADAR_CLUSTER,dp);
            }
        }
    }
    if (dp==9 && type==2 && n==4 && distance_cm!=(float)value) {
        distance_cm=(float)value;change(1,RADAR_CLUSTER,9);
    }
#endif
}
static uint8_t current_network_status(void) {
    hal_zigbee_network_status_t n=hal_zigbee_get_network_status();
    return n==HAL_ZIGBEE_NETWORK_JOINED?1:n==HAL_ZIGBEE_NETWORK_JOINING?3:0;
}
static void on_frame(void *ctx,uint8_t version,uint16_t seq,uint8_t cmd,const uint8_t *p,size_t n) {
    (void)ctx;
    if (version!=2) {version_rejected++; return;}
    switch (cmd) {
    case 0x01: if (n) pid_seen=1; break;
    case 0x05: case 0x06: {
        uint8_t ok=tuya_decode_dps(p,n,on_dp,NULL)?1:0;
        serial_send(seq,cmd,&ok,1);
        break;
    }
    case 0x03:
        if (n==1 && p[0]<=1) {
            serial_send(seq,cmd,NULL,0);
            if (p[0]==1) {hal_zigbee_leave_network();next_join=now_ms()+1000;}
            /* A short MCU reset request is acknowledged without erasing routes. */
        }
        break;
    case 0x0b: if (n==1) mcu_version=p[0]; break;
    case 0x20: {
        uint8_t status=current_network_status();serial_send(seq,cmd,&status,1);break;
    }
    /* Acknowledge network status responses by doing nothing (no ACK loop). */
    default: break;
    }
}

static bool send_route(void *ctx,bool on) {
    unsigned i=(unsigned)(uintptr_t)ctx;
    if (!binding_count[i]) return false;
    hal_zigbee_cmd cmd={.endpoint=i+1,.profile_id=0x0104,.cluster_id=6,
        .command_id=on?1:0,.cluster_specific=1,.disable_default_rsp=1};
    if (hal_zigbee_send_cmd_to_bindings(&cmd)!=HAL_ZIGBEE_OK) return false;
#ifdef BRIDGE_ENABLE_DEBUG
    unsigned index=bridge_debug.send_index++%8;
    bridge_debug.sent[index].time=now_ms();bridge_debug.sent[index].endpoint=i+1;
    bridge_debug.sent[index].state=on;
#endif
    tx_queued++;return true;
}
bool sl_zigbee_af_message_sent_cb(sl_zigbee_outgoing_message_type_t type,uint16_t index,
        sl_zigbee_aps_frame_t *aps,uint16_t n,uint8_t *message,sl_status_t status) {
    (void)type;(void)index;
    if (aps->clusterId==6 && aps->sourceEndpoint>=1 && aps->sourceEndpoint<=BRIDGE_ROUTE_COUNT && n>=3 && (message[0]&3)==1) {
        unsigned offset=(message[0]&4)?4:2;
        if (n<=offset) return false;
        uint8_t command=message[offset];
        if (command<=1) {
            if (status==SL_STATUS_OK) tx_delivered++;
            else {
                tx_failed++;
                occupancy_delivery_failed(&routes[aps->sourceEndpoint-1].control,command==1,now_ms());
            }
        }
    }
    return false;
}
#ifdef BRIDGE_ENABLE_DEBUG
bool sl_zigbee_af_read_attributes_response_cb(uint16_t cluster,uint8_t *data,uint16_t size) {
    if (cluster==6 && size>=5 && data[0]==0 && data[1]==0 && data[2]==0 && data[3]==0x10 && data[4]<=1) {
        sl_zigbee_af_cluster_command_t *command=sl_zigbee_af_current_command();
        bridge_debug.query_value=data[4];bridge_debug.query_source=command->source;
        bridge_debug.query_source_ep=command->apsFrame->sourceEndpoint;
        bridge_debug.query_destination_ep=command->apsFrame->destinationEndpoint;
        bridge_debug.query_received++;
    }
    return false;
}
#endif
static void scan_bindings(uint32_t now) {
    uint32_t hashes[BRIDGE_ROUTE_COUNT]={0};uint8_t counts[BRIDGE_ROUTE_COUNT]={0};
    for (uint8_t j=0;j<SL_ZIGBEE_BINDING_TABLE_SIZE;j++) {
        sl_zigbee_binding_table_entry_t b;
        if (sl_zigbee_get_binding(j,&b)!=SL_STATUS_OK || b.type!=SL_ZIGBEE_UNICAST_BINDING ||
            b.clusterId!=6 || b.local<1 || b.local>BRIDGE_ROUTE_COUNT) continue;
        unsigned i=b.local-1;counts[i]++;
        hashes[i]=(hashes[i]*16777619u)^b.remote;
        for (unsigned k=0;k<8;k++) hashes[i]=(hashes[i]*16777619u)^b.identifier[k];
    }
    for (unsigned i=0;i<BRIDGE_ROUTE_COUNT;i++) {
        binding_count[i]=counts[i];
        if (hashes[i]!=binding_hash[i]) {
            binding_hash[i]=hashes[i];
            if (counts[i]) occupancy_resync(&routes[i].control,now);
        }
    }
}
void bridge_attribute_read(void) {
    rx_snapshot=mcu_uart_rx_bytes;overflow_snapshot=mcu_uart_overflows;uart_error_snapshot=mcu_uart_errors;
}

uint8_t bridge_attribute_write(uint8_t ep,uint16_t cluster,uint16_t attr,const uint8_t *p,uint8_t size) {
    if (cluster==CONFIG_CLUSTER && ep>=1 && ep<=BRIDGE_ROUTE_COUNT) {
        unsigned i=ep-1;route_config c=routes[i].config;
        void *dest=NULL;unsigned expected=0;
        switch (attr) {
        case 0:dest=&c.enabled;expected=1;break;
        case 1:dest=&c.dp;expected=1;break;
        case 2:dest=&c.type;expected=1;break;
        case 3:dest=&c.active_value;expected=4;break;
        case 4:dest=&c.clear_value;expected=4;break;
        case 5:dest=&c.off_delay_seconds;expected=2;break;
        case 6:dest=&c.on_lux_threshold;expected=2;break;
        default:return SL_ZIGBEE_ZCL_STATUS_READ_ONLY;
        }
        if (size!=expected) return SL_ZIGBEE_ZCL_STATUS_INVALID_DATA_TYPE;
        memcpy(dest,p,size);
        if (!route_config_valid(&c)) return SL_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
        uint8_t encoded[BRIDGE_CONFIG_SIZE];route_config_encode(&c,encoded);
        if (nvm3_writeData(nvm3_defaultHandle,ROUTE_KEY+i,encoded,sizeof(encoded))!=ECODE_NVM3_OK) {
            nvm_failed++;return SL_ZIGBEE_ZCL_STATUS_FAILURE;
        }
        route_configure(&routes[i],&c,now_ms());
        known[i]=routes[i].control.known;
        return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
    }
    if (ep==1 && cluster==DIAG_CLUSTER && attr==0x10 && size==1) {
        if (nvm3_writeData(nvm3_defaultHandle,GLOBAL_KEY,p,1)!=ECODE_NVM3_OK) {
            nvm_failed++;return SL_ZIGBEE_ZCL_STATUS_FAILURE;
        }
        illuminance=0xffff;
        for (unsigned i=0;i<BRIDGE_ROUTE_COUNT;i++)
            occupancy_illuminance(&routes[i].control,false,0,now_ms());
        return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
    }
#ifdef BRIDGE_PROFILE_ZTC6GGYL
    if (ep==1 && cluster==RADAR_CLUSTER) {
        uint32_t value=0;uint8_t type=2;
        if (attr==101 && size==1) {value=p[0];type=1;if (value>1) return SL_ZIGBEE_ZCL_STATUS_INVALID_VALUE;}
        else if (size==4 && (attr==2 || attr==3 || attr==4 || attr==102)) {
            memcpy(&value,p,4);
            if ((attr==2 && (value<1 || value>10)) || (attr==102 && value>100) ||
                ((attr==3 || attr==4) && value>950)) return SL_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
        } else return SL_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
        uint8_t dp[8]={(uint8_t)attr,type,0,(type==1)?1:4,0,0,0,0};
        unsigned n=type==1?1:4;
        for (unsigned j=0;j<n;j++) dp[4+j]=value>>(8*(n-j-1));
        /* MCU reports establish actual state; do not optimistically overwrite it. */
        return request(4,dp,4+n)?0xfe:SL_ZIGBEE_ZCL_STATUS_FAILURE;
    }
#endif
    return 0xff; /* Delegate standard cluster writes to the SDK/HAL. */
}

void bridge_init(void) {
    manufacturer[0]=strlen((char *)manufacturer+1);
    model[0]=strlen((char *)model+1);
    /* Hex escapes must be separated from following numeric text. */
    sw_build[0]=5;
#ifdef BRIDGE_PROFILE_ZTC6GGYL
    illuminance_dp=104;
#endif
    nvm3_readData(nvm3_defaultHandle,GLOBAL_KEY,&illuminance_dp,1);
    /* Migrate the former multi-endpoint layout without touching EP1 or network tokens. */
    for (unsigned i=1;i<4;i++) {
        Ecode_t result=nvm3_deleteObject(nvm3_defaultHandle,ROUTE_KEY+i);
        if (result!=ECODE_NVM3_OK && result!=ECODE_NVM3_ERR_KEY_NOT_FOUND) nvm_failed++;
    }
    for (uint8_t j=0;j<SL_ZIGBEE_BINDING_TABLE_SIZE;j++) {
        sl_zigbee_binding_table_entry_t b;
        if (sl_zigbee_get_binding(j,&b)==SL_STATUS_OK && b.type!=SL_ZIGBEE_UNUSED_BINDING &&
            b.local>=2 && b.local<=4 && sl_zigbee_delete_binding(j)!=SL_STATUS_OK) nvm_failed++;
    }
    memcpy(all_diag_attrs,diag_attrs,sizeof(diag_attrs));
    memcpy(all_diag_attrs+ARRAY_SIZE(diag_attrs),uart_attrs,sizeof(uart_attrs));
    for (unsigned i=0;i<BRIDGE_ROUTE_COUNT;i++) {
        route_config c={.type=4,.active_value=1};
#ifdef BRIDGE_PROFILE_ZTC6GGYL
        if (i==0) {c.enabled=1;c.dp=1;}
#endif
        uint8_t encoded[BRIDGE_CONFIG_SIZE];route_config saved;
        if (nvm3_readData(nvm3_defaultHandle,ROUTE_KEY+i,encoded,sizeof(encoded))==ECODE_NVM3_OK &&
            route_config_decode(&saved,encoded,sizeof(encoded))) c=saved;
        route_init(&routes[i],&c);
        route_config *r=&routes[i].config;
        hal_zigbee_attribute ra[]={
            ATTR(0,0x10,ATTR_WRITABLE,&r->enabled),ATTR(1,0x20,ATTR_WRITABLE,&r->dp),
            ATTR(2,0x30,ATTR_WRITABLE,&r->type),ATTR(3,0x23,ATTR_WRITABLE,&r->active_value),
            ATTR(4,0x23,ATTR_WRITABLE,&r->clear_value),ATTR(5,0x21,ATTR_WRITABLE,&r->off_delay_seconds),
            ATTR(6,0x21,ATTR_WRITABLE,&r->on_lux_threshold),
            ATTR(0x10,0x23,ATTR_READONLY,&routes[i].input_value),ATTR(0x11,0x10,ATTR_READONLY,&known[i]),
            ATTR(0x12,0x20,ATTR_READONLY,&binding_count[i]),
            ATTR(0x13,0x23,ATTR_READONLY,&routes[i].matched_reports),
            ATTR(0x14,0x23,ATTR_READONLY,&routes[i].ignored_values),
        };
        memcpy(route_attrs[i],ra,sizeof(ra));
        hal_zigbee_attribute oa[]={ATTR(0,0x18,ATTR_READONLY,&occupancy[i]),
            ATTR(1,0x30,ATTR_READONLY,&sensor_type),ATTR(2,0x18,ATTR_READONLY,&sensor_bitmap)};
        memcpy(occupancy_attrs[i],oa,sizeof(oa));
        unsigned n=0;
        clusters[i][n++]=(hal_zigbee_cluster){.cluster_id=CONFIG_CLUSTER,.is_server=1,
            .attribute_count=ARRAY_SIZE(ra),.attributes=route_attrs[i]};
        clusters[i][n++]=(hal_zigbee_cluster){.cluster_id=0x0406,.is_server=1,
            .attribute_count=ARRAY_SIZE(oa),.attributes=occupancy_attrs[i]};
        clusters[i][n++]=(hal_zigbee_cluster){.cluster_id=6,.is_server=0};
        if (i==0) {
            clusters[i][n++]=(hal_zigbee_cluster){.cluster_id=0,.is_server=1,
                .attribute_count=ARRAY_SIZE(basic_attrs),.attributes=basic_attrs};
            clusters[i][n++]=(hal_zigbee_cluster){.cluster_id=0x0400,.is_server=1,
                .attribute_count=ARRAY_SIZE(lux_attrs),.attributes=lux_attrs};
            clusters[i][n++]=(hal_zigbee_cluster){.cluster_id=DIAG_CLUSTER,.is_server=1,
                .attribute_count=ARRAY_SIZE(all_diag_attrs),.attributes=all_diag_attrs};
#ifdef BRIDGE_PROFILE_ZTC6GGYL
            clusters[i][n++]=(hal_zigbee_cluster){.cluster_id=RADAR_CLUSTER,.is_server=1,
                .attribute_count=ARRAY_SIZE(radar_attrs),.attributes=radar_attrs};
#endif
            hal_ota_cluster_setup(&clusters[i][n++]);
        }
        endpoints[i]=(hal_zigbee_endpoint){.endpoint=i+1,.profile_id=0x0104,.device_id=0x0107,
            .device_version=1,.cluster_count=n,.clusters=clusters[i]};
    }
    hal_zigbee_init(endpoints,BRIDGE_ROUTE_COUNT);
    mcu_uart_init();
}
void bridge_process(void) {
    uint32_t now=now_ms();uint8_t b;
#ifdef BRIDGE_ENABLE_DEBUG
    bridge_debug.ticks=now;bridge_debug.frames=parser.frames;
    bridge_debug.queued=tx_queued;bridge_debug.delivered=tx_delivered;
    bridge_debug.failed=tx_failed;bridge_debug.network=current_network_status();
    uint32_t command=bridge_debug.command;
    if (command) {
        bridge_debug.command=0;bridge_debug.status=1;
        if (command==1) {hal_zigbee_leave_network();next_join=now+1000;}
        else if (command==2) {
            uint8_t dp[8]={(uint8_t)bridge_debug.dp,(uint8_t)bridge_debug.type,0,0};
            unsigned n=(dp[1]==1 || dp[1]==4)?1:4;
            dp[3]=n;
            for (unsigned j=0;j<n;j++) dp[4+j]=bridge_debug.value>>(8*(n-j-1));
            uint8_t frame[TUYA_MAX_FRAME];
            size_t length=tuya_encode(frame,sizeof(frame),2,0xfff0,6,dp,4+n);
            /* A synthetic byte stream must not interleave a partial UART frame. */
            tuya_parser injected={0};
            for (size_t j=0;j<length;j++) tuya_parser_feed(&injected,frame[j],on_frame,NULL);
        } else if (command==3 && bridge_debug.endpoint>=1 && bridge_debug.endpoint<=BRIDGE_ROUTE_COUNT) {
            unsigned i=bridge_debug.endpoint-1;
            route_config c={.dp=bridge_debug.dp,.type=bridge_debug.type,.enabled=bridge_debug.enabled,
                .off_delay_seconds=bridge_debug.delay,.active_value=bridge_debug.active,.clear_value=bridge_debug.clear};
            uint8_t data[BRIDGE_CONFIG_SIZE];route_config_encode(&c,data);
            if (!route_config_valid(&c)) bridge_debug.status=2;
            else if (nvm3_writeData(nvm3_defaultHandle,ROUTE_KEY+i,data,sizeof(data))!=ECODE_NVM3_OK) bridge_debug.status=3;
            else {route_configure(&routes[i],&c,now);known[i]=routes[i].control.known;}
        } else if (command==4 && bridge_debug.endpoint>=1 && bridge_debug.endpoint<=BRIDGE_ROUTE_COUNT) {
            const uint8_t attr[]={0,0};
            hal_zigbee_cmd query={.endpoint=bridge_debug.endpoint,.profile_id=0x0104,.cluster_id=6,
                .command_id=0,.cluster_specific=0,.disable_default_rsp=1,.payload=attr,.payload_len=2};
            if (hal_zigbee_send_cmd_to_bindings(&query)!=HAL_ZIGBEE_OK) bridge_debug.status=5;
        } else bridge_debug.status=4;
    }
#endif
    /* Limit each pass so malformed/continuous UART cannot starve the radio. */
    if ((uint32_t)(now-last_uart_rx)>200) tuya_parser_timeout(&parser);
    for (unsigned i=0;i<256 && mcu_uart_read(&b);i++) {
        last_uart_rx=now;tuya_parser_feed(&parser,b,on_frame,NULL);
    }
    uint8_t status=current_network_status();bool joined=status==1;
    if (status!=net_status) {if (request(2,&status,1)) net_status=status;}
    if (joined && !previous_joined) {
        for (unsigned i=0;i<BRIDGE_ROUTE_COUNT;i++) occupancy_resync(&routes[i].control,now);
        hal_zigbee_send_announce();
    }
    previous_joined=joined;
    if (!status && (int32_t)(now-next_join)>=0) {
        next_join=now+30000;hal_zigbee_start_network_steering();
    }
    if ((int32_t)(now-next_handshake)>=0 && handshake_attempts<3) {
        if (!pid_seen) request(1,NULL,0);
        request(0x0b,NULL,0);request(0x28,NULL,0);
        handshake_attempts++;next_handshake=now+3000;
    }
    if ((int32_t)(now-next_bind_check)>=0) {scan_bindings(now);next_bind_check=now+1000;}
    for (unsigned i=0;i<BRIDGE_ROUTE_COUNT;i++) occupancy_tick(&routes[i].control,now,send_route,(void *)(uintptr_t)i);
}
