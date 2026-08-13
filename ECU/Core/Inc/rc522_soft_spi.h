#ifndef RC522_SOFT_SPI_H
#define RC522_SOFT_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rc522.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*Rc522SoftSpiWritePinFn)(void *context, bool high);
typedef bool (*Rc522SoftSpiReadPinFn)(void *context);
typedef void (*Rc522SoftSpiDelayUsFn)(void *context, uint32_t microseconds);

typedef struct
{
  Rc522SoftSpiWritePinFn write_cs;
  Rc522SoftSpiWritePinFn write_sck;
  Rc522SoftSpiWritePinFn write_mosi;
  Rc522SoftSpiReadPinFn read_miso;
  Rc522SoftSpiWritePinFn write_reset;
  Rc522SoftSpiDelayUsFn delay_us;
  void *context;
} Rc522SoftSpiIo;

typedef struct
{
  Rc522SoftSpiIo io;
  uint32_t half_cycle_us;
  bool initialized;
} Rc522SoftSpi;

bool RC522_SoftSpi_Init(Rc522SoftSpi *bus, const Rc522SoftSpiIo *io,
                        uint32_t half_cycle_us);
uint8_t RC522_SoftSpi_TransferByte(Rc522SoftSpi *bus, uint8_t value);
uint8_t RC522_SoftSpi_ReadRegister(void *context, uint8_t address);
void RC522_SoftSpi_WriteRegister(void *context, uint8_t address,
                                 uint8_t value);
bool RC522_SoftSpi_BuildDeviceIo(Rc522SoftSpi *bus, Rc522Io *device_io);

#ifdef __cplusplus
}
#endif

#endif
