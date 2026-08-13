#include "ecu_monitor.h"

#include <stddef.h>
#include <string.h>

bool ECU_Monitor_Init(EcuMonitor *monitor, const EcuMonitorIo *io,
                      const EcuMonitorConfig *config)
{
  if ((monitor == NULL) || (io == NULL) || (config == NULL) ||
      (io->read_uid == NULL) || (io->halt_card == NULL) ||
      (io->read_measurement == NULL) || (config->floor_height_m <= 0.0) ||
      (config->movement_threshold_pa < 0.0) ||
      (config->height_window_size == 0U) ||
      (config->height_window_size > HEIGHT_ESTIMATOR_MAX_WINDOW_SIZE))
  {
    return false;
  }

  memset(monitor, 0, sizeof(*monitor));
  monitor->io = *io;
  monitor->config = *config;
  monitor->last_card_status = RC522_STATUS_NO_CARD;
  monitor->last_measurement_status = BMP280_STATUS_INVALID_ARGUMENT;
  monitor->initialized = true;
  return true;
}

Rc522Status ECU_Monitor_PollCard(EcuMonitor *monitor)
{
  Rc522Status status;

  if ((monitor == NULL) || !monitor->initialized)
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  monitor->card_poll_count++;
  status = monitor->io.read_uid(monitor->io.context, monitor->last_uid);
  monitor->last_card_status = status;
  if (status == RC522_STATUS_NO_CARD)
  {
    return status;
  }
  if (status != RC522_STATUS_OK)
  {
    monitor->card_error_count++;
    return status;
  }

  if (memcmp(monitor->last_uid, monitor->config.authorized_uid,
             RC522_UID_SIZE) == 0)
  {
    monitor->authorized = true;
    monitor->authorized_card_count++;
  }
  else
  {
    monitor->rejected_card_count++;
  }

  status = monitor->io.halt_card(monitor->io.context);
  if (status != RC522_STATUS_OK)
  {
    monitor->last_card_status = status;
    monitor->card_error_count++;
  }
  return status;
}

Bmp280Status ECU_Monitor_SampleEnvironment(EcuMonitor *monitor)
{
  Bmp280Status status;

  if ((monitor == NULL) || !monitor->initialized)
  {
    return BMP280_STATUS_INVALID_ARGUMENT;
  }

  status = monitor->io.read_measurement(monitor->io.context,
                                        &monitor->latest_measurement);
  monitor->last_measurement_status = status;
  if (status != BMP280_STATUS_OK)
  {
    monitor->measurement_error_count++;
    return status;
  }

  monitor->measurement_count++;
  monitor->has_measurement = true;
  if (!monitor->height_estimator_initialized)
  {
    monitor->height_estimator_initialized = HeightEstimator_Init(
        &monitor->height_estimator, monitor->latest_measurement.pressure_pa,
        monitor->config.floor_height_m, monitor->config.reference_floor,
        monitor->config.height_window_size,
        monitor->config.movement_threshold_pa);
  }

  if (monitor->height_estimator_initialized &&
      HeightEstimator_AddSample(
          &monitor->height_estimator,
          monitor->latest_measurement.pressure_pa,
          monitor->latest_measurement.temperature_c,
          &monitor->latest_height))
  {
    monitor->has_height_estimate = true;
    monitor->height_estimate_count++;
  }

  return BMP280_STATUS_OK;
}

void ECU_Monitor_SyncControlState(EcuMonitor *monitor,
                                  EcuControlState control_state)
{
  if ((monitor == NULL) || !monitor->initialized)
  {
    return;
  }

  if ((control_state == ECU_CONTROL_SAFE_STARTUP) ||
      (control_state == ECU_CONTROL_DISCONNECTED))
  {
    monitor->authorized = false;
  }
}
