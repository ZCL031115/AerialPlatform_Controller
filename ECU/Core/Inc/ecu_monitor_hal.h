#ifndef ECU_MONITOR_HAL_H
#define ECU_MONITOR_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecu_control.h"
#include "ecu_monitor.h"

#include <stdbool.h>
#include <stdint.h>

bool ECU_Monitor_HAL_Init(void);
void ECU_Monitor_HAL_Process(uint32_t tick_ms,
                             EcuControlState control_state);
const EcuMonitor *ECU_Monitor_HAL_GetState(void);
bool ECU_Monitor_HAL_IsRc522Available(void);
bool ECU_Monitor_HAL_IsBmp280Available(void);

#ifdef __cplusplus
}
#endif

#endif
