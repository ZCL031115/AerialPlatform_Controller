#include "pcu_estop.h"

#include <stddef.h>

void PCU_Estop_Init(PcuEstop *estop, bool input_active)
{
  if (estop == NULL)
  {
    return;
  }

  estop->state = PCU_ESTOP_LATCHED;
  estop->input_active = input_active;
  estop->estop_command_sent = false;
  estop->last_estop_command_tick = 0U;
  estop->estop_command_count = 0U;
  estop->reset_command_count = 0U;
  estop->rejected_reset_count = 0U;
}

bool PCU_Estop_UpdateInput(PcuEstop *estop, bool input_active)
{
  bool newly_active;

  if (estop == NULL)
  {
    return false;
  }

  newly_active = input_active && !estop->input_active;
  estop->input_active = input_active;
  if (input_active)
  {
    estop->state = PCU_ESTOP_LATCHED;
    if (newly_active)
    {
      estop->estop_command_sent = false;
    }
  }

  return newly_active;
}

bool PCU_Estop_Process(PcuEstop *estop, uint32_t tick_ms, uint8_t *command)
{
  if ((estop == NULL) || (command == NULL) ||
      (estop->state != PCU_ESTOP_LATCHED))
  {
    return false;
  }

  if (estop->estop_command_sent &&
      ((uint32_t)(tick_ms - estop->last_estop_command_tick) <
       PCU_ESTOP_REPEAT_INTERVAL_MS))
  {
    return false;
  }

  *command = PCU_COMMAND_ESTOP;
  estop->estop_command_sent = true;
  estop->last_estop_command_tick = tick_ms;
  estop->estop_command_count++;
  return true;
}

bool PCU_Estop_RequestReset(PcuEstop *estop, uint8_t *command)
{
  if ((estop == NULL) || (command == NULL))
  {
    return false;
  }

  if (estop->input_active)
  {
    estop->rejected_reset_count++;
    return false;
  }

  estop->state = PCU_ESTOP_READY;
  estop->estop_command_sent = false;
  *command = PCU_COMMAND_RESTORE;
  estop->reset_command_count++;
  return true;
}

bool PCU_Estop_IsLatched(const PcuEstop *estop)
{
  return (estop != NULL) && (estop->state == PCU_ESTOP_LATCHED);
}
