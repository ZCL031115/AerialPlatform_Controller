#ifndef ECU_APP_H
#define ECU_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecu_control.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  ECU_LORA_MODE_NORMAL = 0,
  ECU_LORA_MODE_CONFIG
} EcuLoraMode;

typedef bool (*EcuAppStartReceiveFn)(void *context, uint8_t *destination);
typedef bool (*EcuAppTransmitByteFn)(void *context, uint8_t value);
typedef void (*EcuAppWriteRelayFn)(void *context, bool energized);
typedef void (*EcuAppWriteLoraModeFn)(void *context, bool md0, bool md1);

enum
{
  ECU_APP_FORWARD_QUEUE_CAPACITY = 16U
};

typedef struct
{
  EcuAppStartReceiveFn start_receive;
  EcuAppStartReceiveFn start_external_receive;
  EcuAppTransmitByteFn transmit_lora;
  EcuAppTransmitByteFn transmit_external;
  EcuAppWriteRelayFn write_relay;
  EcuAppWriteLoraModeFn write_lora_mode;
  void *context;
} EcuAppIo;

typedef struct
{
  EcuControl control;
  EcuAppIo io;
  EcuLoraMode lora_mode;
  uint8_t rx_byte;
  uint8_t external_rx_byte;
  uint8_t to_lora_queue[ECU_APP_FORWARD_QUEUE_CAPACITY];
  uint8_t to_external_queue[ECU_APP_FORWARD_QUEUE_CAPACITY];
  volatile uint8_t to_lora_head;
  volatile uint8_t to_lora_tail;
  volatile uint8_t to_external_head;
  volatile uint8_t to_external_tail;
  bool receive_pending;
  bool external_receive_pending;
  uint32_t receive_start_failure_count;
  uint32_t receive_error_count;
  uint32_t unexpected_receive_callback_count;
  uint32_t external_receive_start_failure_count;
  uint32_t external_receive_error_count;
  uint32_t unexpected_external_receive_callback_count;
  uint32_t lora_forwarded_byte_count;
  uint32_t external_forwarded_byte_count;
  uint32_t lora_queue_overflow_count;
  uint32_t external_queue_overflow_count;
  uint32_t lora_transmit_failure_count;
  uint32_t external_transmit_failure_count;
} EcuApp;

bool ECU_App_Init(EcuApp *app, const EcuAppIo *io);
bool ECU_App_StartReceive(EcuApp *app);
bool ECU_App_StartExternalReceive(EcuApp *app);
bool ECU_App_OnReceiveComplete(EcuApp *app);
bool ECU_App_OnExternalReceiveComplete(EcuApp *app);
bool ECU_App_OnReceiveError(EcuApp *app);
bool ECU_App_OnExternalReceiveError(EcuApp *app);
void ECU_App_Process(EcuApp *app);
bool ECU_App_SetLoraMode(EcuApp *app, EcuLoraMode mode);

#ifdef __cplusplus
}
#endif

#endif
