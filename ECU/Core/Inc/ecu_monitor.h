#ifndef ECU_MONITOR_H
#define ECU_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bmp280.h"
#include "ecu_control.h"
#include "height_estimator.h"
#include "rc522.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef Rc522Status (*EcuMonitorReadUidFn)(void *context,
                                           uint8_t uid[RC522_UID_SIZE]);
typedef Rc522Status (*EcuMonitorHaltCardFn)(void *context);
typedef Bmp280Status (*EcuMonitorReadMeasurementFn)(
    void *context, Bmp280Measurement *measurement);

typedef struct
{
  EcuMonitorReadUidFn read_uid;
  EcuMonitorHaltCardFn halt_card;
  EcuMonitorReadMeasurementFn read_measurement;
  void *context;
} EcuMonitorIo;

typedef struct
{
  uint8_t authorized_uid[RC522_UID_SIZE];
  double floor_height_m;
  double movement_threshold_pa;
  size_t height_window_size;
  int32_t reference_floor;
} EcuMonitorConfig;

typedef struct
{
  EcuMonitorIo io;
  EcuMonitorConfig config;
  uint8_t last_uid[RC522_UID_SIZE];
  Rc522Status last_card_status;
  Bmp280Status last_measurement_status;
  Bmp280Measurement latest_measurement;
  HeightEstimator height_estimator;
  HeightEstimate latest_height;
  uint32_t card_poll_count;
  uint32_t authorized_card_count;
  uint32_t rejected_card_count;
  uint32_t card_error_count;
  uint32_t measurement_count;
  uint32_t measurement_error_count;
  uint32_t height_estimate_count;
  bool authorized;
  bool has_measurement;
  bool has_height_estimate;
  bool height_estimator_initialized;
  bool initialized;
} EcuMonitor;

bool ECU_Monitor_Init(EcuMonitor *monitor, const EcuMonitorIo *io,
                      const EcuMonitorConfig *config);
Rc522Status ECU_Monitor_PollCard(EcuMonitor *monitor);
Bmp280Status ECU_Monitor_SampleEnvironment(EcuMonitor *monitor);
void ECU_Monitor_SyncControlState(EcuMonitor *monitor,
                                  EcuControlState control_state);

#ifdef __cplusplus
}
#endif

#endif
