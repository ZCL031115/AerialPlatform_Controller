#include "pcu_app.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(condition)                                                        \
  do                                                                            \
  {                                                                             \
    if (!(condition))                                                           \
    {                                                                           \
      printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition);            \
      return false;                                                             \
    }                                                                           \
  } while (0)

typedef struct
{
  bool start_controller_ok;
  bool start_lora_ok;
  bool transmit_controller_ok;
  bool transmit_lora_ok;
  uint8_t controller_tx[64];
  uint8_t lora_tx[64];
  size_t controller_tx_count;
  size_t lora_tx_count;
} FakeIo;

static bool fake_start_controller(void *context, uint8_t *destination)
{
  (void)destination;
  return ((FakeIo *)context)->start_controller_ok;
}

static bool fake_start_lora(void *context, uint8_t *destination)
{
  (void)destination;
  return ((FakeIo *)context)->start_lora_ok;
}

static bool fake_transmit_controller(void *context, uint8_t value)
{
  FakeIo *io = (FakeIo *)context;

  if (!io->transmit_controller_ok)
  {
    return false;
  }
  io->controller_tx[io->controller_tx_count++] = value;
  return true;
}

static bool fake_transmit_lora(void *context, uint8_t value)
{
  FakeIo *io = (FakeIo *)context;

  if (!io->transmit_lora_ok)
  {
    return false;
  }
  io->lora_tx[io->lora_tx_count++] = value;
  return true;
}

static bool init_app(PcuApp *app, FakeIo *fake, bool estop_active)
{
  const PcuAppIo io = {fake_start_controller, fake_start_lora,
                       fake_transmit_controller, fake_transmit_lora, fake};

  fake->start_controller_ok = true;
  fake->start_lora_ok = true;
  fake->transmit_controller_ok = true;
  fake->transmit_lora_ok = true;
  fake->controller_tx_count = 0U;
  fake->lora_tx_count = 0U;
  return PCU_App_Init(app, &io, estop_active) &&
         PCU_App_StartControllerReceive(app) &&
         PCU_App_StartLoraReceive(app);
}

static bool receive_controller(PcuApp *app, uint8_t value)
{
  app->controller_rx_byte = value;
  return PCU_App_OnControllerReceiveComplete(app);
}

static bool receive_lora(PcuApp *app, uint8_t value)
{
  app->lora_rx_byte = value;
  return PCU_App_OnLoraReceiveComplete(app);
}

static bool test_bidirectional_forwarding(void)
{
  PcuApp app;
  FakeIo fake;

  CHECK(init_app(&app, &fake, false));
  CHECK(receive_controller(&app, 0x55U));
  CHECK(receive_lora(&app, 0x31U));
  PCU_App_Process(&app, 0U);
  CHECK(fake.lora_tx_count == 2U);
  CHECK(fake.lora_tx[0] == PCU_COMMAND_ESTOP);
  CHECK(fake.lora_tx[1] == 0x55U);
  CHECK(fake.controller_tx_count == 1U);
  CHECK(fake.controller_tx[0] == 0x31U);
  return true;
}

static bool test_controller_estop_is_forwarded(void)
{
  PcuApp app;
  FakeIo fake;

  CHECK(init_app(&app, &fake, false));
  CHECK(receive_controller(&app, PCU_COMMAND_ESTOP));
  PCU_App_Process(&app, 0U);
  CHECK(fake.lora_tx_count == 2U);
  CHECK(fake.lora_tx[1] == PCU_COMMAND_ESTOP);
  return true;
}

static bool test_restore_requires_inactive_input(void)
{
  PcuApp app;
  FakeIo fake;

  CHECK(init_app(&app, &fake, true));
  CHECK(!receive_controller(&app, PCU_COMMAND_RESTORE));
  CHECK(app.estop.rejected_reset_count == 1U);
  CHECK(PCU_App_UpdateEstopInput(&app, false));
  CHECK(receive_controller(&app, PCU_COMMAND_RESTORE));
  PCU_App_Process(&app, 0U);
  CHECK(fake.lora_tx_count == 2U);
  CHECK(fake.lora_tx[0] == PCU_COMMAND_ESTOP);
  CHECK(fake.lora_tx[1] == PCU_COMMAND_RESTORE);
  CHECK(!PCU_Estop_IsLatched(&app.estop));
  return true;
}

static bool test_queued_restore_is_cancelled_by_new_estop(void)
{
  PcuApp app;
  FakeIo fake;

  CHECK(init_app(&app, &fake, false));
  CHECK(receive_controller(&app, PCU_COMMAND_RESTORE));
  CHECK(PCU_App_UpdateEstopInput(&app, true));
  PCU_App_Process(&app, 0U);
  CHECK(fake.lora_tx_count == 1U);
  CHECK(fake.lora_tx[0] == PCU_COMMAND_ESTOP);
  CHECK(app.estop.rejected_reset_count == 1U);
  CHECK(PCU_Estop_IsLatched(&app.estop));
  CHECK(app.controller_to_lora_forwarded_count == 0U);
  return true;
}

static bool test_local_estop_repeats(void)
{
  PcuApp app;
  FakeIo fake;

  CHECK(init_app(&app, &fake, true));
  PCU_App_Process(&app, 10U);
  PCU_App_Process(&app, 109U);
  PCU_App_Process(&app, 110U);
  CHECK(fake.lora_tx_count == 2U);
  CHECK(fake.lora_tx[0] == PCU_COMMAND_ESTOP);
  CHECK(fake.lora_tx[1] == PCU_COMMAND_ESTOP);
  return true;
}

static bool test_failed_transmit_retries_queue(void)
{
  PcuApp app;
  FakeIo fake;

  CHECK(init_app(&app, &fake, false));
  CHECK(receive_controller(&app, 0x44U));
  fake.transmit_lora_ok = false;
  PCU_App_Process(&app, 0U);
  CHECK(app.lora_transmit_failure_count == 1U);
  fake.transmit_lora_ok = true;
  PCU_App_Process(&app, 1U);
  CHECK(fake.lora_tx_count == 1U);
  CHECK(fake.lora_tx[0] == 0x44U);
  return true;
}

static bool test_receive_errors_are_observable(void)
{
  PcuApp app;
  FakeIo fake;

  CHECK(init_app(&app, &fake, false));
  CHECK(PCU_App_OnControllerReceiveError(&app));
  CHECK(PCU_App_OnLoraReceiveError(&app));
  CHECK(app.controller_receive_error_count == 1U);
  CHECK(app.lora_receive_error_count == 1U);
  return true;
}

typedef bool (*TestFunction)(void);

typedef struct
{
  const char *name;
  TestFunction function;
} TestCase;

int main(void)
{
  static const TestCase tests[] = {
      {"bidirectional_forwarding", test_bidirectional_forwarding},
      {"controller_estop_is_forwarded",
       test_controller_estop_is_forwarded},
      {"restore_requires_inactive_input",
       test_restore_requires_inactive_input},
      {"queued_restore_is_cancelled_by_new_estop",
       test_queued_restore_is_cancelled_by_new_estop},
      {"local_estop_repeats", test_local_estop_repeats},
      {"failed_transmit_retries_queue", test_failed_transmit_retries_queue},
      {"receive_errors_are_observable",
       test_receive_errors_are_observable}};
  size_t index;

  for (index = 0U; index < (sizeof(tests) / sizeof(tests[0])); index++)
  {
    if (!tests[index].function())
    {
      return 1;
    }
    printf("PASS: %s\n", tests[index].name);
  }

  printf("All PCU app tests passed.\n");
  return 0;
}
