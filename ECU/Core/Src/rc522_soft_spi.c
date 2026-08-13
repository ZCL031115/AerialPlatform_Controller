#include "rc522_soft_spi.h"

#include <stddef.h>

static void RC522_SoftSpi_WriteReset(void *context, bool high)
{
  Rc522SoftSpi *bus = (Rc522SoftSpi *)context;

  if ((bus != NULL) && bus->initialized)
  {
    bus->io.write_reset(bus->io.context, high);
  }
}

static void RC522_SoftSpi_DelayUs(void *context, uint32_t microseconds)
{
  Rc522SoftSpi *bus = (Rc522SoftSpi *)context;

  if ((bus != NULL) && bus->initialized)
  {
    bus->io.delay_us(bus->io.context, microseconds);
  }
}

bool RC522_SoftSpi_Init(Rc522SoftSpi *bus, const Rc522SoftSpiIo *io,
                        uint32_t half_cycle_us)
{
  if ((bus == NULL) || (io == NULL) || (io->write_cs == NULL) ||
      (io->write_sck == NULL) || (io->write_mosi == NULL) ||
      (io->read_miso == NULL) || (io->write_reset == NULL) ||
      (io->delay_us == NULL))
  {
    return false;
  }

  bus->io = *io;
  bus->half_cycle_us = (half_cycle_us == 0U) ? 1U : half_cycle_us;
  bus->initialized = true;

  bus->io.write_cs(bus->io.context, true);
  bus->io.write_sck(bus->io.context, false);
  bus->io.write_mosi(bus->io.context, false);
  bus->io.write_reset(bus->io.context, true);
  return true;
}

uint8_t RC522_SoftSpi_TransferByte(Rc522SoftSpi *bus, uint8_t value)
{
  uint8_t received = 0U;
  uint8_t bit;

  if ((bus == NULL) || !bus->initialized)
  {
    return 0U;
  }

  for (bit = 0U; bit < 8U; bit++)
  {
    bus->io.write_mosi(bus->io.context, (value & 0x80U) != 0U);
    bus->io.delay_us(bus->io.context, bus->half_cycle_us);

    bus->io.write_sck(bus->io.context, true);
    bus->io.delay_us(bus->io.context, bus->half_cycle_us);
    received = (uint8_t)(received << 1U);
    if (bus->io.read_miso(bus->io.context))
    {
      received |= 0x01U;
    }

    bus->io.write_sck(bus->io.context, false);
    bus->io.delay_us(bus->io.context, bus->half_cycle_us);
    value = (uint8_t)(value << 1U);
  }

  return received;
}

uint8_t RC522_SoftSpi_ReadRegister(void *context, uint8_t address)
{
  Rc522SoftSpi *bus = (Rc522SoftSpi *)context;
  uint8_t value;

  if ((bus == NULL) || !bus->initialized)
  {
    return 0U;
  }

  bus->io.write_cs(bus->io.context, false);
  (void)RC522_SoftSpi_TransferByte(
      bus, (uint8_t)(((uint8_t)(address << 1U) & 0x7EU) | 0x80U));
  value = RC522_SoftSpi_TransferByte(bus, 0U);
  bus->io.write_cs(bus->io.context, true);
  return value;
}

void RC522_SoftSpi_WriteRegister(void *context, uint8_t address, uint8_t value)
{
  Rc522SoftSpi *bus = (Rc522SoftSpi *)context;

  if ((bus == NULL) || !bus->initialized)
  {
    return;
  }

  bus->io.write_cs(bus->io.context, false);
  (void)RC522_SoftSpi_TransferByte(
      bus, (uint8_t)((uint8_t)(address << 1U) & 0x7EU));
  (void)RC522_SoftSpi_TransferByte(bus, value);
  bus->io.write_cs(bus->io.context, true);
}

bool RC522_SoftSpi_BuildDeviceIo(Rc522SoftSpi *bus, Rc522Io *device_io)
{
  if ((bus == NULL) || !bus->initialized || (device_io == NULL))
  {
    return false;
  }

  device_io->read_register = RC522_SoftSpi_ReadRegister;
  device_io->write_register = RC522_SoftSpi_WriteRegister;
  device_io->write_reset = RC522_SoftSpi_WriteReset;
  device_io->delay_us = RC522_SoftSpi_DelayUs;
  device_io->context = bus;
  return true;
}
