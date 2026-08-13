#ifndef PCU_APP_HAL_H
#define PCU_APP_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pcu_app.h"

#include <stdbool.h>
#include <stdint.h>

bool PCU_App_HAL_Init(void);
void PCU_App_HAL_Process(uint32_t tick_ms);
const PcuApp *PCU_App_HAL_GetState(void);

#ifdef __cplusplus
}
#endif

#endif
