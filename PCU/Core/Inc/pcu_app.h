#ifndef PCU_APP_H
#define PCU_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pcu_estop.h"

#include <stdbool.h>
#include <stdint.h>

enum
{
  PCU_APP_FORWARD_QUEUE_CAPACITY = 32U
};

typedef bool (*PcuAppStartReceiveFn)(void *context, uint8_t *destination);
typedef bool (*PcuAppTransmitByteFn)(void *context, uint8_t value);

typedef struct
{
  PcuAppStartReceiveFn start_controller_receive;
  PcuAppStartReceiveFn start_lora_receive;
  PcuAppTransmitByteFn transmit_controller;
  PcuAppTransmitByteFn transmit_lora;
  void *context;
} PcuAppIo;

typedef struct
{
  PcuEstop estop;
  PcuAppIo io;
  uint8_t controller_rx_byte;
  uint8_t lora_rx_byte;
  uint8_t to_lora_queue[PCU_APP_FORWARD_QUEUE_CAPACITY];
  uint8_t to_controller_queue[PCU_APP_FORWARD_QUEUE_CAPACITY];
  volatile uint8_t to_lora_head;
  volatile uint8_t to_lora_tail;
  volatile uint8_t to_controller_head;
  volatile uint8_t to_controller_tail;
  bool controller_receive_pending;
  bool lora_receive_pending;
  uint32_t controller_receive_start_failure_count;
  uint32_t controller_receive_error_count;
  uint32_t unexpected_controller_callback_count;
  uint32_t lora_receive_start_failure_count;
  uint32_t lora_receive_error_count;
  uint32_t unexpected_lora_callback_count;
  uint32_t controller_to_lora_forwarded_count;
  uint32_t lora_to_controller_forwarded_count;
  uint32_t to_lora_queue_overflow_count;
  uint32_t to_controller_queue_overflow_count;
  uint32_t lora_transmit_failure_count;
  uint32_t controller_transmit_failure_count;
  uint32_t local_estop_transmitted_count;
  uint32_t local_estop_transmit_failure_count;
  uint32_t estop_input_transition_count;
} PcuApp;

bool PCU_App_Init(PcuApp *app, const PcuAppIo *io, bool estop_input_active);
bool PCU_App_StartControllerReceive(PcuApp *app);
bool PCU_App_StartLoraReceive(PcuApp *app);
bool PCU_App_OnControllerReceiveComplete(PcuApp *app);
bool PCU_App_OnLoraReceiveComplete(PcuApp *app);
bool PCU_App_OnControllerReceiveError(PcuApp *app);
bool PCU_App_OnLoraReceiveError(PcuApp *app);
bool PCU_App_UpdateEstopInput(PcuApp *app, bool input_active);
void PCU_App_Process(PcuApp *app, uint32_t tick_ms);

#ifdef __cplusplus
}
#endif

#endif
