#ifndef __DTM_BOARD_H__
#define __DTM_BOARD_H__

#include "dtm_resource.h"

int dtm_board_init(void);
void dtm_board_set_console(const pup_dtm_uart_config_t *config);
void dtm_board_set_clock(const pup_dtm_clock_config_t *config);
void dtm_board_set_key_exti(const pup_dtm_exti_config_t *config);
int dtm_board_get_console(pup_dtm_uart_config_t *out_config);
int dtm_board_get_clock(pup_dtm_clock_config_t *out_config);
int dtm_board_get_key_exti(pup_dtm_exti_config_t *out_config);

#endif
