#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*SoftI2cWritePinFn)(void *context, bool high);
typedef bool (*SoftI2cReadPinFn)(void *context);
typedef void (*SoftI2cDelayUsFn)(void *context, uint32_t microseconds);

typedef struct
{
  SoftI2cWritePinFn write_scl;
  SoftI2cWritePinFn write_sda;
  SoftI2cReadPinFn read_sda;
  SoftI2cDelayUsFn delay_us;
  void *context;
} SoftI2cIo;

typedef struct
{
  SoftI2cIo io;
  uint32_t half_cycle_us;
  bool initialized;
} SoftI2c;

bool SoftI2c_Init(SoftI2c *bus, const SoftI2cIo *io,
                  uint32_t half_cycle_us);
bool SoftI2c_Write(SoftI2c *bus, uint8_t address_7bit,
                   const uint8_t *data, size_t length);
bool SoftI2c_WriteRead(SoftI2c *bus, uint8_t address_7bit,
                       const uint8_t *write_data, size_t write_length,
                       uint8_t *read_data, size_t read_length);

#ifdef __cplusplus
}
#endif

#endif
