#include "ecu_monitor.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHECK(condition)                                                    \
  do                                                                             \
  {                                                                              \
    if (!(condition))                                                            \
    {                                                                            \
      fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition);     \
      return 1;                                                                  \
    }                                                                            \
  } while (0)

typedef struct
{
  Rc522Status uid_status;
  Rc522Status halt_status;
  Bmp280Status measurement_status;
  uint8_t uid[RC522_UID_SIZE];
  Bmp280Measurement measurement;
  uint32_t uid_call_count;
  uint32_t halt_call_count;
  uint32_t measurement_call_count;
} FakeMonitorIo;

static Rc522Status fake_read_uid(void *context,
                                 uint8_t uid[RC522_UID_SIZE])
{
  FakeMonitorIo *fake = (FakeMonitorIo *)context;

  fake->uid_call_count++;
  if (fake->uid_status == RC522_STATUS_OK)
  {
    memcpy(uid, fake->uid, RC522_UID_SIZE);
  }
  return fake->uid_status;
}

static Rc522Status fake_halt_card(void *context)
{
  FakeMonitorIo *fake = (FakeMonitorIo *)context;

  fake->halt_call_count++;
  return fake->halt_status;
}

static Bmp280Status fake_read_measurement(
    void *context, Bmp280Measurement *measurement)
{
  FakeMonitorIo *fake = (FakeMonitorIo *)context;

  fake->measurement_call_count++;
  if (fake->measurement_status == BMP280_STATUS_OK)
  {
    *measurement = fake->measurement;
  }
  return fake->measurement_status;
}

static EcuMonitorIo make_io(FakeMonitorIo *fake)
{
  const EcuMonitorIo io = {
    fake_read_uid,
    fake_halt_card,
    fake_read_measurement,
    fake
  };

  return io;
}

static EcuMonitorConfig make_config(void)
{
  const EcuMonitorConfig config = {
    {0x92U, 0xE6U, 0xC2U, 0x51U},
    3.0,
    2.0,
    4U,
    1
  };

  return config;
}

static int test_authorized_card_is_latched(void)
{
  EcuMonitor monitor;
  FakeMonitorIo fake = {0};
  EcuMonitorIo io;
  EcuMonitorConfig config = make_config();

  fake.uid_status = RC522_STATUS_OK;
  fake.halt_status = RC522_STATUS_OK;
  memcpy(fake.uid, config.authorized_uid, RC522_UID_SIZE);
  io = make_io(&fake);
  TEST_CHECK(ECU_Monitor_Init(&monitor, &io, &config));
  TEST_CHECK(ECU_Monitor_PollCard(&monitor) == RC522_STATUS_OK);
  TEST_CHECK(monitor.authorized);
  TEST_CHECK(monitor.authorized_card_count == 1U);
  TEST_CHECK(fake.halt_call_count == 1U);

  ECU_Monitor_SyncControlState(&monitor, ECU_CONTROL_EMERGENCY_STOPPED);
  TEST_CHECK(monitor.authorized);
  ECU_Monitor_SyncControlState(&monitor, ECU_CONTROL_DISCONNECTED);
  TEST_CHECK(!monitor.authorized);
  return 0;
}

static int test_wrong_card_and_no_card_are_separate(void)
{
  EcuMonitor monitor;
  FakeMonitorIo fake = {0};
  EcuMonitorIo io;
  EcuMonitorConfig config = make_config();

  fake.uid_status = RC522_STATUS_OK;
  fake.halt_status = RC522_STATUS_OK;
  fake.uid[0] = 0x11U;
  io = make_io(&fake);
  TEST_CHECK(ECU_Monitor_Init(&monitor, &io, &config));
  TEST_CHECK(ECU_Monitor_PollCard(&monitor) == RC522_STATUS_OK);
  TEST_CHECK(!monitor.authorized);
  TEST_CHECK(monitor.rejected_card_count == 1U);
  TEST_CHECK(monitor.card_error_count == 0U);

  fake.uid_status = RC522_STATUS_NO_CARD;
  TEST_CHECK(ECU_Monitor_PollCard(&monitor) == RC522_STATUS_NO_CARD);
  TEST_CHECK(monitor.card_poll_count == 2U);
  TEST_CHECK(monitor.card_error_count == 0U);
  return 0;
}

static int test_measurements_feed_height_estimator(void)
{
  EcuMonitor monitor;
  FakeMonitorIo fake = {0};
  EcuMonitorIo io;
  EcuMonitorConfig config = make_config();
  size_t index;

  fake.uid_status = RC522_STATUS_NO_CARD;
  fake.halt_status = RC522_STATUS_OK;
  fake.measurement_status = BMP280_STATUS_OK;
  fake.measurement.pressure_pa = 101325.0;
  fake.measurement.temperature_c = 20.0;
  io = make_io(&fake);
  TEST_CHECK(ECU_Monitor_Init(&monitor, &io, &config));

  for (index = 0U; index < config.height_window_size; index++)
  {
    TEST_CHECK(ECU_Monitor_SampleEnvironment(&monitor) ==
               BMP280_STATUS_OK);
  }
  TEST_CHECK(monitor.has_measurement);
  TEST_CHECK(monitor.measurement_count == 4U);
  TEST_CHECK(monitor.has_height_estimate);
  TEST_CHECK(monitor.height_estimate_count == 1U);
  TEST_CHECK(monitor.latest_height.floor_number == 1);
  return 0;
}

static int test_measurement_error_is_recorded(void)
{
  EcuMonitor monitor;
  FakeMonitorIo fake = {0};
  EcuMonitorIo io;
  EcuMonitorConfig config = make_config();

  fake.uid_status = RC522_STATUS_NO_CARD;
  fake.halt_status = RC522_STATUS_OK;
  fake.measurement_status = BMP280_STATUS_IO_ERROR;
  io = make_io(&fake);
  TEST_CHECK(ECU_Monitor_Init(&monitor, &io, &config));
  TEST_CHECK(ECU_Monitor_SampleEnvironment(&monitor) ==
             BMP280_STATUS_IO_ERROR);
  TEST_CHECK(monitor.measurement_error_count == 1U);
  TEST_CHECK(!monitor.has_measurement);
  return 0;
}

static int test_invalid_arguments(void)
{
  EcuMonitor monitor = {0};
  FakeMonitorIo fake = {0};
  EcuMonitorIo io = make_io(&fake);
  EcuMonitorConfig config = make_config();

  TEST_CHECK(!ECU_Monitor_Init(NULL, &io, &config));
  io.read_uid = NULL;
  TEST_CHECK(!ECU_Monitor_Init(&monitor, &io, &config));
  TEST_CHECK(ECU_Monitor_PollCard(&monitor) ==
             RC522_STATUS_INVALID_ARGUMENT);
  TEST_CHECK(ECU_Monitor_SampleEnvironment(&monitor) ==
             BMP280_STATUS_INVALID_ARGUMENT);
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
    {"authorized_card_is_latched", test_authorized_card_is_latched},
    {"wrong_card_and_no_card_are_separate",
     test_wrong_card_and_no_card_are_separate},
    {"measurements_feed_height_estimator",
     test_measurements_feed_height_estimator},
    {"measurement_error_is_recorded", test_measurement_error_is_recorded},
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

  printf("All ECU monitor tests passed.\n");
  return 0;
}
