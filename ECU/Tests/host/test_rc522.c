#include "rc522.h"

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

enum
{
  REG_COMMAND = 0x01U,
  REG_DIV_IRQ = 0x05U,
  REG_COM_IRQ = 0x04U,
  REG_ERROR = 0x06U,
  REG_FIFO_DATA = 0x09U,
  REG_FIFO_LEVEL = 0x0AU,
  REG_CONTROL = 0x0CU,
  REG_TX_CONTROL = 0x14U,
  REG_CRC_RESULT_HIGH = 0x21U,
  REG_CRC_RESULT_LOW = 0x22U,
  REG_VERSION = 0x37U,
  COMMAND_IDLE = 0x00U,
  COMMAND_CALCULATE_CRC = 0x03U,
  COMMAND_TRANSCEIVE = 0x0CU,
  COMMAND_SOFT_RESET = 0x0FU
};

typedef struct
{
  uint8_t registers[64];
  uint8_t tx_fifo[64];
  size_t tx_length;
  uint8_t rx_fifo[64];
  size_t rx_length;
  size_t rx_index;
  uint8_t uid[RC522_UID_SIZE];
  bool card_present;
  bool bad_uid_checksum;
  bool force_timeout;
  bool reset_levels[8];
  size_t reset_level_count;
  uint32_t delay_total_us;
  uint32_t transceive_count;
} FakeRc522;

static uint16_t crc_a(const uint8_t *data, size_t length)
{
  uint16_t crc = 0x6363U;
  size_t index;

  for (index = 0U; index < length; index++)
  {
    uint8_t value = (uint8_t)(data[index] ^ (uint8_t)(crc & 0x00FFU));
    value = (uint8_t)(value ^ (uint8_t)(value << 4U));
    crc = (uint16_t)((crc >> 8U) ^ ((uint16_t)value << 8U) ^
                     ((uint16_t)value << 3U) ^ ((uint16_t)value >> 4U));
  }
  return crc;
}

static void fake_set_response(FakeRc522 *fake, const uint8_t *data,
                              size_t length, uint8_t last_bits)
{
  memcpy(fake->rx_fifo, data, length);
  fake->rx_length = length;
  fake->rx_index = 0U;
  fake->registers[REG_CONTROL] = last_bits;
  fake->registers[REG_COM_IRQ] = 0x30U;
  fake->registers[REG_ERROR] = 0U;
}

static void fake_begin_transceive(FakeRc522 *fake)
{
  uint8_t response[5];

  fake->transceive_count++;
  fake->rx_length = 0U;
  fake->rx_index = 0U;
  fake->registers[REG_COM_IRQ] = 0U;

  if (fake->force_timeout)
  {
    return;
  }

  if (!fake->card_present ||
      ((fake->tx_length >= 1U) && (fake->tx_fifo[0] == 0x50U)))
  {
    fake->registers[REG_COM_IRQ] = 0x01U;
    return;
  }

  if ((fake->tx_length == 1U) && (fake->tx_fifo[0] == 0x52U))
  {
    const uint8_t tag_type[2] = {0x04U, 0x00U};
    fake_set_response(fake, tag_type, sizeof(tag_type), 0U);
    return;
  }

  if ((fake->tx_length == 2U) && (fake->tx_fifo[0] == 0x93U) &&
      (fake->tx_fifo[1] == 0x20U))
  {
    response[0] = fake->uid[0];
    response[1] = fake->uid[1];
    response[2] = fake->uid[2];
    response[3] = fake->uid[3];
    response[4] = (uint8_t)(response[0] ^ response[1] ^ response[2] ^
                            response[3]);
    if (fake->bad_uid_checksum)
    {
      response[4] ^= 0x01U;
    }
    fake_set_response(fake, response, sizeof(response), 0U);
    return;
  }

  if ((fake->tx_length == 9U) && (fake->tx_fifo[0] == 0x93U) &&
      (fake->tx_fifo[1] == 0x70U))
  {
    const uint8_t select_response[3] = {0x08U, 0xB6U, 0xDDU};
    fake_set_response(fake, select_response, sizeof(select_response), 0U);
    return;
  }

  fake->registers[REG_ERROR] = 0x01U;
  fake->registers[REG_COM_IRQ] = 0x30U;
}

static uint8_t fake_read_register(void *context, uint8_t address)
{
  FakeRc522 *fake = (FakeRc522 *)context;

  if (address == REG_FIFO_DATA)
  {
    if (fake->rx_index < fake->rx_length)
    {
      return fake->rx_fifo[fake->rx_index++];
    }
    return 0U;
  }

  if (address == REG_FIFO_LEVEL)
  {
    return (uint8_t)(fake->rx_length - fake->rx_index);
  }

  if ((address == REG_COM_IRQ) && fake->force_timeout)
  {
    return 0U;
  }

  return fake->registers[address & 0x3FU];
}

static void fake_write_register(void *context, uint8_t address, uint8_t value)
{
  FakeRc522 *fake = (FakeRc522 *)context;
  const uint8_t register_index = (uint8_t)(address & 0x3FU);

  if (address == REG_FIFO_DATA)
  {
    if (fake->tx_length < sizeof(fake->tx_fifo))
    {
      fake->tx_fifo[fake->tx_length++] = value;
    }
    return;
  }

  if ((address == REG_FIFO_LEVEL) && ((value & 0x80U) != 0U))
  {
    fake->tx_length = 0U;
    fake->rx_length = 0U;
    fake->rx_index = 0U;
    fake->registers[REG_FIFO_LEVEL] = 0U;
    return;
  }

  if (address == REG_COMMAND)
  {
    if (value == COMMAND_SOFT_RESET)
    {
      fake->registers[REG_COMMAND] = 0U;
      return;
    }
    if (value == COMMAND_CALCULATE_CRC)
    {
      const uint16_t crc = crc_a(fake->tx_fifo, fake->tx_length);
      fake->registers[REG_CRC_RESULT_LOW] = (uint8_t)(crc & 0x00FFU);
      fake->registers[REG_CRC_RESULT_HIGH] = (uint8_t)(crc >> 8U);
      fake->registers[REG_DIV_IRQ] |= 0x04U;
    }
    else if (value == COMMAND_TRANSCEIVE)
    {
      fake_begin_transceive(fake);
    }
    else if (value == COMMAND_IDLE)
    {
      fake->registers[REG_COMMAND] = COMMAND_IDLE;
    }
  }

  fake->registers[register_index] = value;
}

static void fake_write_reset(void *context, bool high)
{
  FakeRc522 *fake = (FakeRc522 *)context;

  if (fake->reset_level_count <
      (sizeof(fake->reset_levels) / sizeof(fake->reset_levels[0])))
  {
    fake->reset_levels[fake->reset_level_count++] = high;
  }
}

static void fake_delay_us(void *context, uint32_t microseconds)
{
  ((FakeRc522 *)context)->delay_total_us += microseconds;
}

static Rc522Io make_io(FakeRc522 *fake)
{
  const Rc522Io io = {
    fake_read_register,
    fake_write_register,
    fake_write_reset,
    fake_delay_us,
    fake
  };

  return io;
}

static void init_fake(FakeRc522 *fake)
{
  memset(fake, 0, sizeof(*fake));
  fake->uid[0] = 0x92U;
  fake->uid[1] = 0xE6U;
  fake->uid[2] = 0xC2U;
  fake->uid[3] = 0x51U;
  fake->card_present = true;
  fake->registers[REG_VERSION] = 0x92U;
}

static int test_reset_and_configuration(void)
{
  Rc522 device;
  FakeRc522 fake;
  Rc522Io io;

  init_fake(&fake);
  io = make_io(&fake);
  TEST_CHECK(RC522_Init(&device, &io));
  TEST_CHECK(RC522_ResetAndConfigure(&device) == RC522_STATUS_OK);
  TEST_CHECK(fake.reset_level_count == 3U);
  TEST_CHECK(fake.reset_levels[0]);
  TEST_CHECK(!fake.reset_levels[1]);
  TEST_CHECK(fake.reset_levels[2]);
  TEST_CHECK(fake.delay_total_us == 5U);
  TEST_CHECK(fake.registers[0x11U] == 0x3DU);
  TEST_CHECK(fake.registers[0x17U] == 0x86U);
  TEST_CHECK(fake.registers[0x26U] == 0x7FU);
  TEST_CHECK((fake.registers[REG_TX_CONTROL] & 0x03U) == 0x03U);
  TEST_CHECK(RC522_ReadVersion(&device) == 0x92U);
  return 0;
}

static int test_read_uid_flow(void)
{
  Rc522 device;
  FakeRc522 fake;
  Rc522Io io;
  uint8_t uid[RC522_UID_SIZE] = {0U};

  init_fake(&fake);
  io = make_io(&fake);
  TEST_CHECK(RC522_Init(&device, &io));
  TEST_CHECK(RC522_ReadUid(&device, uid) == RC522_STATUS_OK);
  TEST_CHECK(memcmp(uid, fake.uid, sizeof(uid)) == 0);
  TEST_CHECK(fake.transceive_count == 3U);
  TEST_CHECK(RC522_Halt(&device) == RC522_STATUS_OK);
  TEST_CHECK(fake.transceive_count == 4U);
  return 0;
}

static int test_no_card_is_reported(void)
{
  Rc522 device;
  FakeRc522 fake;
  Rc522Io io;
  uint8_t uid[RC522_UID_SIZE];

  init_fake(&fake);
  fake.card_present = false;
  io = make_io(&fake);
  TEST_CHECK(RC522_Init(&device, &io));
  TEST_CHECK(RC522_ReadUid(&device, uid) == RC522_STATUS_NO_CARD);
  TEST_CHECK(fake.transceive_count == 1U);
  return 0;
}

static int test_bad_uid_checksum_is_rejected(void)
{
  Rc522 device;
  FakeRc522 fake;
  Rc522Io io;
  uint8_t uid[RC522_UID_SIZE];

  init_fake(&fake);
  fake.bad_uid_checksum = true;
  io = make_io(&fake);
  TEST_CHECK(RC522_Init(&device, &io));
  TEST_CHECK(RC522_ReadUid(&device, uid) == RC522_STATUS_PROTOCOL_ERROR);
  TEST_CHECK(fake.transceive_count == 2U);
  return 0;
}

static int test_timeout_is_bounded(void)
{
  Rc522 device;
  FakeRc522 fake;
  Rc522Io io;
  uint8_t tag_type[RC522_TAG_TYPE_SIZE];

  init_fake(&fake);
  fake.force_timeout = true;
  io = make_io(&fake);
  TEST_CHECK(RC522_Init(&device, &io));
  device.communication_timeout_loops = 3U;
  TEST_CHECK(RC522_Request(&device, 0x52U, tag_type) ==
             RC522_STATUS_TIMEOUT);
  return 0;
}

static int test_invalid_arguments(void)
{
  Rc522 device = {0};
  FakeRc522 fake;
  Rc522Io io;
  uint8_t uid[RC522_UID_SIZE];

  init_fake(&fake);
  io = make_io(&fake);
  TEST_CHECK(!RC522_Init(NULL, &io));
  io.read_register = NULL;
  TEST_CHECK(!RC522_Init(&device, &io));
  TEST_CHECK(RC522_ReadUid(&device, uid) == RC522_STATUS_INVALID_ARGUMENT);
  TEST_CHECK(RC522_ReadVersion(NULL) == 0U);
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
    {"reset_and_configuration", test_reset_and_configuration},
    {"read_uid_flow", test_read_uid_flow},
    {"no_card_is_reported", test_no_card_is_reported},
    {"bad_uid_checksum_is_rejected", test_bad_uid_checksum_is_rejected},
    {"timeout_is_bounded", test_timeout_is_bounded},
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

  printf("All RC522 protocol tests passed.\n");
  return 0;
}
