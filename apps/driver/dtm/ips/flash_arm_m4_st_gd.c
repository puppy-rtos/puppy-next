
#include <stdint.h>
#include "dtm_bindings.h"

#ifdef DTM_BINFDING_FLASH_ARM_M4_ST_GD

typedef struct
{
  volatile uint32_t ACR;      /*!< FLASH access control register,   Address offset: 0x00 */
  volatile uint32_t KEYR;     /*!< FLASH key register,              Address offset: 0x04 */
  volatile uint32_t OPTKEYR;  /*!< FLASH option key register,       Address offset: 0x08 */
  volatile uint32_t SR;       /*!< FLASH status register,           Address offset: 0x0C */
  volatile uint32_t CR;       /*!< FLASH control register,          Address offset: 0x10 */
  volatile uint32_t OPTCR;    /*!< FLASH option control register ,  Address offset: 0x14 */
  volatile uint32_t OPTCR1;   /*!< FLASH option control register 1, Address offset: 0x18 */
} DTM_FLASH_Type;

#ifndef DTM_FLASH_BASE
#define DTM_FLASH_BASE 0x40023C00UL
#endif

#define FLASH ((DTM_FLASH_Type *)DTM_FLASH_BASE)

/**
 * @brief       Initialize the flash controller
 * @retval      None
 */
void dtm_flash_init(void)
{
    // Enable the flash access control register
    FLASH->ACR |= 1 << 8;                   /* Instruction prefetch enable */
    FLASH->ACR |= 1 << 9;                   /* Instruction cache enable */
    FLASH->ACR |= 1 << 10;                  /* Data cache enable */
    FLASH->ACR |= 5 << 0;                   /* 5 CPU wait cycles */
}

#endif
