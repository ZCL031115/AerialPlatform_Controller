#include "rc522_hal.h"

#include "main.h"
#include "rc522_soft_spi.h"

#include <stddef.h>

static Rc522SoftSpi rc522_bus;
static Rc522 rc522_device;
static bool rc522_hal_initialized;
static uint8_t rc522_version;

static void RC522_HAL_WriteCs(void *context, bool high)
{
  (void)context;
  HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin,
                    high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void RC522_HAL_WriteSck(void *context, bool high)
{
  (void)context;
  HAL_GPIO_WritePin(RC522_SCK_GPIO_Port, RC522_SCK_Pin,
                    high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void RC522_HAL_WriteMosi(void *context, bool high)
{
  (void)context;
  HAL_GPIO_WritePin(RC522_MOSI_GPIO_Port, RC522_MOSI_Pin,
                    high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool RC522_HAL_ReadMiso(void *context)
{
  (void)context;
  return HAL_GPIO_ReadPin(RC522_MISO_GPIO_Port, RC522_MISO_Pin) ==
         GPIO_PIN_SET;
}

static void RC522_HAL_WriteReset(void *context, bool high)
{
  (void)context;
  HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin,
                    high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void RC522_HAL_DelayUs(void *context, uint32_t microseconds)
{
  uint32_t start;
  uint32_t ticks;
  uint32_t ticks_per_microsecond;

  (void)context;
  if (microseconds == 0U)
  {
    return;
  }

  ticks_per_microsecond = HAL_RCC_GetHCLKFreq() / 1000000U;
  ticks = ticks_per_microsecond * microseconds;
  start = DWT->CYCCNT;
  while ((uint32_t)(DWT->CYCCNT - start) < ticks)
  {
  }
}

bool RC522_HAL_Init(void)
{
  const Rc522SoftSpiIo bus_io = {
    RC522_HAL_WriteCs,
    RC522_HAL_WriteSck,
    RC522_HAL_WriteMosi,
    RC522_HAL_ReadMiso,
    RC522_HAL_WriteReset,
    RC522_HAL_DelayUs,
    NULL
  };
  Rc522Io device_io;

  rc522_hal_initialized = false;
  rc522_version = 0U;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
  {
    return false;
  }

  if (!RC522_SoftSpi_Init(&rc522_bus, &bus_io, 1U) ||
      !RC522_SoftSpi_BuildDeviceIo(&rc522_bus, &device_io) ||
      !RC522_Init(&rc522_device, &device_io))
  {
    return false;
  }

  if (RC522_ResetAndConfigure(&rc522_device) != RC522_STATUS_OK)
  {
    return false;
  }

  rc522_version = RC522_ReadVersion(&rc522_device);
  rc522_hal_initialized =
      (rc522_version != 0x00U) && (rc522_version != 0xFFU);
  return rc522_hal_initialized;
}

Rc522Status RC522_HAL_ReadUid(uint8_t uid[RC522_UID_SIZE])
{
  if (!rc522_hal_initialized)
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  return RC522_ReadUid(&rc522_device, uid);
}

Rc522Status RC522_HAL_Halt(void)
{
  if (!rc522_hal_initialized)
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  return RC522_Halt(&rc522_device);
}

uint8_t RC522_HAL_GetVersion(void)
{
  return rc522_version;
}

const Rc522 *RC522_HAL_GetDevice(void)
{
  return rc522_hal_initialized ? &rc522_device : NULL;
}
