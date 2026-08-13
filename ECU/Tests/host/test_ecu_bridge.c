#include "ecu_app.h"

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
  uint8_t *lora_rx;
  uint8_t *external_rx;
  bool lora_tx_result;
  bool external_tx_result;
  uint8_t last_lora_tx;
  uint8_t last_external_tx;
  uint32_t lora_tx_count;
  uint32_t external_tx_count;
} FakeBridgeIo;

static bool start_lora_receive(void *context, uint8_t *destination)
{
  ((FakeBridgeIo *)context)->lora_rx = destination;
  return true;
}

static bool start_external_receive(void *context, uint8_t *destination)
{
  ((FakeBridgeIo *)context)->external_rx = destination;
  return true;
}

static bool transmit_lora(void *context, uint8_t value)
{
  FakeBridgeIo *fake = (FakeBridgeIo *)context;

  fake->lora_tx_count++;
  fake->last_lora_tx = value;
  return fake->lora_tx_result;
}

static bool transmit_external(void *context, uint8_t value)
{
  FakeBridgeIo *fake = (FakeBridgeIo *)context;

  fake->external_tx_count++;
  fake->last_external_tx = value;
  return fake->external_tx_result;
}

static void write_relay(void *context, bool energized)
{
  (void)context;
  (void)energized;
}

static void write_lora_mode(void *context, bool md0, bool md1)
{
  (void)context;
  (void)md0;
  (void)md1;
}

static EcuAppIo make_io(FakeBridgeIo *fake)
{
  EcuAppIo io;

  io.start_receive = start_lora_receive;
  io.start_external_receive = start_external_receive;
  io.transmit_lora = transmit_lora;
  io.transmit_external = transmit_external;
  io.write_relay = write_relay;
  io.write_lora_mode = write_lora_mode;
  io.context = fake;
  return io;
}

static int test_non_command_lora_byte_routes_to_external(void)
{
  EcuApp app;
  FakeBridgeIo fake = {0};
  EcuAppIo io = make_io(&fake);

  fake.lora_tx_result = true;
  fake.external_tx_result = true;
  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(ECU_App_StartReceive(&app));
  *fake.lora_rx = 0x55U;
  TEST_CHECK(!ECU_App_OnReceiveComplete(&app));
  TEST_CHECK(fake.external_tx_count == 0U);

  ECU_App_Process(&app);
  TEST_CHECK(fake.external_tx_count == 1U);
  TEST_CHECK(fake.last_external_tx == 0x55U);
  TEST_CHECK(app.external_forwarded_byte_count == 1U);
  return 0;
}

static int test_safety_command_is_applied_and_forwarded(void)
{
  EcuApp app;
  FakeBridgeIo fake = {0};
  EcuAppIo io = make_io(&fake);

  fake.lora_tx_result = true;
  fake.external_tx_result = true;
  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(ECU_App_StartReceive(&app));
  *fake.lora_rx = ECU_COMMAND_BYTE_EMERGENCY_STOP;
  TEST_CHECK(ECU_App_OnReceiveComplete(&app));
  TEST_CHECK(app.control.state == ECU_CONTROL_EMERGENCY_STOPPED);
  ECU_App_Process(&app);
  TEST_CHECK(fake.external_tx_count == 1U);
  TEST_CHECK(fake.last_external_tx == ECU_COMMAND_BYTE_EMERGENCY_STOP);
  TEST_CHECK(app.external_forwarded_byte_count == 1U);
  return 0;
}

static int test_external_byte_routes_to_lora(void)
{
  EcuApp app;
  FakeBridgeIo fake = {0};
  EcuAppIo io = make_io(&fake);

  fake.lora_tx_result = true;
  fake.external_tx_result = true;
  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(ECU_App_StartExternalReceive(&app));
  *fake.external_rx = 0xA5U;
  TEST_CHECK(ECU_App_OnExternalReceiveComplete(&app));
  TEST_CHECK(fake.lora_tx_count == 0U);

  ECU_App_Process(&app);
  TEST_CHECK(fake.lora_tx_count == 1U);
  TEST_CHECK(fake.last_lora_tx == 0xA5U);
  TEST_CHECK(app.lora_forwarded_byte_count == 1U);
  return 0;
}

static int test_failed_transmit_retries(void)
{
  EcuApp app;
  FakeBridgeIo fake = {0};
  EcuAppIo io = make_io(&fake);

  fake.external_tx_result = false;
  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(ECU_App_StartReceive(&app));
  *fake.lora_rx = 0x44U;
  TEST_CHECK(!ECU_App_OnReceiveComplete(&app));
  ECU_App_Process(&app);
  TEST_CHECK(app.external_transmit_failure_count == 1U);
  TEST_CHECK(app.external_forwarded_byte_count == 0U);

  fake.external_tx_result = true;
  ECU_App_Process(&app);
  TEST_CHECK(fake.external_tx_count == 2U);
  TEST_CHECK(fake.last_external_tx == 0x44U);
  TEST_CHECK(app.external_forwarded_byte_count == 1U);
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
    {"non_command_lora_byte_routes_to_external",
     test_non_command_lora_byte_routes_to_external},
    {"safety_command_is_applied_and_forwarded",
     test_safety_command_is_applied_and_forwarded},
    {"external_byte_routes_to_lora", test_external_byte_routes_to_lora},
    {"failed_transmit_retries", test_failed_transmit_retries}
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

  printf("All ECU bridge tests passed.\n");
  return 0;
}
