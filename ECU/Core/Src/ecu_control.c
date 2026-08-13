#include "ecu_control.h"

#include <stddef.h>

void ECU_Control_Init(EcuControl *control)
{
  if (control == NULL)
  {
    return;
  }

  control->state = ECU_CONTROL_SAFE_STARTUP;
  control->last_command = ECU_COMMAND_UNKNOWN;
  control->accepted_command_count = 0U;
  control->rejected_byte_count = 0U;
}

EcuCommand ECU_Command_Decode(uint8_t byte)
{
  switch (byte)
  {
    case ECU_COMMAND_BYTE_DISCONNECT:
      return ECU_COMMAND_DISCONNECT;

    case ECU_COMMAND_BYTE_EMERGENCY_STOP:
      return ECU_COMMAND_EMERGENCY_STOP;

    case ECU_COMMAND_BYTE_RESTORE:
      return ECU_COMMAND_RESTORE;

    default:
      return ECU_COMMAND_UNKNOWN;
  }
}

bool ECU_Control_ApplyCommand(EcuControl *control, EcuCommand command)
{
  if (control == NULL)
  {
    return false;
  }

  switch (command)
  {
    case ECU_COMMAND_DISCONNECT:
      control->state = ECU_CONTROL_DISCONNECTED;
      break;

    case ECU_COMMAND_EMERGENCY_STOP:
      control->state = ECU_CONTROL_EMERGENCY_STOPPED;
      break;

    case ECU_COMMAND_RESTORE:
      control->state = ECU_CONTROL_RUNNING;
      break;

    case ECU_COMMAND_UNKNOWN:
    default:
      return false;
  }

  control->last_command = command;
  control->accepted_command_count++;
  return true;
}

bool ECU_Control_HandleByte(EcuControl *control, uint8_t byte)
{
  EcuCommand command;

  if (control == NULL)
  {
    return false;
  }

  command = ECU_Command_Decode(byte);
  if (command == ECU_COMMAND_UNKNOWN)
  {
    control->rejected_byte_count++;
    return false;
  }

  return ECU_Control_ApplyCommand(control, command);
}

bool ECU_Control_ShouldEnergizeRelay(const EcuControl *control)
{
  return (control != NULL) && (control->state == ECU_CONTROL_RUNNING);
}
