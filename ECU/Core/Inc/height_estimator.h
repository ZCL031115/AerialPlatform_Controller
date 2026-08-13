#ifndef HEIGHT_ESTIMATOR_H
#define HEIGHT_ESTIMATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
  HEIGHT_ESTIMATOR_MAX_WINDOW_SIZE = 20U
};

typedef enum
{
  HEIGHT_MOTION_STATIONARY = 0,
  HEIGHT_MOTION_ASCENDING,
  HEIGHT_MOTION_DESCENDING
} HeightMotion;

typedef struct
{
  double average_pressure_pa;
  double average_temperature_c;
  double relative_height_m;
  double pressure_change_pa;
  int32_t floor_number;
  HeightMotion motion;
} HeightEstimate;

typedef struct
{
  double reference_pressure_pa;
  double floor_height_m;
  double movement_threshold_pa;
  double pressure_sum;
  double temperature_sum;
  double previous_average_pressure_pa;
  size_t sample_count;
  size_t window_size;
  int32_t reference_floor;
  bool has_previous_average;
  bool initialized;
} HeightEstimator;

bool HeightEstimator_Init(HeightEstimator *estimator,
                          double reference_pressure_pa,
                          double floor_height_m, int32_t reference_floor,
                          size_t window_size, double movement_threshold_pa);
bool HeightEstimator_AddSample(HeightEstimator *estimator, double pressure_pa,
                               double temperature_c,
                               HeightEstimate *estimate);

#ifdef __cplusplus
}
#endif

#endif
