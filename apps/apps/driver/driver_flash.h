#ifndef DRIVER_FLASH_H
#define DRIVER_FLASH_H

#include "shared/pup_devfs_proto.h"
#include "shared/pup_flash.h"
#include "driver_of.h"

extern const dtm_of_driver_t g_flash_of_driver;

void flash_resources_init(void);
const pup_dtm_flash_device_t *flash_device(void);
int flash_handle_request(pup_devfs_proxy_payload_t *req);
int flash_register_one(void);
int flash_init(void);
int flash_selftest(void);

#endif
