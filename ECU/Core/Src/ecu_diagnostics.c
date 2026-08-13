#include "ecu_diagnostics.h"

#include "bmp280_hal.h"
#include "ecu_app_hal.h"
#include "ecu_monitor_hal.h"
#include "rc522_hal.h"
#include "usart.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum
{
  ECU_DIAGNOSTICS_BUFFER_SIZE = 192U,
  ECU_DIAGNOSTICS_UART_TIMEOUT_MS = 20U,
  ECU_DIAGNOSTICS_BRIDGE_INTERVAL_MS = 100U,
  ECU_DIAGNOSTICS_ERROR_INTERVAL_MS = 1000U,
  ECU_DIAGNOSTICS_STATUS_INTERVAL_MS = 5000U
};

typedef struct
{
  uint32_t accepted_command_count;
  uint32_t rejected_byte_count;
  uint32_t lora_forwarded_byte_count;
  uint32_t external_forwarded_byte_count;
  uint32_t receive_start_failure_count;
  uint32_t receive_error_count;
  uint32_t unexpected_receive_callback_count;
  uint32_t external_receive_start_failure_count;
  uint32_t external_receive_error_count;
  uint32_t unexpected_external_receive_callback_count;
  uint32_t lora_queue_overflow_count;
  uint32_t external_queue_overflow_count;
  uint32_t lora_transmit_failure_count;
  uint32_t external_transmit_failure_count;
  uint32_t authorized_card_count;
  uint32_t rejected_card_count;
  uint32_t card_error_count;
  uint32_t measurement_error_count;
  uint32_t height_estimate_count;
  EcuLoraMode lora_mode;
  bool external_uart_available;
} EcuDiagnosticsSnapshot;

static EcuDiagnosticsSnapshot diagnostics_snapshot;
static uint32_t last_bridge_report_tick;
static uint32_t last_error_report_tick;
static uint32_t last_status_report_tick;
static bool bridge_reported;
static bool error_reported;

static const char *ECU_Diagnostics_BoolText(bool value)
{
  return value ? "YES" : "NO";
}

static const char *ECU_Diagnostics_OkText(bool value)
{
  return value ? "OK" : "FAIL";
}

static const char *ECU_Diagnostics_RelayText(const EcuControl *control)
{
  return ECU_Control_ShouldEnergizeRelay(control) ? "ON" : "OFF";
}

static const char *ECU_Diagnostics_CommandName(EcuCommand command)
{
  switch (command)
  {
    case ECU_COMMAND_DISCONNECT:
      return "DISCONNECT";
    case ECU_COMMAND_EMERGENCY_STOP:
      return "ESTOP";
    case ECU_COMMAND_RESTORE:
      return "RESTORE";
    case ECU_COMMAND_UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

static uint8_t ECU_Diagnostics_CommandByte(EcuCommand command)
{
  switch (command)
  {
    case ECU_COMMAND_DISCONNECT:
      return ECU_COMMAND_BYTE_DISCONNECT;
    case ECU_COMMAND_EMERGENCY_STOP:
      return ECU_COMMAND_BYTE_EMERGENCY_STOP;
    case ECU_COMMAND_RESTORE:
      return ECU_COMMAND_BYTE_RESTORE;
    case ECU_COMMAND_UNKNOWN:
    default:
      return 0U;
  }
}

static const char *ECU_Diagnostics_StateName(EcuControlState state)
{
  switch (state)
  {
    case ECU_CONTROL_SAFE_STARTUP:
      return "SAFE_STARTUP";
    case ECU_CONTROL_RUNNING:
      return "RUNNING";
    case ECU_CONTROL_EMERGENCY_STOPPED:
      return "ESTOP";
    case ECU_CONTROL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

static const char *ECU_Diagnostics_LoraModeName(EcuLoraMode mode)
{
  return (mode == ECU_LORA_MODE_CONFIG) ? "CONFIG" : "NORMAL";
}

static const char *ECU_Diagnostics_MotionName(HeightMotion motion)
{
  switch (motion)
  {
    case HEIGHT_MOTION_ASCENDING:
      return "UP";
    case HEIGHT_MOTION_DESCENDING:
      return "DOWN";
    case HEIGHT_MOTION_STATIONARY:
    default:
      return "STATIONARY";
  }
}

static const char *ECU_Diagnostics_Rc522StatusName(Rc522Status status)
{
  switch (status)
  {
    case RC522_STATUS_OK:
      return "OK";
    case RC522_STATUS_NO_CARD:
      return "NO_CARD";
    case RC522_STATUS_TIMEOUT:
      return "TIMEOUT";
    case RC522_STATUS_COMMUNICATION_ERROR:
      return "COMM_ERROR";
    case RC522_STATUS_PROTOCOL_ERROR:
      return "PROTOCOL_ERROR";
    case RC522_STATUS_BUFFER_TOO_SMALL:
      return "BUFFER_SMALL";
    case RC522_STATUS_INVALID_ARGUMENT:
    default:
      return "INVALID";
  }
}

static const char *ECU_Diagnostics_Bmp280StatusName(Bmp280Status status)
{
  switch (status)
  {
    case BMP280_STATUS_OK:
      return "OK";
    case BMP280_STATUS_IO_ERROR:
      return "IO_ERROR";
    case BMP280_STATUS_BAD_CHIP_ID:
      return "BAD_CHIP_ID";
    case BMP280_STATUS_TIMEOUT:
      return "TIMEOUT";
    case BMP280_STATUS_BAD_CALIBRATION:
      return "BAD_CALIBRATION";
    case BMP280_STATUS_INVALID_ARGUMENT:
    default:
      return "INVALID";
  }
}

static int32_t ECU_Diagnostics_RoundToInt32(double value)
{
  if (value >= (double)INT32_MAX)
  {
    return INT32_MAX;
  }
  if (value <= (double)INT32_MIN)
  {
    return INT32_MIN;
  }
  return (value >= 0.0) ? (int32_t)(value + 0.5)
                        : (int32_t)(value - 0.5);
}

static void ECU_Diagnostics_Print(const char *format, ...)
{
  char buffer[ECU_DIAGNOSTICS_BUFFER_SIZE];
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
                          ECU_DIAGNOSTICS_UART_TIMEOUT_MS);
}

static void ECU_Diagnostics_CaptureApp(const EcuApp *app)
{
  if (app == NULL)
  {
    return;
  }

  diagnostics_snapshot.accepted_command_count =
      app->control.accepted_command_count;
  diagnostics_snapshot.rejected_byte_count = app->control.rejected_byte_count;
  diagnostics_snapshot.lora_forwarded_byte_count =
      app->lora_forwarded_byte_count;
  diagnostics_snapshot.external_forwarded_byte_count =
      app->external_forwarded_byte_count;
  diagnostics_snapshot.receive_start_failure_count =
      app->receive_start_failure_count;
  diagnostics_snapshot.receive_error_count = app->receive_error_count;
  diagnostics_snapshot.unexpected_receive_callback_count =
      app->unexpected_receive_callback_count;
  diagnostics_snapshot.external_receive_start_failure_count =
      app->external_receive_start_failure_count;
  diagnostics_snapshot.external_receive_error_count =
      app->external_receive_error_count;
  diagnostics_snapshot.unexpected_external_receive_callback_count =
      app->unexpected_external_receive_callback_count;
  diagnostics_snapshot.lora_queue_overflow_count =
      app->lora_queue_overflow_count;
  diagnostics_snapshot.external_queue_overflow_count =
      app->external_queue_overflow_count;
  diagnostics_snapshot.lora_transmit_failure_count =
      app->lora_transmit_failure_count;
  diagnostics_snapshot.external_transmit_failure_count =
      app->external_transmit_failure_count;
  diagnostics_snapshot.lora_mode = app->lora_mode;
  diagnostics_snapshot.external_uart_available =
      ECU_App_HAL_IsExternalUartAvailable();
}

static void ECU_Diagnostics_CaptureMonitor(const EcuMonitor *monitor)
{
  if (monitor == NULL)
  {
    return;
  }

  diagnostics_snapshot.authorized_card_count = monitor->authorized_card_count;
  diagnostics_snapshot.rejected_card_count = monitor->rejected_card_count;
  diagnostics_snapshot.card_error_count = monitor->card_error_count;
  diagnostics_snapshot.measurement_error_count =
      monitor->measurement_error_count;
  diagnostics_snapshot.height_estimate_count = monitor->height_estimate_count;
}

static bool ECU_Diagnostics_AppErrorChanged(const EcuApp *app)
{
  return (app->receive_start_failure_count !=
          diagnostics_snapshot.receive_start_failure_count) ||
         (app->receive_error_count !=
          diagnostics_snapshot.receive_error_count) ||
         (app->unexpected_receive_callback_count !=
          diagnostics_snapshot.unexpected_receive_callback_count) ||
         (app->external_receive_start_failure_count !=
          diagnostics_snapshot.external_receive_start_failure_count) ||
         (app->external_receive_error_count !=
          diagnostics_snapshot.external_receive_error_count) ||
         (app->unexpected_external_receive_callback_count !=
          diagnostics_snapshot.unexpected_external_receive_callback_count) ||
         (app->lora_queue_overflow_count !=
          diagnostics_snapshot.lora_queue_overflow_count) ||
         (app->external_queue_overflow_count !=
          diagnostics_snapshot.external_queue_overflow_count) ||
         (app->lora_transmit_failure_count !=
          diagnostics_snapshot.lora_transmit_failure_count) ||
         (app->external_transmit_failure_count !=
          diagnostics_snapshot.external_transmit_failure_count);
}

static void ECU_Diagnostics_ReportApp(uint32_t tick_ms, const EcuApp *app)
{
  const bool external_uart_available =
      ECU_App_HAL_IsExternalUartAvailable();
  const bool bridge_due =
      !bridge_reported ||
      ((uint32_t)(tick_ms - last_bridge_report_tick) >=
       ECU_DIAGNOSTICS_BRIDGE_INTERVAL_MS);
  const bool error_due =
      !error_reported ||
      ((uint32_t)(tick_ms - last_error_report_tick) >=
       ECU_DIAGNOSTICS_ERROR_INTERVAL_MS);

  if (app->control.accepted_command_count !=
      diagnostics_snapshot.accepted_command_count)
  {
    ECU_Diagnostics_Print(
        "[CONTROL] rx=0x%02X command=%s state=%s relay=%s count=%lu (+%lu)",
        ECU_Diagnostics_CommandByte(app->control.last_command),
        ECU_Diagnostics_CommandName(app->control.last_command),
        ECU_Diagnostics_StateName(app->control.state),
        ECU_Diagnostics_RelayText(&app->control),
        (unsigned long)app->control.accepted_command_count,
        (unsigned long)(app->control.accepted_command_count -
                        diagnostics_snapshot.accepted_command_count));
    diagnostics_snapshot.accepted_command_count =
        app->control.accepted_command_count;
  }

  if ((app->lora_mode != diagnostics_snapshot.lora_mode))
  {
    ECU_Diagnostics_Print("[LORA] mode=%s MD0=%u MD1=%u",
                          ECU_Diagnostics_LoraModeName(app->lora_mode),
                          app->lora_mode == ECU_LORA_MODE_CONFIG ? 1U : 0U,
                          app->lora_mode == ECU_LORA_MODE_CONFIG ? 1U : 0U);
    diagnostics_snapshot.lora_mode = app->lora_mode;
  }

  if (external_uart_available !=
      diagnostics_snapshot.external_uart_available)
  {
    ECU_Diagnostics_Print("[USART2] state=%s",
                          external_uart_available ? "ONLINE" : "OFFLINE");
    diagnostics_snapshot.external_uart_available = external_uart_available;
  }

  if (bridge_due &&
      (app->control.rejected_byte_count !=
       diagnostics_snapshot.rejected_byte_count))
  {
    ECU_Diagnostics_Print(
        "[LORA] latest=0x%02X route=USART2 received=%lu (+%lu)",
        app->rx_byte, (unsigned long)app->control.rejected_byte_count,
        (unsigned long)(app->control.rejected_byte_count -
                        diagnostics_snapshot.rejected_byte_count));
    diagnostics_snapshot.rejected_byte_count =
        app->control.rejected_byte_count;
    bridge_reported = true;
    last_bridge_report_tick = tick_ms;
  }

  if (bridge_due &&
      (app->lora_forwarded_byte_count !=
       diagnostics_snapshot.lora_forwarded_byte_count))
  {
    ECU_Diagnostics_Print(
        "[USART2] latest=0x%02X route=LORA forwarded=%lu (+%lu)",
        app->external_rx_byte, (unsigned long)app->lora_forwarded_byte_count,
        (unsigned long)(app->lora_forwarded_byte_count -
                        diagnostics_snapshot.lora_forwarded_byte_count));
    diagnostics_snapshot.lora_forwarded_byte_count =
        app->lora_forwarded_byte_count;
    bridge_reported = true;
    last_bridge_report_tick = tick_ms;
  }

  if (bridge_due &&
      (app->external_forwarded_byte_count !=
       diagnostics_snapshot.external_forwarded_byte_count))
  {
    ECU_Diagnostics_Print(
        "[BRIDGE] LORA->USART2 forwarded=%lu (+%lu)",
        (unsigned long)app->external_forwarded_byte_count,
        (unsigned long)(app->external_forwarded_byte_count -
                        diagnostics_snapshot.external_forwarded_byte_count));
    diagnostics_snapshot.external_forwarded_byte_count =
        app->external_forwarded_byte_count;
    bridge_reported = true;
    last_bridge_report_tick = tick_ms;
  }

  if (error_due && ECU_Diagnostics_AppErrorChanged(app))
  {
    ECU_Diagnostics_Print(
        "[ERROR] lora_rx_start=%lu rx=%lu cb=%lu usart2_rx_start=%lu rx=%lu cb=%lu queue_lora=%lu queue_usart2=%lu tx_lora=%lu tx_usart2=%lu",
        (unsigned long)app->receive_start_failure_count,
        (unsigned long)app->receive_error_count,
        (unsigned long)app->unexpected_receive_callback_count,
        (unsigned long)app->external_receive_start_failure_count,
        (unsigned long)app->external_receive_error_count,
        (unsigned long)app->unexpected_external_receive_callback_count,
        (unsigned long)app->lora_queue_overflow_count,
        (unsigned long)app->external_queue_overflow_count,
        (unsigned long)app->lora_transmit_failure_count,
        (unsigned long)app->external_transmit_failure_count);
    diagnostics_snapshot.receive_start_failure_count =
        app->receive_start_failure_count;
    diagnostics_snapshot.receive_error_count = app->receive_error_count;
    diagnostics_snapshot.unexpected_receive_callback_count =
        app->unexpected_receive_callback_count;
    diagnostics_snapshot.external_receive_start_failure_count =
        app->external_receive_start_failure_count;
    diagnostics_snapshot.external_receive_error_count =
        app->external_receive_error_count;
    diagnostics_snapshot.unexpected_external_receive_callback_count =
        app->unexpected_external_receive_callback_count;
    diagnostics_snapshot.lora_queue_overflow_count =
        app->lora_queue_overflow_count;
    diagnostics_snapshot.external_queue_overflow_count =
        app->external_queue_overflow_count;
    diagnostics_snapshot.lora_transmit_failure_count =
        app->lora_transmit_failure_count;
    diagnostics_snapshot.external_transmit_failure_count =
        app->external_transmit_failure_count;
    error_reported = true;
    last_error_report_tick = tick_ms;
  }
}

static void ECU_Diagnostics_ReportMonitor(uint32_t tick_ms,
                                          const EcuMonitor *monitor)
{
  const bool error_due =
      !error_reported ||
      ((uint32_t)(tick_ms - last_error_report_tick) >=
       ECU_DIAGNOSTICS_ERROR_INTERVAL_MS);

  if (monitor->authorized_card_count !=
      diagnostics_snapshot.authorized_card_count)
  {
    ECU_Diagnostics_Print(
        "[NFC] uid=%02X%02X%02X%02X authorized=YES count=%lu (+%lu)",
        monitor->last_uid[0], monitor->last_uid[1], monitor->last_uid[2],
        monitor->last_uid[3],
        (unsigned long)monitor->authorized_card_count,
        (unsigned long)(monitor->authorized_card_count -
                        diagnostics_snapshot.authorized_card_count));
    diagnostics_snapshot.authorized_card_count =
        monitor->authorized_card_count;
  }

  if (monitor->rejected_card_count != diagnostics_snapshot.rejected_card_count)
  {
    ECU_Diagnostics_Print(
        "[NFC] uid=%02X%02X%02X%02X authorized=NO count=%lu (+%lu)",
        monitor->last_uid[0], monitor->last_uid[1], monitor->last_uid[2],
        monitor->last_uid[3], (unsigned long)monitor->rejected_card_count,
        (unsigned long)(monitor->rejected_card_count -
                        diagnostics_snapshot.rejected_card_count));
    diagnostics_snapshot.rejected_card_count = monitor->rejected_card_count;
  }

  if (error_due &&
      (monitor->card_error_count != diagnostics_snapshot.card_error_count))
  {
    ECU_Diagnostics_Print("[ERROR] rc522=%s count=%lu (+%lu)",
                          ECU_Diagnostics_Rc522StatusName(
                              monitor->last_card_status),
                          (unsigned long)monitor->card_error_count,
                          (unsigned long)(monitor->card_error_count -
                                          diagnostics_snapshot.card_error_count));
    diagnostics_snapshot.card_error_count = monitor->card_error_count;
    error_reported = true;
    last_error_report_tick = tick_ms;
  }

  if (error_due &&
      (monitor->measurement_error_count !=
       diagnostics_snapshot.measurement_error_count))
  {
    ECU_Diagnostics_Print(
        "[ERROR] bmp280=%s count=%lu (+%lu)",
        ECU_Diagnostics_Bmp280StatusName(monitor->last_measurement_status),
        (unsigned long)monitor->measurement_error_count,
        (unsigned long)(monitor->measurement_error_count -
                        diagnostics_snapshot.measurement_error_count));
    diagnostics_snapshot.measurement_error_count =
        monitor->measurement_error_count;
    error_reported = true;
    last_error_report_tick = tick_ms;
  }

  if (monitor->height_estimate_count !=
      diagnostics_snapshot.height_estimate_count)
  {
    const uint32_t pressure_pa =
        (monitor->latest_measurement.pressure_q24_8_pa + 128U) / 256U;
    const int32_t height_cm = ECU_Diagnostics_RoundToInt32(
        monitor->latest_height.relative_height_m * 100.0);
    const int32_t pressure_change_pa = ECU_Diagnostics_RoundToInt32(
        monitor->latest_height.pressure_change_pa);

    ECU_Diagnostics_Print(
        "[BMP] sample=%lu pressure_pa=%lu temp_centi_c=%ld",
        (unsigned long)monitor->measurement_count,
        (unsigned long)pressure_pa,
        (long)monitor->latest_measurement.temperature_centi_c);
    ECU_Diagnostics_Print(
        "[HEIGHT] estimate=%lu height_cm=%ld floor=%ld motion=%s pressure_delta_pa=%ld",
        (unsigned long)monitor->height_estimate_count, (long)height_cm,
        (long)monitor->latest_height.floor_number,
        ECU_Diagnostics_MotionName(monitor->latest_height.motion),
        (long)pressure_change_pa);
    diagnostics_snapshot.height_estimate_count =
        monitor->height_estimate_count;
  }
}

void ECU_Diagnostics_Init(void)
{
  memset(&diagnostics_snapshot, 0, sizeof(diagnostics_snapshot));
  last_bridge_report_tick = 0U;
  last_error_report_tick = 0U;
  last_status_report_tick = HAL_GetTick();
  bridge_reported = false;
  error_reported = false;
  ECU_Diagnostics_Print("[BOOT] ECU diagnostics USART1=115200 8N1");
}

void ECU_Diagnostics_ReportInit(bool app_ok, bool monitor_ok)
{
  const EcuApp *app = ECU_App_HAL_GetState();
  const EcuMonitor *monitor = ECU_Monitor_HAL_GetState();
  const Bmp280 *bmp280 = BMP280_HAL_GetDevice();

  ECU_Diagnostics_Print(
      "[INIT] app=%s monitor=%s lora=%s mode=%s usart2=%s relay=%s",
      ECU_Diagnostics_OkText(app_ok), ECU_Diagnostics_OkText(monitor_ok),
      ECU_Diagnostics_OkText(app_ok),
      app != NULL ? ECU_Diagnostics_LoraModeName(app->lora_mode) : "UNKNOWN",
      ECU_Diagnostics_OkText(ECU_App_HAL_IsExternalUartAvailable()),
      app != NULL ? ECU_Diagnostics_RelayText(&app->control) : "OFF");
  ECU_Diagnostics_Print(
      "[INIT] rc522=%s version=0x%02X bmp280=%s chip_id=0x%02X",
      ECU_Diagnostics_OkText(ECU_Monitor_HAL_IsRc522Available()),
      RC522_HAL_GetVersion(),
      ECU_Diagnostics_OkText(ECU_Monitor_HAL_IsBmp280Available()),
      bmp280 != NULL ? bmp280->chip_id : 0U);

  ECU_Diagnostics_CaptureApp(app);
  ECU_Diagnostics_CaptureMonitor(monitor);
}

void ECU_Diagnostics_Process(uint32_t tick_ms)
{
  const EcuApp *app = ECU_App_HAL_GetState();
  const EcuMonitor *monitor = ECU_Monitor_HAL_GetState();

  if (app != NULL)
  {
    ECU_Diagnostics_ReportApp(tick_ms, app);
  }
  if (monitor != NULL)
  {
    ECU_Diagnostics_ReportMonitor(tick_ms, monitor);
  }

  if ((uint32_t)(tick_ms - last_status_report_tick) >=
      ECU_DIAGNOSTICS_STATUS_INTERVAL_MS)
  {
    last_status_report_tick = tick_ms;
    if (app != NULL)
    {
      ECU_Diagnostics_Print(
          "[STATUS] state=%s relay=%s usart2=%s lora_to_usart2=%lu usart2_to_lora=%lu nfc_auth=%s bmp_samples=%lu heights=%lu",
          ECU_Diagnostics_StateName(app->control.state),
          ECU_Diagnostics_RelayText(&app->control),
          ECU_App_HAL_IsExternalUartAvailable() ? "ONLINE" : "OFFLINE",
          (unsigned long)app->external_forwarded_byte_count,
          (unsigned long)app->lora_forwarded_byte_count,
          monitor != NULL ? ECU_Diagnostics_BoolText(monitor->authorized)
                          : "NO",
          monitor != NULL ? (unsigned long)monitor->measurement_count : 0UL,
          monitor != NULL ? (unsigned long)monitor->height_estimate_count
                          : 0UL);
    }
  }
}
