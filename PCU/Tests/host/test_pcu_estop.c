#include "pcu_estop.h"

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

static bool test_safe_start_requires_reset(void)
{
  PcuEstop estop;
  uint8_t command = 0U;

  PCU_Estop_Init(&estop, false);
  CHECK(PCU_Estop_IsLatched(&estop));
  CHECK(PCU_Estop_Process(&estop, 0U, &command));
  CHECK(command == PCU_COMMAND_ESTOP);
  CHECK(PCU_Estop_RequestReset(&estop, &command));
  CHECK(command == PCU_COMMAND_RESTORE);
  CHECK(!PCU_Estop_IsLatched(&estop));
  CHECK(!PCU_Estop_Process(&estop, 100U, &command));
  return true;
}

static bool test_active_input_repeats_estop(void)
{
  PcuEstop estop;
  uint8_t command = 0U;

  PCU_Estop_Init(&estop, true);
  CHECK(PCU_Estop_Process(&estop, 10U, &command));
  CHECK(command == PCU_COMMAND_ESTOP);
  CHECK(!PCU_Estop_Process(&estop, 109U, &command));
  CHECK(PCU_Estop_Process(&estop, 110U, &command));
  CHECK(estop.estop_command_count == 2U);
  return true;
}

static bool test_release_remains_latched(void)
{
  PcuEstop estop;
  uint8_t command = 0U;

  PCU_Estop_Init(&estop, true);
  CHECK(PCU_Estop_Process(&estop, 0U, &command));
  CHECK(!PCU_Estop_UpdateInput(&estop, false));
  CHECK(PCU_Estop_IsLatched(&estop));
  CHECK(PCU_Estop_Process(&estop, 100U, &command));
  CHECK(command == PCU_COMMAND_ESTOP);
  return true;
}

static bool test_reset_rejected_while_active(void)
{
  PcuEstop estop;
  uint8_t command = 0U;

  PCU_Estop_Init(&estop, true);
  CHECK(!PCU_Estop_RequestReset(&estop, &command));
  CHECK(PCU_Estop_IsLatched(&estop));
  CHECK(estop.rejected_reset_count == 1U);
  CHECK(estop.reset_command_count == 0U);
  return true;
}

static bool test_new_estop_is_immediate(void)
{
  PcuEstop estop;
  uint8_t command = 0U;

  PCU_Estop_Init(&estop, false);
  CHECK(PCU_Estop_RequestReset(&estop, &command));
  CHECK(PCU_Estop_UpdateInput(&estop, true));
  CHECK(PCU_Estop_Process(&estop, 1U, &command));
  CHECK(command == PCU_COMMAND_ESTOP);
  CHECK(PCU_Estop_IsLatched(&estop));
  return true;
}

static bool test_tick_wraparound(void)
{
  PcuEstop estop;
  uint8_t command = 0U;

  PCU_Estop_Init(&estop, true);
  CHECK(PCU_Estop_Process(&estop, UINT32_MAX - 50U, &command));
  CHECK(!PCU_Estop_Process(&estop, 48U, &command));
  CHECK(PCU_Estop_Process(&estop, 49U, &command));
  return true;
}

static bool test_invalid_arguments(void)
{
  PcuEstop estop;
  uint8_t command = 0U;

  PCU_Estop_Init(NULL, false);
  PCU_Estop_Init(&estop, false);
  CHECK(!PCU_Estop_UpdateInput(NULL, true));
  CHECK(!PCU_Estop_Process(NULL, 0U, &command));
  CHECK(!PCU_Estop_Process(&estop, 0U, NULL));
  CHECK(!PCU_Estop_RequestReset(NULL, &command));
  CHECK(!PCU_Estop_RequestReset(&estop, NULL));
  CHECK(!PCU_Estop_IsLatched(NULL));
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
    {"safe_start_requires_reset", test_safe_start_requires_reset},
    {"active_input_repeats_estop", test_active_input_repeats_estop},
    {"release_remains_latched", test_release_remains_latched},
    {"reset_rejected_while_active", test_reset_rejected_while_active},
    {"new_estop_is_immediate", test_new_estop_is_immediate},
    {"tick_wraparound", test_tick_wraparound},
    {"invalid_arguments", test_invalid_arguments}
  };
  size_t index;

  for (index = 0U; index < (sizeof(tests) / sizeof(tests[0])); index++)
  {
    if (!tests[index].function())
    {
      return 1;
    }
    printf("PASS: %s\n", tests[index].name);
  }

  printf("All PCU emergency-stop tests passed.\n");
  return 0;
}
