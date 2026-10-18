#!/usr/bin/env python3
"""Exercise the actual external SDK OTA query function with cached-image fixtures."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv) != 2:
    raise SystemExit("Usage: sdk_ota_size_check.py SDK_OTA_CLIENT_C_FILE")
source = Path(sys.argv[1]).read_text()
start = source.index("static sl_zigbee_af_status_t queryNextImageResponseParse(uint8_t* buffer,")
# Skip a forward declaration if the SDK contains one.
while source.index("{", start) > source.index(";", start):
    start = source.index("static sl_zigbee_af_status_t queryNextImageResponseParse(uint8_t* buffer,", start + 1)
end = source.index("\n}", start) + 2
function = source[start:end]
fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef int sl_zigbee_af_status_t;
typedef int sl_zigbee_af_ota_storage_status_t;
typedef struct {uint16_t manufacturerId, imageTypeId; uint32_t firmwareVersion;} sl_zigbee_af_ota_image_id_t;
enum {SL_ZIGBEE_ZCL_STATUS_SUCCESS=0, SL_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND=128,
SL_ZIGBEE_ZCL_STATUS_INVALID_FIELD=133, SL_ZIGBEE_ZCL_STATUS_INSUFFICIENT_SPACE=137,
SL_ZIGBEE_ZCL_STATUS_INVALID_IMAGE=150, SL_ZIGBEE_AF_OTA_STORAGE_SUCCESS=0,
SL_ZIGBEE_AF_IMAGE_UNKNOWN=0, ZCL_UPGRADE_END_REQUEST_COMMAND_ID=6,
SL_ZIGBEE_AF_RUN_UPGRADE_REQUEST_DELAY_MS=1, DELAY_BEFORE_SEND_QUERY=1};
#define QUERY_NEXT_IMAGE_SUCCESS_RESPONSE_MIN_LENGTH 13
#define otaPrintln(...) ((void)0)
#define sl_zigbee_af_get_int8u(...) 0
#define sl_zigbee_af_get_int32u(...) offeredSize
#define sl_zigbee_af_ota_storage_driver_max_download_size_cb() 0x56000u
#define updateImageTypeIdAttribute(...) ((void)0)
#define queryNextImage(...) ((void)0)
#define sendMessage(...) ((void)0)
static uint32_t totalImageSize, offeredSize, offeredVersion, cachedSize, cachedVersion, selectedSize;
static bool disableOtaDowngrades, downloaded, reused;
static int cachedStatus;
static sl_zigbee_af_ota_image_id_t currentDownloadFile;
static uint8_t sli_zigbee_af_ota_parse_image_id_from_message(sl_zigbee_af_ota_image_id_t *id,const uint8_t *p,uint8_t n) {
 (void)p;(void)n;*id=(sl_zigbee_af_ota_image_id_t){0xffff,0x4d43,offeredVersion};return 8;
}
static int sl_zigbee_af_ota_storage_check_temp_data_cb(uint32_t *offset,uint32_t *size,sl_zigbee_af_ota_image_id_t *id) {
 *offset=cachedSize;*size=cachedSize;*id=(sl_zigbee_af_ota_image_id_t){0xffff,0x4d43,cachedVersion};return cachedStatus;
}
static void startDownload(uint32_t version) {assert(version==offeredVersion);downloaded=true;selectedSize=totalImageSize;}
static void continueImageVerification(int status) {(void)status;reused=true;selectedSize=totalImageSize;}
'''
checks = r'''
static void check(uint32_t offered,uint32_t cached,int status,bool reuse) {
 uint8_t buffer[16]={0};offeredSize=offered;cachedSize=cached;offeredVersion=7;
 cachedVersion=reuse?8:6;cachedStatus=status;downloaded=reused=false;selectedSize=0;
 currentDownloadFile=(sl_zigbee_af_ota_image_id_t){0xffff,0x4d43,6};
 assert(queryNextImageResponseParse(buffer,3,sizeof(buffer))==SL_ZIGBEE_ZCL_STATUS_SUCCESS);
 assert(reused==reuse && downloaded!=reuse);
 if(selectedSize!=(reuse?cached:offered)) {
  fprintf(stderr,"Incorrect selected size: %u, expected %u\n",selectedSize,reuse?cached:offered);
  return_value=1;
 }
}
int main(void) {
 check(286298,286798,0,false); /* Shrink after a complete older download. */
 check(300000,286798,0,false); /* Grow: must not stop at the old length. */
 check(286298,280000,1,false); /* A partial cache must not replace the offer. */
 check(286298,300000,0,true);  /* Intentionally reuse a newer cached image. */
 if(!return_value)puts("SDK cached-image size regression: PASS (shrink/grow/partial/reuse)");
 return return_value;
}
'''
with tempfile.TemporaryDirectory(prefix="bridge-ota-size-") as temp:
    root = Path(temp)
    program = root / "check.c"
    program.write_text(fixture + function + "\nstatic int return_value;\n" + checks)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                    "-Wno-unused-parameter", "-Wno-unused-but-set-variable",
                    str(program), "-o", str(root / "check")], check=True)
    subprocess.run([str(root / "check")], check=True)
