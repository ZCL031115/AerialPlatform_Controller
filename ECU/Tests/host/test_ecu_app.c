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
  bool start_receive_result;
  uint8_t *receive_destination;
  uint32_t start_receive_call_count;
  bool relay_energized;
  uint32_t relay_write_count;
  bool md0;
  bool md1;
  uint32_t mode_write_count;
  bool external_start_receive_result;
  uint8_t *external_receive_destination;
  uint32_t external_start_receive_call_count;
  bool lora_transmit_result;
  bool external_transmit_result;
  uint32_t lora_transmit_call_count;
  uint32_t external_transmit_call_count;
  uint8_t last_lora_tx;
  uint8_t last_external_tx;
} FakeIo;

static bool fake_start_receive(void *context, uint8_t *destination)
{
  FakeIo *io = (FakeIo *)context;

  io->start_receive_call_count++;
  io->receive_destination = destination;
  return io->start_receive_result;
}

static bool fake_start_external_receive(void *context, uint8_t *destination)
{
  FakeIo *io = (FakeIo *)context;

  io->external_start_receive_call_count++;
  io->external_receive_destination = destination;
  return io->external_start_receive_result;
}

static bool fake_transmit_lora(void *context, uint8_t value)
{
  FakeIo *io = (FakeIo *)context;

  io->lora_transmit_call_count++;
  io->last_lora_tx = value;
  return io->lora_transmit_result;
}

static bool fake_transmit_external(void *context, uint8_t value)
{
  FakeIo *io = (FakeIo *)context;

  io->external_transmit_call_count++;
  io->last_external_tx = value;
  return io->external_transmit_result;
}

static void fake_write_relay(void *context, bool energized)
{
  FakeIo *io = (FakeIo *)context;

  io->relay_write_count++;
  io->relay_energized = energized;
}

static void fake_write_lora_mode(void *context, bool md0, bool md1)
{
  FakeIo *io = (FakeIo *)context;

  io->mode_write_count++;
  io->md0 = md0;
  io->md1 = md1;
}

static EcuAppIo make_app_io(FakeIo *fake)
{
  EcuAppIo io;

  io.start_receive = fake_start_receive;
  io.start_external_receive = fake_start_external_receive;
  io.transmit_lora = fake_transmit_lora;
  io.transmit_external = fake_transmit_external;
  io.write_relay = fake_write_relay;
  io.write_lora_mode = fake_write_lora_mode;
  io.context = fake;

  return io;
}

static int test_init_is_safe_and_normal_mode(void)
{
  EcuApp app;
  FakeIo fake = {true, NULL, 0U, true, 0U, true, true, 0U};
  EcuAppIo io = make_app_io(&fake);

  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(!fake.relay_energized);
  TEST_CHECK(fake.relay_write_count == 1U);
  TEST_CHECK(!fake.md0);
  TEST_CHECK(!fake.md1);
  TEST_CHECK(fake.mode_write_count == 1U);
  TEST_CHECK(app.lora_mode == ECU_LORA_MODE_NORMAL);
  TEST_CHECK(!app.receive_pending);
  return 0;
}

static int test_receive_commands_and_restart(void)
{
  EcuApp app;
  FakeIo fake = {true, NULL, 0U, false, 0U, false, false, 0U};
  EcuAppIo io = make_app_io(&fake);

  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(ECU_App_StartReceive(&app));
  TEST_CHECK(fake.receive_destination != NULL);
  TEST_CHECK(fake.start_receive_call_count == 1U);

  *fake.receive_destination = ECU_COMMAND_BYTE_RESTORE;
  TEST_CHECK(ECU_App_OnReceiveComplete(&app));
  TEST_CHECK(fake.relay_energized);
  TEST_CHECK(fake.start_receive_call_count == 2U);
  TEST_CHECK(app.receive_pending);

  *fake.receive_destination = ECU_COMMAND_BYTE_EMERGENCY_STOP;
  TEST_CHECK(ECU_App_OnReceiveComplete(&app));
  TEST_CHECK(!fake.relay_energized);
  TEST_CHECK(fake.start_receive_call_count == 3U);

  *fake.receive_destination = ECU_COMMAND_BYTE_DISCONNECT;
  TEST_CHECK(ECU_App_OnReceiveComplete(&app));
  TEST_CHECK(!fake.relay_energized);
  TEST_CHECK(app.control.state == ECU_CONTROL_DISCONNECTED);
  return 0;
}

static int test_unknown_byte_keeps_safe_state(void)
{
  EcuApp app;
  FakeIo fake = {true, NULL, 0U, false, 0U, false, false, 0U};
  EcuAppIo io = make_app_io(&fake);

  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(ECU_App_StartReceive(&app));
  *fake.receive_destination = 0x55U;
  TEST_CHECK(!ECU_App_OnReceiveComplete(&app));
  TEST_CHECK(!fake.relay_energized);
  TEST_CHECK(app.control.state == ECU_CONTROL_SAFE_STARTUP);
  TEST_CHECK(app.control.rejected_byte_count == 1U);
  TEST_CHECK(app.receive_pending);
  return 0;
}

static int test_lora_config_mode(void)
{
  EcuApp app;
  FakeIo fake = {true, NULL, 0U, false, 0U, false, false, 0U};
  EcuAppIo io = make_app_io(&fake);

  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(ECU_App_SetLoraMode(&app, ECU_LORA_MODE_CONFIG));
  TEST_CHECK(fake.md0);
  TEST_CHECK(fake.md1);
  TEST_CHECK(app.lora_mode == ECU_LORA_MODE_CONFIG);
  TEST_CHECK(!ECU_App_SetLoraMode(&app, (EcuLoraMode)99));
  TEST_CHECK(app.lora_mode == ECU_LORA_MODE_CONFIG);
  return 0;
}

static int test_receive_failures_are_observable(void)
{
  EcuApp app;
  FakeIo fake = {false, NULL, 0U, false, 0U, false, false, 0U};
  EcuAppIo io = make_app_io(&fake);

  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(!ECU_App_StartReceive(&app));
  TEST_CHECK(app.receive_start_failure_count == 1U);
  TEST_CHECK(!app.receive_pending);

  fake.start_receive_result = true;
  TEST_CHECK(ECU_App_OnReceiveError(&app));
  TEST_CHECK(app.receive_error_count == 1U);
  TEST_CHECK(app.receive_pending);
  return 0;
}

static int test_unexpected_callback_is_rejected(void)
{
  EcuApp app;
  FakeIo fake = {true, NULL, 0U, false, 0U, false, false, 0U};
  EcuAppIo io = make_app_io(&fake);

  TEST_CHECK(ECU_App_Init(&app, &io));
  TEST_CHECK(!ECU_App_OnReceiveComplete(&app));
  TEST_CHECK(app.unexpected_receive_callback_count == 1U);
  TEST_CHECK(fake.relay_write_count == 1U);
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
    {"init_is_safe_and_normal_mode", test_init_is_safe_and_normal_mode},
    {"receive_commands_and_restart", test_receive_commands_and_restart},
    {"unknown_byte_keeps_safe_state", test_unknown_byte_keeps_safe_state},
    {"lora_config_mode", test_lora_config_mode},
    {"receive_failures_are_observable", test_receive_failures_are_observable},
    {"unexpected_callback_is_rejected", test_unexpected_callback_is_rejected}
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

  printf("All ECU app tests passed.\n");
  return 0;
}
