#ifndef PCU_DIAGNOSTICS_H
#define PCU_DIAGNOSTICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void PCU_Diagnostics_Init(void);
void PCU_Diagnostics_ReportInit(bool app_ok);
void PCU_Diagnostics_Process(uint32_t tick_ms);

#ifdef __cplusplus
}
#endif

#endif
