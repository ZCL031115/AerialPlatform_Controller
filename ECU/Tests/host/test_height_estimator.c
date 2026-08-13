#include "height_estimator.h"

#include <stddef.h>
#include <stdio.h>

#define TEST_CHECK(condition)                                                    \
  do                                                                             \
  {                                                                              \
    if (!(condition))                                                            \
    {                                                                            \
      fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition);     \
      return 1;                                                                  \
    }                                                                            \
  } while (0)

static bool add_batch(HeightEstimator *estimator, double pressure_pa,
                      double temperature_c, HeightEstimate *estimate)
{
  size_t index;
  bool ready = false;

  for (index = 0U; index < estimator->window_size; index++)
  {
    ready = HeightEstimator_AddSample(estimator, pressure_pa, temperature_c,
                                      estimate);
    if (index + 1U < estimator->window_size && ready)
    {
      return false;
    }
  }
  return ready;
}

static int test_baseline_is_first_floor(void)
{
  HeightEstimator estimator;
  HeightEstimate estimate;

  TEST_CHECK(HeightEstimator_Init(&estimator, 101325.0, 3.0, 1, 4U, 2.0));
  TEST_CHECK(add_batch(&estimator, 101325.0, 20.0, &estimate));
  TEST_CHECK(estimate.relative_height_m > -0.01);
  TEST_CHECK(estimate.relative_height_m < 0.01);
  TEST_CHECK(estimate.floor_number == 1);
  TEST_CHECK(estimate.motion == HEIGHT_MOTION_STATIONARY);
  return 0;
}

static int test_pressure_drop_means_ascending(void)
{
  HeightEstimator estimator;
  HeightEstimate estimate;

  TEST_CHECK(HeightEstimator_Init(&estimator, 101325.0, 3.0, 1, 4U, 2.0));
  TEST_CHECK(add_batch(&estimator, 101325.0, 20.0, &estimate));
  TEST_CHECK(add_batch(&estimator, 101289.0, 20.0, &estimate));
  TEST_CHECK(estimate.relative_height_m > 2.9);
  TEST_CHECK(estimate.relative_height_m < 3.2);
  TEST_CHECK(estimate.floor_number == 2);
  TEST_CHECK(estimate.pressure_change_pa == -36.0);
  TEST_CHECK(estimate.motion == HEIGHT_MOTION_ASCENDING);
  return 0;
}

static int test_pressure_rise_means_descending(void)
{
  HeightEstimator estimator;
  HeightEstimate estimate;

  TEST_CHECK(HeightEstimator_Init(&estimator, 101325.0, 3.0, 1, 2U, 2.0));
  TEST_CHECK(add_batch(&estimator, 101289.0, 20.0, &estimate));
  TEST_CHECK(add_batch(&estimator, 101325.0, 20.0, &estimate));
  TEST_CHECK(estimate.floor_number == 1);
  TEST_CHECK(estimate.pressure_change_pa == 36.0);
  TEST_CHECK(estimate.motion == HEIGHT_MOTION_DESCENDING);
  return 0;
}

static int test_small_change_is_stationary(void)
{
  HeightEstimator estimator;
  HeightEstimate estimate;

  TEST_CHECK(HeightEstimator_Init(&estimator, 101325.0, 3.0, 1, 2U, 2.0));
  TEST_CHECK(add_batch(&estimator, 101325.0, 20.0, &estimate));
  TEST_CHECK(add_batch(&estimator, 101324.0, 20.0, &estimate));
  TEST_CHECK(estimate.motion == HEIGHT_MOTION_STATIONARY);
  return 0;
}

static int test_invalid_arguments(void)
{
  HeightEstimator estimator = {0};
  HeightEstimate estimate;

  TEST_CHECK(!HeightEstimator_Init(NULL, 101325.0, 3.0, 1, 20U, 2.0));
  TEST_CHECK(!HeightEstimator_Init(&estimator, 0.0, 3.0, 1, 20U, 2.0));
  TEST_CHECK(!HeightEstimator_Init(&estimator, 101325.0, 3.0, 1, 21U,
                                   2.0));
  TEST_CHECK(!HeightEstimator_AddSample(&estimator, 101325.0, 20.0,
                                        &estimate));
  return 0;
}

typedef int (*TestFunction)(void);

typedef struct
{
  const char *name;
  TestFunction function;
} TestCase;

int main(void)
{
  static const TestCase tests[] = {
    {"baseline_is_first_floor", test_baseline_is_first_floor},
    {"pressure_drop_means_ascending", test_pressure_drop_means_ascending},
    {"pressure_rise_means_descending", test_pressure_rise_means_descending},
    {"small_change_is_stationary", test_small_change_is_stationary},
    {"invalid_arguments", test_invalid_arguments}
  };
  size_t index;

  for (index = 0U; index < (sizeof(tests) / sizeof(tests[0])); index++)
  {
    if (tests[index].function() != 0)
    {
      fprintf(stderr, "Test failed: %s\n", tests[index].name);
      return 1;
    }
    printf("PASS: %s\n", tests[index].name);
  }

  printf("All height estimator tests passed.\n");
  return 0;
}
