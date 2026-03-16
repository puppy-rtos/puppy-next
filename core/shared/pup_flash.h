#ifndef __PUP_FLASH_H__
#define __PUP_FLASH_H__

#include <stdint.h>

#define PUP_FLASH_NAME_MAX            16

#define PUP_FLASH_IOCTL_GET_INFO      0x3101
#define PUP_FLASH_IOCTL_READ          0x3102
#define PUP_FLASH_IOCTL_WRITE         0x3103
#define PUP_FLASH_IOCTL_ERASE         0x3104
#define PUP_FLASH_IOCTL_ERASE_WRITE   0x3105
#define PUP_FLASH_IOCTL_READ_STATUS   0x3106

typedef struct {
    char name[PUP_FLASH_NAME_MAX];
    char spi_name[PUP_FLASH_NAME_MAX];
    uint32_t capacity;
    uint32_t erase_gran;
    uint8_t erase_cmd;
    uint8_t init_ok;
    uint8_t sfdp_available;
    uint8_t reserved;
    uint8_t mf_id;
    uint8_t type_id;
    uint8_t capacity_id;
} pup_flash_info_t;

typedef struct {
    uint32_t addr;
    uint8_t *buf;
    uint32_t size;
} pup_flash_read_req_t;

typedef struct {
    uint32_t addr;
    const uint8_t *buf;
    uint32_t size;
} pup_flash_write_req_t;

typedef struct {
    uint32_t addr;
    uint32_t size;
} pup_flash_erase_req_t;

typedef struct {
    uint8_t value;
} pup_flash_status_t;

#endif