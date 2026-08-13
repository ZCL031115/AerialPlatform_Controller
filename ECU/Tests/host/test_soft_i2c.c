#include "soft_i2c.h"

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

typedef struct
{
  bool scl;
  bool sda;
  uint8_t read_bits[128];
  size_t read_bit_count;
  size_t read_bit_index;
  uint8_t rising_sda[256];
  size_t rising_count;
  uint32_t delay_total_us;
} FakeI2cPins;

static void fake_write_scl(void *context, bool high)
{
  FakeI2cPins *pins = (FakeI2cPins *)context;

  if (!pins->scl && high && (pins->rising_count < sizeof(pins->rising_sda)))
  {
    pins->rising_sda[pins->rising_count++] = pins->sda ? 1U : 0U;
  }
  pins->scl = high;
}

static void fake_write_sda(void *context, bool high)
{
  ((FakeI2cPins *)context)->sda = high;
}

static bool fake_read_sda(void *context)
{
  FakeI2cPins *pins = (FakeI2cPins *)context;

  if (pins->read_bit_index >= pins->read_bit_count)
  {
    return true;
  }
  return pins->read_bits[pins->read_bit_index++] != 0U;
}

static void fake_delay_us(void *context, uint32_t microseconds)
{
  ((FakeI2cPins *)context)->delay_total_us += microseconds;
}

static SoftI2cIo make_io(FakeI2cPins *pins)
{
  const SoftI2cIo io = {
    fake_write_scl,
    fake_write_sda,
    fake_read_sda,
    fake_delay_us,
    pins
  };

  return io;
}

static void append_read_bit(FakeI2cPins *pins, bool high)
{
  pins->read_bits[pins->read_bit_count++] = high ? 1U : 0U;
}

static void append_read_byte(FakeI2cPins *pins, uint8_t value)
{
  uint8_t bit;

  for (bit = 0U; bit < 8U; bit++)
  {
    append_read_bit(pins, (value & 0x80U) != 0U);
    value = (uint8_t)(value << 1U);
  }
}

static uint8_t sampled_byte(const FakeI2cPins *pins, size_t first_sample)
{
  uint8_t value = 0U;
  size_t bit;

  for (bit = 0U; bit < 8U; bit++)
  {
    value = (uint8_t)((value << 1U) |
                      pins->rising_sda[first_sample + bit]);
  }
  return value;
}

static int test_init_releases_bus(void)
{
  SoftI2c bus;
  FakeI2cPins pins = {0};
  SoftI2cIo io = make_io(&pins);

  TEST_CHECK(SoftI2c_Init(&bus, &io, 10U));
  TEST_CHECK(pins.scl);
  TEST_CHECK(pins.sda);
  TEST_CHECK(bus.half_cycle_us == 10U);
  return 0;
}

static int test_write_sends_address_and_data(void)
{
  SoftI2c bus;
  FakeI2cPins pins = {0};
  SoftI2cIo io = make_io(&pins);
  const uint8_t data[2] = {0xF4U, 0x2FU};

  append_read_bit(&pins, false);
  append_read_bit(&pins, false);
  append_read_bit(&pins, false);
  TEST_CHECK(SoftI2c_Init(&bus, &io, 1U));
  pins.rising_count = 0U;
  TEST_CHECK(SoftI2c_Write(&bus, 0x76U, data, sizeof(data)));
  TEST_CHECK(sampled_byte(&pins, 0U) == 0xECU);
  TEST_CHECK(sampled_byte(&pins, 9U) == 0xF4U);
  TEST_CHECK(sampled_byte(&pins, 18U) == 0x2FU);
  TEST_CHECK(pins.scl);
  TEST_CHECK(pins.sda);
  return 0;
}

static int test_write_read_uses_ack_and_nack(void)
{
  SoftI2c bus;
  FakeI2cPins pins = {0};
  SoftI2cIo io = make_io(&pins);
  const uint8_t register_address = 0xD0U;
  uint8_t value = 0U;

  append_read_bit(&pins, false);
  append_read_bit(&pins, false);
  append_read_bit(&pins, false);
  append_read_byte(&pins, 0x58U);
  TEST_CHECK(SoftI2c_Init(&bus, &io, 1U));
  TEST_CHECK(SoftI2c_WriteRead(&bus, 0x76U, &register_address, 1U, &value,
                               1U));
  TEST_CHECK(value == 0x58U);
  TEST_CHECK(pins.read_bit_index == 11U);
  TEST_CHECK(pins.scl);
  TEST_CHECK(pins.sda);
  return 0;
}

static int test_nack_stops_transaction(void)
{
  SoftI2c bus;
  FakeI2cPins pins = {0};
  SoftI2cIo io = make_io(&pins);
  const uint8_t data = 0x00U;

  append_read_bit(&pins, true);
  TEST_CHECK(SoftI2c_Init(&bus, &io, 1U));
  TEST_CHECK(!SoftI2c_Write(&bus, 0x76U, &data, 1U));
  TEST_CHECK(pins.read_bit_index == 1U);
  TEST_CHECK(pins.scl);
  TEST_CHECK(pins.sda);
  return 0;
}

static int test_invalid_arguments(void)
{
  SoftI2c bus = {0};
  FakeI2cPins pins = {0};
  SoftI2cIo io = make_io(&pins);
  uint8_t data = 0U;

  TEST_CHECK(!SoftI2c_Init(NULL, &io, 1U));
  io.read_sda = NULL;
  TEST_CHECK(!SoftI2c_Init(&bus, &io, 1U));
  TEST_CHECK(!SoftI2c_Write(&bus, 0x76U, &data, 1U));
  TEST_CHECK(!SoftI2c_WriteRead(&bus, 0x76U, &data, 1U, &data, 1U));
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
    {"init_releases_bus", test_init_releases_bus},
    {"write_sends_address_and_data", test_write_sends_address_and_data},
    {"write_read_uses_ack_and_nack", test_write_read_uses_ack_and_nack},
    {"nack_stops_transaction", test_nack_stops_transaction},
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

  printf("All software I2C tests passed.\n");
  return 0;
}
