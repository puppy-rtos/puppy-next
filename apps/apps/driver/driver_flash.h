#ifndef DRIVER_FLASH_H
#define DRIVER_FLASH_H

#include "shared/pup_devfs_proto.h"
#include "shared/pup_flash.h"

#define FLASH_DEVICE_ID  4

int flash_handle_request(pup_devfs_proxy_payload_t *req);
int flash_register_one(void);
int flash_init(void);
int flash_selftest(void);

#endif
