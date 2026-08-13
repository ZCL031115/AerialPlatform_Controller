#include "bmp280.h"

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
  uint8_t registers[256];
  bool read_fails;
  bool write_fails;
  uint32_t read_count;
  uint32_t write_count;
  uint32_t delay_total_ms;
} FakeBmp280;

static void put_u16(FakeBmp280 *fake, uint8_t address, uint16_t value)
{
  fake->registers[address] = (uint8_t)(value & 0x00FFU);
  fake->registers[(uint8_t)(address + 1U)] = (uint8_t)(value >> 8U);
}

static void put_s16(FakeBmp280 *fake, uint8_t address, int16_t value)
{
  put_u16(fake, address, (uint16_t)value);
}

static void put_raw20(FakeBmp280 *fake, uint8_t address, int32_t value)
{
  fake->registers[address] = (uint8_t)((uint32_t)value >> 12U);
  fake->registers[(uint8_t)(address + 1U)] =
      (uint8_t)((uint32_t)value >> 4U);
  fake->registers[(uint8_t)(address + 2U)] =
      (uint8_t)((uint32_t)value << 4U);
}

static void init_fake(FakeBmp280 *fake)
{
  memset(fake, 0, sizeof(*fake));
  fake->registers[0xD0U] = BMP280_CHIP_ID;

  put_u16(fake, 0x88U, 27504U);
  put_s16(fake, 0x8AU, 26435);
  put_s16(fake, 0x8CU, -1000);
  put_u16(fake, 0x8EU, 36477U);
  put_s16(fake, 0x90U, -10685);
  put_s16(fake, 0x92U, 3024);
  put_s16(fake, 0x94U, 2855);
  put_s16(fake, 0x96U, 140);
  put_s16(fake, 0x98U, -7);
  put_s16(fake, 0x9AU, 15500);
  put_s16(fake, 0x9CU, -14600);
  put_s16(fake, 0x9EU, 6000);

  put_raw20(fake, 0xF7U, 415148);
  put_raw20(fake, 0xFAU, 519888);
}

static bool fake_read_registers(void *context, uint8_t start_register,
                                uint8_t *data, size_t length)
{
  FakeBmp280 *fake = (FakeBmp280 *)context;

  fake->read_count++;
  if (fake->read_fails)
  {
    return false;
  }
  memcpy(data, &fake->registers[start_register], length);
  return true;
}

static bool fake_write_register(void *context, uint8_t address, uint8_t value)
{
  FakeBmp280 *fake = (FakeBmp280 *)context;

  fake->write_count++;
  if (fake->write_fails)
  {
    return false;
  }
  fake->registers[address] = value;
  return true;
}

static void fake_delay_ms(void *context, uint32_t milliseconds)
{
  ((FakeBmp280 *)context)->delay_total_ms += milliseconds;
}

static Bmp280Io make_io(FakeBmp280 *fake)
{
  const Bmp280Io io = {
    fake_read_registers,
    fake_write_register,
    fake_delay_ms,
    fake
  };

  return io;
}

static int test_init_reads_calibration_and_configures(void)
{
  Bmp280 device;
  FakeBmp280 fake;
  Bmp280Io io;

  init_fake(&fake);
  io = make_io(&fake);
  TEST_CHECK(BMP280_Init(&device, &io) == BMP280_STATUS_OK);
  TEST_CHECK(device.initialized);
  TEST_CHECK(device.chip_id == BMP280_CHIP_ID);
  TEST_CHECK(device.calibration.t1 == 27504U);
  TEST_CHECK(device.calibration.t2 == 26435);
  TEST_CHECK(device.calibration.p1 == 36477U);
  TEST_CHECK(device.calibration.p8 == -14600);
  TEST_CHECK(fake.registers[0xE0U] == 0xB6U);
  TEST_CHECK(fake.registers[0xF5U] == 0x10U);
  TEST_CHECK(fake.registers[0xF4U] == 0x2FU);
  TEST_CHECK(fake.delay_total_ms == 2U);
  return 0;
}

static int test_datasheet_compensation_sample(void)
{
  Bmp280 device;
  Bmp280Measurement measurement;
  FakeBmp280 fake;
  Bmp280Io io;

  init_fake(&fake);
  io = make_io(&fake);
  TEST_CHECK(BMP280_Init(&device, &io) == BMP280_STATUS_OK);
  TEST_CHECK(BMP280_ReadMeasurement(&device, &measurement) ==
             BMP280_STATUS_OK);
  TEST_CHECK(measurement.raw_temperature == 519888);
  TEST_CHECK(measurement.raw_pressure == 415148);
  TEST_CHECK(measurement.temperature_centi_c == 2508);
  TEST_CHECK(measurement.temperature_c > 25.07);
  TEST_CHECK(measurement.temperature_c < 25.09);
  TEST_CHECK(measurement.pressure_pa > 100653.0);
  TEST_CHECK(measurement.pressure_pa < 100654.0);
  return 0;
}

static int test_bad_id_and_calibration_are_rejected(void)
{
  Bmp280 device;
  FakeBmp280 fake;
  Bmp280Io io;

  init_fake(&fake);
  fake.registers[0xD0U] = 0x00U;
  io = make_io(&fake);
  TEST_CHECK(BMP280_Init(&device, &io) == BMP280_STATUS_BAD_CHIP_ID);

  init_fake(&fake);
  put_u16(&fake, 0x8EU, 0U);
  io = make_io(&fake);
  TEST_CHECK(BMP280_Init(&device, &io) == BMP280_STATUS_BAD_CALIBRATION);
  return 0;
}

static int test_reset_timeout_is_bounded(void)
{
  Bmp280 device;
  FakeBmp280 fake;
  Bmp280Io io;

  init_fake(&fake);
  fake.registers[0xF3U] = 0x01U;
  io = make_io(&fake);
  TEST_CHECK(BMP280_Init(&device, &io) == BMP280_STATUS_TIMEOUT);
  TEST_CHECK(fake.delay_total_ms == 102U);
  return 0;
}

static int test_io_errors_and_invalid_arguments(void)
{
  Bmp280 device = {0};
  Bmp280Measurement measurement;
  FakeBmp280 fake;
  Bmp280Io io;

  init_fake(&fake);
  io = make_io(&fake);
  TEST_CHECK(BMP280_Init(NULL, &io) == BMP280_STATUS_INVALID_ARGUMENT);
  io.read_registers = NULL;
  TEST_CHECK(BMP280_Init(&device, &io) == BMP280_STATUS_INVALID_ARGUMENT);

  io = make_io(&fake);
  fake.read_fails = true;
  TEST_CHECK(BMP280_Init(&device, &io) == BMP280_STATUS_IO_ERROR);
  TEST_CHECK(BMP280_ReadMeasurement(&device, &measurement) ==
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
    {"init_reads_calibration_and_configures",
     test_init_reads_calibration_and_configures},
    {"datasheet_compensation_sample", test_datasheet_compensation_sample},
    {"bad_id_and_calibration_are_rejected",
     test_bad_id_and_calibration_are_rejected},
    {"reset_timeout_is_bounded", test_reset_timeout_is_bounded},
    {"io_errors_and_invalid_arguments",
     test_io_errors_and_invalid_arguments}
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

  printf("All BMP280 tests passed.\n");
  return 0;
}
