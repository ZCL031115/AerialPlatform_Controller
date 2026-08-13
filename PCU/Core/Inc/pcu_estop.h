#ifndef PCU_ESTOP_H
#define PCU_ESTOP_H

#include <stdbool.h>
#include <stdint.h>

#define PCU_COMMAND_RESTORE 0x26U
#define PCU_COMMAND_ESTOP 0x87U
#define PCU_ESTOP_REPEAT_INTERVAL_MS 100U

typedef enum
{
  PCU_ESTOP_READY = 0,
  PCU_ESTOP_LATCHED
} PcuEstopState;

typedef struct
{
  PcuEstopState state;
  bool input_active;
  bool estop_command_sent;
  uint32_t last_estop_command_tick;
  uint32_t estop_command_count;
  uint32_t reset_command_count;
  uint32_t rejected_reset_count;
} PcuEstop;

void PCU_Estop_Init(PcuEstop *estop, bool input_active);
bool PCU_Estop_UpdateInput(PcuEstop *estop, bool input_active);
bool PCU_Estop_Process(PcuEstop *estop, uint32_t tick_ms, uint8_t *command);
bool PCU_Estop_RequestReset(PcuEstop *estop, uint8_t *command);
bool PCU_Estop_IsLatched(const PcuEstop *estop);

#endif
