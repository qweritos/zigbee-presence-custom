#include "core/radar_settings.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    radar_settings s=radar_settings_defaults(),decoded;
    assert(s.value[0]==8 && s.value[1]==0 && s.value[2]==525 && s.value[3]==100);
    uint8_t encoded[RADAR_SETTINGS_SIZE];radar_settings_encode(&s,encoded);
    assert(radar_settings_decode(&decoded,encoded,sizeof(encoded)));
    for (unsigned i=0;i<4;i++) assert(decoded.value[i]==s.value[i]);
    encoded[0]=2;assert(!radar_settings_decode(&decoded,encoded,sizeof(encoded)));
    encoded[0]=1;assert(!radar_settings_decode(&decoded,encoded,sizeof(encoded)-1));
    s.value[0]=0;assert(!radar_settings_valid(&s));s.value[0]=11;assert(!radar_settings_valid(&s));
    s=radar_settings_defaults();s.value[1]=526;assert(!radar_settings_valid(&s));
    s=radar_settings_defaults();s.value[2]=951;assert(!radar_settings_valid(&s));
    s=radar_settings_defaults();s.value[3]=101;assert(!radar_settings_valid(&s));
    assert(radar_illuminance_zcl(0)==0 && radar_illuminance_zcl(1)==1);
    assert(radar_illuminance_zcl(10)==10001 && radar_illuminance_zcl(1000)==30001);
    assert(radar_illuminance_zcl(1006)==30026 && radar_illuminance_zcl(UINT32_MAX)==65534);
    puts("radar defaults, persistence validation and stock lux conversion: PASS");
}
