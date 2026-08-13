#include "ecu_app_hal.h"

#include "main.h"
#include "usart.h"

#include <stddef.h>

static EcuApp ecu_app;
static bool ecu_app_initialized;
static bool external_uart_initialized;
static uint32_t last_external_receive_retry_tick;

static bool ECU_App_HAL_InitExternalUart(void)
{
  huart2.Instance = USART2;
  (void)HAL_UART_DeInit(&huart2);
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  external_uart_initialized = (HAL_UART_Init(&huart2) == HAL_OK);
  return external_uart_initialized;
}

static bool ECU_App_HAL_StartReceive(void *context, uint8_t *destination)
{
  (void)context;
  return HAL_UART_Receive_IT(&huart3, destination, 1U) == HAL_OK;
}

static bool ECU_App_HAL_StartExternalReceive(void *context,
                                             uint8_t *destination)
{
  HAL_StatusTypeDef status;

  (void)context;
  if (!external_uart_initialized)
  {
    return false;
  }

  status = HAL_UART_Receive_IT(&huart2, destination, 1U);
  if (status != HAL_OK)
  {
    external_uart_initialized = false;
  }
  return status == HAL_OK;
}

static bool ECU_App_HAL_TransmitLora(void *context, uint8_t value)
{
  (void)context;
  return HAL_UART_Transmit(&huart3, &value, 1U, 2U) == HAL_OK;
}

static bool ECU_App_HAL_TransmitExternal(void *context, uint8_t value)
{
  HAL_StatusTypeDef status;

  (void)context;
  if (!external_uart_initialized)
  {
    return false;
  }

  status = HAL_UART_Transmit(&huart2, &value, 1U, 2U);
  if (status != HAL_OK)
  {
    external_uart_initialized = false;
  }
  return status == HAL_OK;
}

static void ECU_App_HAL_WriteRelay(void *context, bool energized)
{
  (void)context;
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6,
                    energized ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void ECU_App_HAL_WriteLoraMode(void *context, bool md0, bool md1)
{
  (void)context;
  HAL_GPIO_WritePin(LoRa_MD0_GPIO_Port, LoRa_MD0_Pin,
                    md0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LoRa_MD1_GPIO_Port, LoRa_MD1_Pin,
                    md1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool ECU_App_HAL_Init(void)
{
  EcuAppIo io;

  io.start_receive = ECU_App_HAL_StartReceive;
  io.start_external_receive = ECU_App_HAL_StartExternalReceive;
  io.transmit_lora = ECU_App_HAL_TransmitLora;
  io.transmit_external = ECU_App_HAL_TransmitExternal;
  io.write_relay = ECU_App_HAL_WriteRelay;
  io.write_lora_mode = ECU_App_HAL_WriteLoraMode;
  io.context = NULL;

  ecu_app_initialized = ECU_App_Init(&ecu_app, &io);
  external_uart_initialized = false;
  if (!ecu_app_initialized)
  {
    return false;
  }

  if (!ECU_App_StartReceive(&ecu_app))
  {
    return false;
  }

  ecu_app_initialized = true;
  if (ECU_App_HAL_InitExternalUart())
  {
    (void)ECU_App_StartExternalReceive(&ecu_app);
  }
  last_external_receive_retry_tick = HAL_GetTick();
  return true;
}

void ECU_App_HAL_Process(void)
{
  if (ecu_app_initialized)
  {
    const uint32_t tick = HAL_GetTick();

    if (!ecu_app.external_receive_pending &&
        ((uint32_t)(tick - last_external_receive_retry_tick) >= 1000U))
    {
      last_external_receive_retry_tick = tick;
      if (external_uart_initialized || ECU_App_HAL_InitExternalUart())
      {
        (void)ECU_App_StartExternalReceive(&ecu_app);
      }
    }
    ECU_App_Process(&ecu_app);
  }
}

bool ECU_App_HAL_SetLoraMode(EcuLoraMode mode)
{
  return ecu_app_initialized && ECU_App_SetLoraMode(&ecu_app, mode);
}

bool ECU_App_HAL_IsExternalUartAvailable(void)
{
  return external_uart_initialized;
}

const EcuApp *ECU_App_HAL_GetState(void)
{
  return ecu_app_initialized ? &ecu_app : NULL;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (ecu_app_initialized && (huart == &huart3))
  {
    (void)ECU_App_OnReceiveComplete(&ecu_app);
  }
  else if (ecu_app_initialized && (huart == &huart2))
  {
    (void)ECU_App_OnExternalReceiveComplete(&ecu_app);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (ecu_app_initialized && (huart == &huart3))
  {
    (void)ECU_App_OnReceiveError(&ecu_app);
  }
  else if (ecu_app_initialized && (huart == &huart2))
  {
    external_uart_initialized = false;
    (void)ECU_App_OnExternalReceiveError(&ecu_app);
  }
}
