#include "rc522_soft_spi.h"

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
  bool cs;
  bool sck;
  bool mosi;
  bool reset;
  uint8_t sampled_mosi[64];
  size_t sampled_mosi_count;
  uint8_t miso_bits[64];
  size_t miso_bit_count;
  size_t miso_bit_index;
  uint32_t delay_call_count;
  uint32_t delay_total_us;
  uint32_t cs_low_count;
  uint32_t cs_high_count;
} FakePins;

static void fake_write_cs(void *context, bool high)
{
  FakePins *pins = (FakePins *)context;

  pins->cs = high;
  if (high)
  {
    pins->cs_high_count++;
  }
  else
  {
    pins->cs_low_count++;
  }
}

static void fake_write_sck(void *context, bool high)
{
  FakePins *pins = (FakePins *)context;

  if (!pins->sck && high &&
      (pins->sampled_mosi_count < sizeof(pins->sampled_mosi)))
  {
    pins->sampled_mosi[pins->sampled_mosi_count++] = pins->mosi ? 1U : 0U;
  }
  pins->sck = high;
}

static void fake_write_mosi(void *context, bool high)
{
  ((FakePins *)context)->mosi = high;
}

static bool fake_read_miso(void *context)
{
  FakePins *pins = (FakePins *)context;

  if (pins->miso_bit_index >= pins->miso_bit_count)
  {
    return false;
  }

  return pins->miso_bits[pins->miso_bit_index++] != 0U;
}

static void fake_write_reset(void *context, bool high)
{
  ((FakePins *)context)->reset = high;
}

static void fake_delay_us(void *context, uint32_t microseconds)
{
  FakePins *pins = (FakePins *)context;

  pins->delay_call_count++;
  pins->delay_total_us += microseconds;
}

static Rc522SoftSpiIo make_io(FakePins *pins)
{
  const Rc522SoftSpiIo io = {
    fake_write_cs,
    fake_write_sck,
    fake_write_mosi,
    fake_read_miso,
    fake_write_reset,
    fake_delay_us,
    pins
  };

  return io;
}

static void append_miso_byte(FakePins *pins, uint8_t value)
{
  uint8_t bit;

  for (bit = 0U; bit < 8U; bit++)
  {
    pins->miso_bits[pins->miso_bit_count++] =
        ((value & 0x80U) != 0U) ? 1U : 0U;
    value = (uint8_t)(value << 1U);
  }
}

static uint8_t sampled_byte(const FakePins *pins, size_t byte_index)
{
  uint8_t value = 0U;
  size_t bit;
  const size_t first_bit = byte_index * 8U;

  for (bit = 0U; bit < 8U; bit++)
  {
    value = (uint8_t)((value << 1U) |
                      pins->sampled_mosi[first_bit + bit]);
  }
  return value;
}

static int test_init_sets_idle_levels(void)
{
  Rc522SoftSpi bus;
  FakePins pins = {0};
  Rc522SoftSpiIo io = make_io(&pins);

  TEST_CHECK(RC522_SoftSpi_Init(&bus, &io, 2U));
  TEST_CHECK(pins.cs);
  TEST_CHECK(!pins.sck);
  TEST_CHECK(!pins.mosi);
  TEST_CHECK(pins.reset);
  TEST_CHECK(bus.half_cycle_us == 2U);
  return 0;
}

static int test_transfer_byte_is_msb_first(void)
{
  Rc522SoftSpi bus;
  FakePins pins = {0};
  Rc522SoftSpiIo io = make_io(&pins);

  TEST_CHECK(RC522_SoftSpi_Init(&bus, &io, 1U));
  append_miso_byte(&pins, 0x3CU);
  TEST_CHECK(RC522_SoftSpi_TransferByte(&bus, 0xA5U) == 0x3CU);
  TEST_CHECK(pins.sampled_mosi_count == 8U);
  TEST_CHECK(sampled_byte(&pins, 0U) == 0xA5U);
  TEST_CHECK(pins.delay_call_count == 24U);
  TEST_CHECK(pins.delay_total_us == 24U);
  TEST_CHECK(!pins.sck);
  return 0;
}

static int test_register_transactions(void)
{
  Rc522SoftSpi bus;
  Rc522Io device_io;
  FakePins pins = {0};
  Rc522SoftSpiIo io = make_io(&pins);

  TEST_CHECK(RC522_SoftSpi_Init(&bus, &io, 1U));
  TEST_CHECK(RC522_SoftSpi_BuildDeviceIo(&bus, &device_io));

  pins.sampled_mosi_count = 0U;
  device_io.write_register(device_io.context, 0x14U, 0x5AU);
  TEST_CHECK(pins.cs);
  TEST_CHECK(pins.cs_low_count == 1U);
  TEST_CHECK(sampled_byte(&pins, 0U) == 0x28U);
  TEST_CHECK(sampled_byte(&pins, 1U) == 0x5AU);

  pins.sampled_mosi_count = 0U;
  append_miso_byte(&pins, 0x00U);
  append_miso_byte(&pins, 0x92U);
  TEST_CHECK(device_io.read_register(device_io.context, 0x37U) == 0x92U);
  TEST_CHECK(sampled_byte(&pins, 0U) == 0xEEU);
  TEST_CHECK(sampled_byte(&pins, 1U) == 0x00U);
  TEST_CHECK(pins.cs_low_count == 2U);
  return 0;
}

static int test_invalid_arguments(void)
{
  Rc522SoftSpi bus = {0};
  Rc522Io device_io;
  FakePins pins = {0};
  Rc522SoftSpiIo io = make_io(&pins);

  TEST_CHECK(!RC522_SoftSpi_Init(NULL, &io, 1U));
  io.read_miso = NULL;
  TEST_CHECK(!RC522_SoftSpi_Init(&bus, &io, 1U));
  TEST_CHECK(!RC522_SoftSpi_BuildDeviceIo(&bus, &device_io));
  TEST_CHECK(RC522_SoftSpi_TransferByte(NULL, 0xFFU) == 0U);
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
    {"init_sets_idle_levels", test_init_sets_idle_levels},
    {"transfer_byte_is_msb_first", test_transfer_byte_is_msb_first},
    {"register_transactions", test_register_transactions},
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

  printf("All RC522 software SPI tests passed.\n");
  return 0;
}
