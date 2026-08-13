#include "pcu_diagnostics.h"

#include "pcu_app_hal.h"
#include "usart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum
{
  PCU_DIAGNOSTICS_BUFFER_SIZE = 192U,
  PCU_DIAGNOSTICS_UART_TIMEOUT_MS = 20U,
  PCU_DIAGNOSTICS_BRIDGE_INTERVAL_MS = 100U,
  PCU_DIAGNOSTICS_ERROR_INTERVAL_MS = 1000U,
  PCU_DIAGNOSTICS_STATUS_INTERVAL_MS = 5000U
};

typedef struct
{
  uint32_t controller_to_lora_forwarded_count;
  uint32_t lora_to_controller_forwarded_count;
  uint32_t local_estop_transmitted_count;
  uint32_t reset_command_count;
  uint32_t rejected_reset_count;
  uint32_t estop_input_transition_count;
  uint32_t controller_receive_start_failure_count;
  uint32_t controller_receive_error_count;
  uint32_t unexpected_controller_callback_count;
  uint32_t lora_receive_start_failure_count;
  uint32_t lora_receive_error_count;
  uint32_t unexpected_lora_callback_count;
  uint32_t to_lora_queue_overflow_count;
  uint32_t to_controller_queue_overflow_count;
  uint32_t lora_transmit_failure_count;
  uint32_t controller_transmit_failure_count;
  uint32_t local_estop_transmit_failure_count;
} PcuDiagnosticsSnapshot;

static PcuDiagnosticsSnapshot snapshot;
static uint32_t last_bridge_report_tick;
static uint32_t last_error_report_tick;
static uint32_t last_status_report_tick;
static bool error_reported;

static const char *PCU_Diagnostics_BoolText(bool value)
{
  return value ? "YES" : "NO";
}

static void PCU_Diagnostics_Print(const char *format, ...)
{
  char buffer[PCU_DIAGNOSTICS_BUFFER_SIZE];
  va_list arguments;
  int result;
  size_t length;

  va_start(arguments, format);
  result = vsnprintf(buffer, sizeof(buffer) - 2U, format, arguments);
  va_end(arguments);
  if (result < 0)
  {
    return;
  }

  length = (size_t)result;
  if (length >= (sizeof(buffer) - 2U))
  {
    length = sizeof(buffer) - 3U;
  }
  buffer[length++] = '\r';
  buffer[length++] = '\n';
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)buffer, (uint16_t)length,
                          PCU_DIAGNOSTICS_UART_TIMEOUT_MS);
}

static bool PCU_Diagnostics_ErrorChanged(const PcuApp *app)
{
  return (app->controller_receive_start_failure_count !=
          snapshot.controller_receive_start_failure_count) ||
         (app->controller_receive_error_count !=
          snapshot.controller_receive_error_count) ||
         (app->unexpected_controller_callback_count !=
          snapshot.unexpected_controller_callback_count) ||
         (app->lora_receive_start_failure_count !=
          snapshot.lora_receive_start_failure_count) ||
         (app->lora_receive_error_count != snapshot.lora_receive_error_count) ||
         (app->unexpected_lora_callback_count !=
          snapshot.unexpected_lora_callback_count) ||
         (app->to_lora_queue_overflow_count !=
          snapshot.to_lora_queue_overflow_count) ||
         (app->to_controller_queue_overflow_count !=
          snapshot.to_controller_queue_overflow_count) ||
         (app->lora_transmit_failure_count !=
          snapshot.lora_transmit_failure_count) ||
         (app->controller_transmit_failure_count !=
          snapshot.controller_transmit_failure_count) ||
         (app->local_estop_transmit_failure_count !=
          snapshot.local_estop_transmit_failure_count);
}

static void PCU_Diagnostics_CaptureErrors(const PcuApp *app)
{
  snapshot.controller_receive_start_failure_count =
      app->controller_receive_start_failure_count;
  snapshot.controller_receive_error_count =
      app->controller_receive_error_count;
  snapshot.unexpected_controller_callback_count =
      app->unexpected_controller_callback_count;
  snapshot.lora_receive_start_failure_count =
      app->lora_receive_start_failure_count;
  snapshot.lora_receive_error_count = app->lora_receive_error_count;
  snapshot.unexpected_lora_callback_count =
      app->unexpected_lora_callback_count;
  snapshot.to_lora_queue_overflow_count =
      app->to_lora_queue_overflow_count;
  snapshot.to_controller_queue_overflow_count =
      app->to_controller_queue_overflow_count;
  snapshot.lora_transmit_failure_count = app->lora_transmit_failure_count;
  snapshot.controller_transmit_failure_count =
      app->controller_transmit_failure_count;
  snapshot.local_estop_transmit_failure_count =
      app->local_estop_transmit_failure_count;
}

void PCU_Diagnostics_Init(void)
{
  memset(&snapshot, 0, sizeof(snapshot));
  last_bridge_report_tick = 0U;
  last_error_report_tick = 0U;
  last_status_report_tick = HAL_GetTick();
  error_reported = false;
  PCU_Diagnostics_Print("[BOOT] PCU diagnostics USART1=%lu 8N1",
                        (unsigned long)huart1.Init.BaudRate);
}

void PCU_Diagnostics_ReportInit(bool app_ok)
{
  const PcuApp *app = PCU_App_HAL_GetState();

  PCU_Diagnostics_Print(
      "[INIT] app=%s USART2=%lu USART3=%lu mode=NORMAL MD0=0 MD1=0",
      app_ok ? "OK" : "DEGRADED",
      (unsigned long)huart2.Init.BaudRate,
      (unsigned long)huart3.Init.BaudRate);
  if (app != NULL)
  {
    PCU_Diagnostics_Print(
        "[ESTOP] input=%s latched=%s startup=SAFE reset_requires=USART2_0x26",
        app->estop.input_active ? "HIGH" : "LOW",
        PCU_Diagnostics_BoolText(PCU_Estop_IsLatched(&app->estop)));
    PCU_Diagnostics_CaptureErrors(app);
  }
}

void PCU_Diagnostics_Process(uint32_t tick_ms)
{
  const PcuApp *app = PCU_App_HAL_GetState();
  const bool bridge_due =
      (uint32_t)(tick_ms - last_bridge_report_tick) >=
      PCU_DIAGNOSTICS_BRIDGE_INTERVAL_MS;

  if (app == NULL)
  {
    return;
  }

  if (app->estop_input_transition_count !=
      snapshot.estop_input_transition_count)
  {
    PCU_Diagnostics_Print("[ESTOP] input=%s latched=%s",
                          app->estop.input_active ? "HIGH" : "LOW",
                          PCU_Diagnostics_BoolText(
                              PCU_Estop_IsLatched(&app->estop)));
    snapshot.estop_input_transition_count = app->estop_input_transition_count;
  }

  if (app->local_estop_transmitted_count !=
      snapshot.local_estop_transmitted_count)
  {
    PCU_Diagnostics_Print(
        "[ESTOP] tx=0x87 route=LORA count=%lu (+%lu)",
        (unsigned long)app->local_estop_transmitted_count,
        (unsigned long)(app->local_estop_transmitted_count -
                        snapshot.local_estop_transmitted_count));
    snapshot.local_estop_transmitted_count =
        app->local_estop_transmitted_count;
  }

  if (app->estop.reset_command_count != snapshot.reset_command_count)
  {
    PCU_Diagnostics_Print("[RESET] rx=0x26 accepted=YES route=LORA count=%lu",
                          (unsigned long)app->estop.reset_command_count);
    snapshot.reset_command_count = app->estop.reset_command_count;
  }

  if (app->estop.rejected_reset_count != snapshot.rejected_reset_count)
  {
    PCU_Diagnostics_Print(
        "[RESET] rx=0x26 accepted=NO reason=ESTOP_INPUT_HIGH count=%lu",
        (unsigned long)app->estop.rejected_reset_count);
    snapshot.rejected_reset_count = app->estop.rejected_reset_count;
  }

  if (bridge_due &&
      (app->controller_to_lora_forwarded_count !=
       snapshot.controller_to_lora_forwarded_count))
  {
    PCU_Diagnostics_Print(
        "[USART2] latest=0x%02X route=LORA forwarded=%lu (+%lu)",
        app->controller_rx_byte,
        (unsigned long)app->controller_to_lora_forwarded_count,
        (unsigned long)(app->controller_to_lora_forwarded_count -
                        snapshot.controller_to_lora_forwarded_count));
    snapshot.controller_to_lora_forwarded_count =
        app->controller_to_lora_forwarded_count;
    last_bridge_report_tick = tick_ms;
  }

  if (bridge_due &&
      (app->lora_to_controller_forwarded_count !=
       snapshot.lora_to_controller_forwarded_count))
  {
    PCU_Diagnostics_Print(
        "[LORA] latest=0x%02X route=USART2 forwarded=%lu (+%lu)",
        app->lora_rx_byte,
        (unsigned long)app->lora_to_controller_forwarded_count,
        (unsigned long)(app->lora_to_controller_forwarded_count -
                        snapshot.lora_to_controller_forwarded_count));
    snapshot.lora_to_controller_forwarded_count =
        app->lora_to_controller_forwarded_count;
    last_bridge_report_tick = tick_ms;
  }

  if (PCU_Diagnostics_ErrorChanged(app) &&
      (!error_reported ||
       ((uint32_t)(tick_ms - last_error_report_tick) >=
        PCU_DIAGNOSTICS_ERROR_INTERVAL_MS)))
  {
    PCU_Diagnostics_Print(
        "[ERROR] usart2_start=%lu rx=%lu cb=%lu usart3_start=%lu rx=%lu cb=%lu q_lora=%lu q_usart2=%lu tx_lora=%lu tx_usart2=%lu estop_tx=%lu",
        (unsigned long)app->controller_receive_start_failure_count,
        (unsigned long)app->controller_receive_error_count,
        (unsigned long)app->unexpected_controller_callback_count,
        (unsigned long)app->lora_receive_start_failure_count,
        (unsigned long)app->lora_receive_error_count,
        (unsigned long)app->unexpected_lora_callback_count,
        (unsigned long)app->to_lora_queue_overflow_count,
        (unsigned long)app->to_controller_queue_overflow_count,
        (unsigned long)app->lora_transmit_failure_count,
        (unsigned long)app->controller_transmit_failure_count,
        (unsigned long)app->local_estop_transmit_failure_count);
    PCU_Diagnostics_CaptureErrors(app);
    error_reported = true;
    last_error_report_tick = tick_ms;
  }

  if ((uint32_t)(tick_ms - last_status_report_tick) >=
      PCU_DIAGNOSTICS_STATUS_INTERVAL_MS)
  {
    last_status_report_tick = tick_ms;
    PCU_Diagnostics_Print(
        "[STATUS] estop_input=%s latched=%s usart2_to_lora=%lu lora_to_usart2=%lu estop_tx=%lu",
        app->estop.input_active ? "HIGH" : "LOW",
        PCU_Diagnostics_BoolText(PCU_Estop_IsLatched(&app->estop)),
        (unsigned long)app->controller_to_lora_forwarded_count,
        (unsigned long)app->lora_to_controller_forwarded_count,
        (unsigned long)app->local_estop_transmitted_count);
  }
}
