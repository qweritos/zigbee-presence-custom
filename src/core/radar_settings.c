#include "radar_settings.h"
#include <math.h>
radar_settings radar_settings_defaults(void) {
    return (radar_settings){{8,0,525,100}};
}
bool radar_settings_valid(const radar_settings *s) {
    return s && s->value[0]>=1 && s->value[0]<=10 && s->value[1]<=950 &&
        s->value[2]<=950 && s->value[1]<=s->value[2] && s->value[3]<=100;
}
void radar_settings_encode(const radar_settings *s,uint8_t out[RADAR_SETTINGS_SIZE]) {
    out[0]=1;
    for (unsigned i=0;i<4;i++) for (unsigned j=0;j<4;j++) out[1+4*i+j]=s->value[i]>>(8*j);
}
bool radar_settings_decode(radar_settings *s,const uint8_t *data,size_t size) {
    if (!s || !data || size!=RADAR_SETTINGS_SIZE || data[0]!=1) return false;
    radar_settings decoded={{0}};
    for (unsigned i=0;i<4;i++) for (unsigned j=0;j<4;j++) decoded.value[i]|=(uint32_t)data[1+4*i+j]<<(8*j);
    if (!radar_settings_valid(&decoded)) return false;
    *s=decoded;return true;
}
uint16_t radar_illuminance_zcl(uint32_t lux) {
    return lux?(uint16_t)fminf(65534.f,10000.f*log10f((float)lux)+1.f):0;
}
