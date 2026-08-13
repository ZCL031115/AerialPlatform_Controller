#include "bmp280_hal.h"

#include "main.h"
#include "soft_i2c.h"

#include <stddef.h>

enum
{
  BMP280_I2C_ADDRESS = 0x76U
};

static SoftI2c bmp280_bus;
static Bmp280 bmp280_device;
static bool bmp280_hal_initialized;

static void BMP280_HAL_WriteScl(void *context, bool high)
{
  (void)context;
  HAL_GPIO_WritePin(BMP_SCL_GPIO_Port, BMP_SCL_Pin,
                    high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void BMP280_HAL_WriteSda(void *context, bool high)
{
  (void)context;
  HAL_GPIO_WritePin(BMP_SDA_GPIO_Port, BMP_SDA_Pin,
                    high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool BMP280_HAL_ReadSda(void *context)
{
  (void)context;
  return HAL_GPIO_ReadPin(BMP_SDA_GPIO_Port, BMP_SDA_Pin) == GPIO_PIN_SET;
}

static void BMP280_HAL_DelayUs(void *context, uint32_t microseconds)
{
  uint32_t start;
  uint32_t ticks;
  const uint32_t ticks_per_microsecond =
      HAL_RCC_GetHCLKFreq() / 1000000U;

  (void)context;
  ticks = ticks_per_microsecond * microseconds;
  start = DWT->CYCCNT;
  while ((uint32_t)(DWT->CYCCNT - start) < ticks)
  {
  }
}

static bool BMP280_HAL_ReadRegisters(void *context, uint8_t start_register,
                                     uint8_t *data, size_t length)
{
  SoftI2c *bus = (SoftI2c *)context;

  return SoftI2c_WriteRead(bus, BMP280_I2C_ADDRESS, &start_register, 1U, data,
                           length);
}

static bool BMP280_HAL_WriteRegister(void *context, uint8_t address,
                                     uint8_t value)
{
  SoftI2c *bus = (SoftI2c *)context;
  const uint8_t data[2] = {address, value};

  return SoftI2c_Write(bus, BMP280_I2C_ADDRESS, data, sizeof(data));
}

static void BMP280_HAL_DelayMs(void *context, uint32_t milliseconds)
{
  (void)context;
  HAL_Delay(milliseconds);
}

bool BMP280_HAL_Init(void)
{
  const SoftI2cIo bus_io = {
    BMP280_HAL_WriteScl,
    BMP280_HAL_WriteSda,
    BMP280_HAL_ReadSda,
    BMP280_HAL_DelayUs,
    NULL
  };
  Bmp280Io device_io;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
  {
    return false;
  }

  if (!SoftI2c_Init(&bmp280_bus, &bus_io, 5U))
  {
    return false;
  }

  device_io.read_registers = BMP280_HAL_ReadRegisters;
  device_io.write_register = BMP280_HAL_WriteRegister;
  device_io.delay_ms = BMP280_HAL_DelayMs;
  device_io.context = &bmp280_bus;
  bmp280_hal_initialized =
      BMP280_Init(&bmp280_device, &device_io) == BMP280_STATUS_OK;
  return bmp280_hal_initialized;
}

Bmp280Status BMP280_HAL_ReadMeasurement(Bmp280Measurement *measurement)
{
  if (!bmp280_hal_initialized)
  {
    return BMP280_STATUS_INVALID_ARGUMENT;
  }

  return BMP280_ReadMeasurement(&bmp280_device, measurement);
}

const Bmp280 *BMP280_HAL_GetDevice(void)
{
  return bmp280_hal_initialized ? &bmp280_device : NULL;
}
