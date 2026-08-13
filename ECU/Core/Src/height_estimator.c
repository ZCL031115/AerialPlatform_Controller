#include "height_estimator.h"

#include <math.h>
#include <stddef.h>

static int32_t HeightEstimator_RoundToInt(double value)
{
  return (value >= 0.0) ? (int32_t)(value + 0.5)
                        : (int32_t)(value - 0.5);
}

bool HeightEstimator_Init(HeightEstimator *estimator,
                          double reference_pressure_pa,
                          double floor_height_m, int32_t reference_floor,
                          size_t window_size, double movement_threshold_pa)
{
  if ((estimator == NULL) || (reference_pressure_pa <= 0.0) ||
      (floor_height_m <= 0.0) || (window_size == 0U) ||
      (window_size > HEIGHT_ESTIMATOR_MAX_WINDOW_SIZE) ||
      (movement_threshold_pa < 0.0))
  {
    return false;
  }

  estimator->reference_pressure_pa = reference_pressure_pa;
  estimator->floor_height_m = floor_height_m;
  estimator->movement_threshold_pa = movement_threshold_pa;
  estimator->pressure_sum = 0.0;
  estimator->temperature_sum = 0.0;
  estimator->previous_average_pressure_pa = 0.0;
  estimator->sample_count = 0U;
  estimator->window_size = window_size;
  estimator->reference_floor = reference_floor;
  estimator->has_previous_average = false;
  estimator->initialized = true;
  return true;
}

bool HeightEstimator_AddSample(HeightEstimator *estimator, double pressure_pa,
                               double temperature_c,
                               HeightEstimate *estimate)
{
  double height_per_pressure_decade;
  double floor_offset;

  if ((estimator == NULL) || !estimator->initialized ||
      (pressure_pa <= 0.0) || (temperature_c <= -273.15) ||
      (estimate == NULL))
  {
    return false;
  }

  estimator->pressure_sum += pressure_pa;
  estimator->temperature_sum += temperature_c;
  estimator->sample_count++;
  if (estimator->sample_count < estimator->window_size)
  {
    return false;
  }

  estimate->average_pressure_pa =
      estimator->pressure_sum / (double)estimator->window_size;
  estimate->average_temperature_c =
      estimator->temperature_sum / (double)estimator->window_size;
  height_per_pressure_decade =
      18400.0 * (1.0 + estimate->average_temperature_c / 273.15);
  estimate->relative_height_m =
      height_per_pressure_decade *
      log10(estimator->reference_pressure_pa /
            estimate->average_pressure_pa);
  floor_offset = estimate->relative_height_m / estimator->floor_height_m;
  estimate->floor_number =
      estimator->reference_floor + HeightEstimator_RoundToInt(floor_offset);

  if (!estimator->has_previous_average)
  {
    estimate->pressure_change_pa = 0.0;
    estimate->motion = HEIGHT_MOTION_STATIONARY;
    estimator->has_previous_average = true;
  }
  else
  {
    estimate->pressure_change_pa =
        estimate->average_pressure_pa -
        estimator->previous_average_pressure_pa;
    if (estimate->pressure_change_pa < -estimator->movement_threshold_pa)
    {
      estimate->motion = HEIGHT_MOTION_ASCENDING;
    }
    else if (estimate->pressure_change_pa >
             estimator->movement_threshold_pa)
    {
      estimate->motion = HEIGHT_MOTION_DESCENDING;
    }
    else
    {
      estimate->motion = HEIGHT_MOTION_STATIONARY;
    }
  }

  estimator->previous_average_pressure_pa = estimate->average_pressure_pa;
  estimator->pressure_sum = 0.0;
  estimator->temperature_sum = 0.0;
  estimator->sample_count = 0U;
  return true;
}
