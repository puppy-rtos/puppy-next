#ifndef _SFUD_CFG_H_
#define _SFUD_CFG_H_

#define SFUD_DEBUG_MODE
#define SFUD_USING_SFDP
#define SFUD_USING_FLASH_INFO_TABLE

enum {
    SFUD_FLASH0_DEVICE_INDEX = 0,
};

#define SFUD_FLASH_DEVICE_TABLE                                        \
{                                                                      \
    [SFUD_FLASH0_DEVICE_INDEX] = {                                     \
        .name = "flash0",                                             \
        .spi.name = "spi2",                                           \
    },                                                                 \
}

#endif
