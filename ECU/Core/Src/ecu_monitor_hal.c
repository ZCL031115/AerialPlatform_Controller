#include "ecu_monitor_hal.h"

#include "bmp280_hal.h"
#include "rc522_hal.h"

#include <stddef.h>

enum
{
  ECU_MONITOR_CARD_INTERVAL_MS = 100U,
  ECU_MONITOR_MEASUREMENT_INTERVAL_MS = 50U
};

static EcuMonitor ecu_monitor;
static bool ecu_monitor_initialized;
static bool rc522_available;
static bool bmp280_available;
static uint32_t last_card_tick;
static uint32_t last_measurement_tick;

static Rc522Status ECU_Monitor_HAL_ReadUid(void *context,
                                           uint8_t uid[RC522_UID_SIZE])
{
  (void)context;
  return RC522_HAL_ReadUid(uid);
}

static Rc522Status ECU_Monitor_HAL_HaltCard(void *context)
{
  (void)context;
  return RC522_HAL_Halt();
}

static Bmp280Status ECU_Monitor_HAL_ReadMeasurement(
    void *context, Bmp280Measurement *measurement)
{
  (void)context;
  return BMP280_HAL_ReadMeasurement(measurement);
}

bool ECU_Monitor_HAL_Init(void)
{
  static const EcuMonitorConfig config = {
      {0x92U, 0xE6U, 0xC2U, 0x51U},
      3.0,
      2.0,
      20U,
      1};
  const EcuMonitorIo io = {
      ECU_Monitor_HAL_ReadUid,
      ECU_Monitor_HAL_HaltCard,
      ECU_Monitor_HAL_ReadMeasurement,
      NULL};

  ecu_monitor_initialized = ECU_Monitor_Init(&ecu_monitor, &io, &config);
  if (!ecu_monitor_initialized)
  {
    return false;
  }

  rc522_available = RC522_HAL_Init();
  bmp280_available = BMP280_HAL_Init();
  last_card_tick = 0U;
  last_measurement_tick = 0U;
  return true;
}

void ECU_Monitor_HAL_Process(uint32_t tick_ms,
                             EcuControlState control_state)
{
  if (!ecu_monitor_initialized)
  {
    return;
  }

  ECU_Monitor_SyncControlState(&ecu_monitor, control_state);
  if (rc522_available &&
      ((uint32_t)(tick_ms - last_card_tick) >=
       ECU_MONITOR_CARD_INTERVAL_MS))
  {
    last_card_tick = tick_ms;
    (void)ECU_Monitor_PollCard(&ecu_monitor);
  }

  if (bmp280_available &&
      ((uint32_t)(tick_ms - last_measurement_tick) >=
       ECU_MONITOR_MEASUREMENT_INTERVAL_MS))
  {
    last_measurement_tick = tick_ms;
    (void)ECU_Monitor_SampleEnvironment(&ecu_monitor);
  }
}

const EcuMonitor *ECU_Monitor_HAL_GetState(void)
{
  return ecu_monitor_initialized ? &ecu_monitor : NULL;
}

bool ECU_Monitor_HAL_IsRc522Available(void)
{
  return rc522_available;
}

bool ECU_Monitor_HAL_IsBmp280Available(void)
{
  return bmp280_available;
}
