#include "ecu_control.h"

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

static int test_command_decode(void)
{
  TEST_CHECK(ECU_Command_Decode(0x86U) == ECU_COMMAND_DISCONNECT);
  TEST_CHECK(ECU_Command_Decode(0x87U) == ECU_COMMAND_EMERGENCY_STOP);
  TEST_CHECK(ECU_Command_Decode(0x26U) == ECU_COMMAND_RESTORE);
  TEST_CHECK(ECU_Command_Decode(0x00U) == ECU_COMMAND_UNKNOWN);
  TEST_CHECK(ECU_Command_Decode(0xFFU) == ECU_COMMAND_UNKNOWN);
  return 0;
}

static int test_safe_startup(void)
{
  EcuControl control;

  ECU_Control_Init(&control);

  TEST_CHECK(control.state == ECU_CONTROL_SAFE_STARTUP);
  TEST_CHECK(control.last_command == ECU_COMMAND_UNKNOWN);
  TEST_CHECK(control.accepted_command_count == 0U);
  TEST_CHECK(control.rejected_byte_count == 0U);
  TEST_CHECK(!ECU_Control_ShouldEnergizeRelay(&control));
  return 0;
}

static int test_restore_enables_relay(void)
{
  EcuControl control;

  ECU_Control_Init(&control);
  TEST_CHECK(ECU_Control_HandleByte(&control, 0x26U));
  TEST_CHECK(control.state == ECU_CONTROL_RUNNING);
  TEST_CHECK(control.last_command == ECU_COMMAND_RESTORE);
  TEST_CHECK(control.accepted_command_count == 1U);
  TEST_CHECK(ECU_Control_ShouldEnergizeRelay(&control));
  return 0;
}

static int test_emergency_stop_and_restore(void)
{
  EcuControl control;

  ECU_Control_Init(&control);
  TEST_CHECK(ECU_Control_HandleByte(&control, 0x26U));
  TEST_CHECK(ECU_Control_HandleByte(&control, 0x87U));
  TEST_CHECK(control.state == ECU_CONTROL_EMERGENCY_STOPPED);
  TEST_CHECK(!ECU_Control_ShouldEnergizeRelay(&control));

  TEST_CHECK(!ECU_Control_HandleByte(&control, 0x55U));
  TEST_CHECK(control.state == ECU_CONTROL_EMERGENCY_STOPPED);
  TEST_CHECK(control.rejected_byte_count == 1U);
  TEST_CHECK(!ECU_Control_ShouldEnergizeRelay(&control));

  TEST_CHECK(ECU_Control_HandleByte(&control, 0x26U));
  TEST_CHECK(control.state == ECU_CONTROL_RUNNING);
  TEST_CHECK(ECU_Control_ShouldEnergizeRelay(&control));
  TEST_CHECK(control.accepted_command_count == 3U);
  return 0;
}

static int test_disconnect_is_fail_safe(void)
{
  EcuControl control;

  ECU_Control_Init(&control);
  TEST_CHECK(ECU_Control_HandleByte(&control, 0x26U));
  TEST_CHECK(ECU_Control_HandleByte(&control, 0x86U));
  TEST_CHECK(control.state == ECU_CONTROL_DISCONNECTED);
  TEST_CHECK(control.last_command == ECU_COMMAND_DISCONNECT);
  TEST_CHECK(!ECU_Control_ShouldEnergizeRelay(&control));
  return 0;
}

static int test_null_arguments(void)
{
  ECU_Control_Init(NULL);
  TEST_CHECK(!ECU_Control_ApplyCommand(NULL, ECU_COMMAND_RESTORE));
  TEST_CHECK(!ECU_Control_HandleByte(NULL, 0x26U));
  TEST_CHECK(!ECU_Control_ShouldEnergizeRelay(NULL));
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
    {"command_decode", test_command_decode},
    {"safe_startup", test_safe_startup},
    {"restore_enables_relay", test_restore_enables_relay},
    {"emergency_stop_and_restore", test_emergency_stop_and_restore},
    {"disconnect_is_fail_safe", test_disconnect_is_fail_safe},
    {"null_arguments", test_null_arguments}
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

  printf("All ECU control tests passed.\n");
  return 0;
}
