#ifndef ECU_DIAGNOSTICS_H
#define ECU_DIAGNOSTICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void ECU_Diagnostics_Init(void);
void ECU_Diagnostics_ReportInit(bool app_ok, bool monitor_ok);
void ECU_Diagnostics_Process(uint32_t tick_ms);

#ifdef __cplusplus
}
#endif

#endif
