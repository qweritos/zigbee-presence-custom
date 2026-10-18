#include "app/framework/include/af.h"
#include "hal/zigbee_ota.h"
#include "ota-client-policy-config.h"

static struct OtaData {
    uint64_t upgrade_server_id;
    uint32_t offset;
    uint8_t  status;
    uint32_t current_version, downloaded_version;
    uint16_t manufacturer, image_type, current_stack, downloaded_stack;
} ota_data={.upgrade_server_id=UINT64_MAX,
    .current_version=SL_ZIGBEE_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION,
    .downloaded_version=UINT32_MAX,.manufacturer=0xffff,
    .image_type=SL_ZIGBEE_AF_PLUGIN_OTA_CLIENT_POLICY_IMAGE_TYPE_ID,
    .current_stack=2,.downloaded_stack=0xffff};

#define OTA_ATTR(id,type,field) {id,type,ATTR_WRITABLE,sizeof(ota_data.field),(uint8_t *)&ota_data.field}

hal_zigbee_attribute attrs[] = {
    { .attribute_id = 0x0000,
      .data_type_id = ZCL_IEEE_ADDRESS_ATTRIBUTE_TYPE,
      .size         = 8,
      .value        = (uint8_t *)&ota_data.upgrade_server_id,
      .flag         = ATTR_WRITABLE },
    { .attribute_id = 0x0001,
      .data_type_id = ZCL_INT32U_ATTRIBUTE_TYPE,
      .size         = 4,
      .value        = (uint8_t *)&ota_data.offset,
      .flag         = ATTR_WRITABLE },
    { .attribute_id = 0x0006,
      .data_type_id = ZCL_ENUM8_ATTRIBUTE_TYPE,
      .size         = 1,
      .value        = (uint8_t *)&ota_data.status,
      .flag         = ATTR_WRITABLE },
    OTA_ATTR(0x0002,ZCL_INT32U_ATTRIBUTE_TYPE,current_version),
    OTA_ATTR(0x0003,ZCL_INT16U_ATTRIBUTE_TYPE,current_stack),
    OTA_ATTR(0x0004,ZCL_INT32U_ATTRIBUTE_TYPE,downloaded_version),
    OTA_ATTR(0x0005,ZCL_INT16U_ATTRIBUTE_TYPE,downloaded_stack),
    OTA_ATTR(0x0007,ZCL_INT16U_ATTRIBUTE_TYPE,manufacturer),
    OTA_ATTR(0x0008,ZCL_INT16U_ATTRIBUTE_TYPE,image_type),
};

void hal_ota_cluster_setup(hal_zigbee_cluster *cluster) {
    if (cluster == NULL) {
        return;
    }
    cluster->cluster_id      = ZCL_OTA_BOOTLOAD_CLUSTER_ID;
    cluster->is_server       = false;
    cluster->attribute_count = sizeof(attrs)/sizeof(attrs[0]);
    cluster->attributes      = attrs;
    cluster->cmd_callback    = NULL;
}

void hal_zigbee_set_image_type(uint16_t image_type) {
    // Silabs doesn't support dynamic image type setting
    // This can be probably hacked in, but it requires patching their SDK
    // internals so it's out of scope for now
}
