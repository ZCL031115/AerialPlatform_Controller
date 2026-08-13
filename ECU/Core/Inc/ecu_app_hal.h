#ifndef ECU_APP_HAL_H
#define ECU_APP_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecu_app.h"

#include <stdbool.h>

bool ECU_App_HAL_Init(void);
void ECU_App_HAL_Process(void);
bool ECU_App_HAL_SetLoraMode(EcuLoraMode mode);
bool ECU_App_HAL_IsExternalUartAvailable(void);
const EcuApp *ECU_App_HAL_GetState(void);

#ifdef __cplusplus
}
#endif

#endif
