#ifndef RADAR_SETTINGS_H
#define RADAR_SETTINGS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RADAR_SETTINGS_SIZE 17u
/* Motion sensitivity, minimum cm, maximum cm, presence sensitivity (raw). */
typedef struct {uint32_t value[4];} radar_settings;
radar_settings radar_settings_defaults(void);
bool radar_settings_valid(const radar_settings *s);
void radar_settings_encode(const radar_settings *s,uint8_t out[RADAR_SETTINGS_SIZE]);
bool radar_settings_decode(radar_settings *s,const uint8_t *data,size_t size);
uint16_t radar_illuminance_zcl(uint32_t lux);
#endif
