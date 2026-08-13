#include "pcu_app_hal.h"

#include "main.h"
#include "usart.h"

#include <stddef.h>

enum
{
  PCU_APP_UART_TIMEOUT_MS = 2U,
  PCU_APP_RECEIVE_RETRY_INTERVAL_MS = 1000U
};

static PcuApp pcu_app;
static bool pcu_app_initialized;
static uint32_t last_controller_receive_retry_tick;
static uint32_t last_lora_receive_retry_tick;

static bool PCU_App_HAL_StartControllerReceive(void *context,
                                                uint8_t *destination)
{
  (void)context;
  return HAL_UART_Receive_IT(&huart2, destination, 1U) == HAL_OK;
}

static bool PCU_App_HAL_StartLoraReceive(void *context, uint8_t *destination)
{
  (void)context;
  return HAL_UART_Receive_IT(&huart3, destination, 1U) == HAL_OK;
}

static bool PCU_App_HAL_TransmitController(void *context, uint8_t value)
{
  (void)context;
  return HAL_UART_Transmit(&huart2, &value, 1U,
                           PCU_APP_UART_TIMEOUT_MS) == HAL_OK;
}

static bool PCU_App_HAL_TransmitLora(void *context, uint8_t value)
{
  (void)context;
  return HAL_UART_Transmit(&huart3, &value, 1U,
                           PCU_APP_UART_TIMEOUT_MS) == HAL_OK;
}

bool PCU_App_HAL_Init(void)
{
  const PcuAppIo io = {PCU_App_HAL_StartControllerReceive,
                       PCU_App_HAL_StartLoraReceive,
                       PCU_App_HAL_TransmitController,
                       PCU_App_HAL_TransmitLora, NULL};
  const bool estop_input_active =
      HAL_GPIO_ReadPin(ESTOP_IN_GPIO_Port, ESTOP_IN_Pin) == GPIO_PIN_SET;
  bool controller_receive_ok;
  bool lora_receive_ok;

  pcu_app_initialized = PCU_App_Init(&pcu_app, &io, estop_input_active);
  if (!pcu_app_initialized)
  {
    return false;
  }

  controller_receive_ok = PCU_App_StartControllerReceive(&pcu_app);
  lora_receive_ok = PCU_App_StartLoraReceive(&pcu_app);
  last_controller_receive_retry_tick = HAL_GetTick();
  last_lora_receive_retry_tick = HAL_GetTick();
  return controller_receive_ok && lora_receive_ok;
}

void PCU_App_HAL_Process(uint32_t tick_ms)
{
  bool estop_input_active;

  if (!pcu_app_initialized)
  {
    return;
  }

  estop_input_active =
      HAL_GPIO_ReadPin(ESTOP_IN_GPIO_Port, ESTOP_IN_Pin) == GPIO_PIN_SET;
  (void)PCU_App_UpdateEstopInput(&pcu_app, estop_input_active);

  if (!pcu_app.controller_receive_pending &&
      ((uint32_t)(tick_ms - last_controller_receive_retry_tick) >=
       PCU_APP_RECEIVE_RETRY_INTERVAL_MS))
  {
    last_controller_receive_retry_tick = tick_ms;
    (void)PCU_App_StartControllerReceive(&pcu_app);
  }

  if (!pcu_app.lora_receive_pending &&
      ((uint32_t)(tick_ms - last_lora_receive_retry_tick) >=
       PCU_APP_RECEIVE_RETRY_INTERVAL_MS))
  {
    last_lora_receive_retry_tick = tick_ms;
    (void)PCU_App_StartLoraReceive(&pcu_app);
  }

  PCU_App_Process(&pcu_app, tick_ms);
}

const PcuApp *PCU_App_HAL_GetState(void)
{
  return pcu_app_initialized ? &pcu_app : NULL;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (!pcu_app_initialized)
  {
    return;
  }

  if (huart == &huart2)
  {
    (void)PCU_App_OnControllerReceiveComplete(&pcu_app);
  }
  else if (huart == &huart3)
  {
    (void)PCU_App_OnLoraReceiveComplete(&pcu_app);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (!pcu_app_initialized)
  {
    return;
  }

  if (huart == &huart2)
  {
    (void)PCU_App_OnControllerReceiveError(&pcu_app);
  }
  else if (huart == &huart3)
  {
    (void)PCU_App_OnLoraReceiveError(&pcu_app);
  }
}
