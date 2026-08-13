#ifndef ECU_CONTROL_H
#define ECU_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

enum
{
  ECU_COMMAND_BYTE_RESTORE = 0x26U,
  ECU_COMMAND_BYTE_DISCONNECT = 0x86U,
  ECU_COMMAND_BYTE_EMERGENCY_STOP = 0x87U
};

typedef enum
{
  ECU_COMMAND_UNKNOWN = 0,
  ECU_COMMAND_DISCONNECT,
  ECU_COMMAND_EMERGENCY_STOP,
  ECU_COMMAND_RESTORE
} EcuCommand;

typedef enum
{
  ECU_CONTROL_SAFE_STARTUP = 0,
  ECU_CONTROL_RUNNING,
  ECU_CONTROL_EMERGENCY_STOPPED,
  ECU_CONTROL_DISCONNECTED
} EcuControlState;

typedef struct
{
  EcuControlState state;
  EcuCommand last_command;
  uint32_t accepted_command_count;
  uint32_t rejected_byte_count;
} EcuControl;

void ECU_Control_Init(EcuControl *control);
EcuCommand ECU_Command_Decode(uint8_t byte);
bool ECU_Control_ApplyCommand(EcuControl *control, EcuCommand command);
bool ECU_Control_HandleByte(EcuControl *control, uint8_t byte);
bool ECU_Control_ShouldEnergizeRelay(const EcuControl *control);

#ifdef __cplusplus
}
#endif

#endif
