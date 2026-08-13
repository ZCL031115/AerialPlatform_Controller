#include "soft_i2c.h"

#include <stddef.h>

static void SoftI2c_Delay(const SoftI2c *bus)
{
  bus->io.delay_us(bus->io.context, bus->half_cycle_us);
}

static void SoftI2c_Start(const SoftI2c *bus)
{
  bus->io.write_sda(bus->io.context, true);
  bus->io.write_scl(bus->io.context, true);
  SoftI2c_Delay(bus);
  bus->io.write_sda(bus->io.context, false);
  SoftI2c_Delay(bus);
  bus->io.write_scl(bus->io.context, false);
  SoftI2c_Delay(bus);
}

static void SoftI2c_Stop(const SoftI2c *bus)
{
  bus->io.write_sda(bus->io.context, false);
  SoftI2c_Delay(bus);
  bus->io.write_scl(bus->io.context, true);
  SoftI2c_Delay(bus);
  bus->io.write_sda(bus->io.context, true);
  SoftI2c_Delay(bus);
}

static bool SoftI2c_WriteByte(const SoftI2c *bus, uint8_t value)
{
  uint8_t bit;
  bool acknowledged;

  for (bit = 0U; bit < 8U; bit++)
  {
    bus->io.write_sda(bus->io.context, (value & 0x80U) != 0U);
    SoftI2c_Delay(bus);
    bus->io.write_scl(bus->io.context, true);
    SoftI2c_Delay(bus);
    bus->io.write_scl(bus->io.context, false);
    SoftI2c_Delay(bus);
    value = (uint8_t)(value << 1U);
  }

  bus->io.write_sda(bus->io.context, true);
  SoftI2c_Delay(bus);
  bus->io.write_scl(bus->io.context, true);
  SoftI2c_Delay(bus);
  acknowledged = !bus->io.read_sda(bus->io.context);
  bus->io.write_scl(bus->io.context, false);
  SoftI2c_Delay(bus);
  return acknowledged;
}

static uint8_t SoftI2c_ReadByte(const SoftI2c *bus, bool acknowledge)
{
  uint8_t value = 0U;
  uint8_t bit;

  bus->io.write_sda(bus->io.context, true);
  for (bit = 0U; bit < 8U; bit++)
  {
    SoftI2c_Delay(bus);
    bus->io.write_scl(bus->io.context, true);
    SoftI2c_Delay(bus);
    value = (uint8_t)(value << 1U);
    if (bus->io.read_sda(bus->io.context))
    {
      value |= 0x01U;
    }
    bus->io.write_scl(bus->io.context, false);
    SoftI2c_Delay(bus);
  }

  bus->io.write_sda(bus->io.context, !acknowledge);
  SoftI2c_Delay(bus);
  bus->io.write_scl(bus->io.context, true);
  SoftI2c_Delay(bus);
  bus->io.write_scl(bus->io.context, false);
  SoftI2c_Delay(bus);
  bus->io.write_sda(bus->io.context, true);
  return value;
}

bool SoftI2c_Init(SoftI2c *bus, const SoftI2cIo *io,
                  uint32_t half_cycle_us)
{
  if ((bus == NULL) || (io == NULL) || (io->write_scl == NULL) ||
      (io->write_sda == NULL) || (io->read_sda == NULL) ||
      (io->delay_us == NULL))
  {
    return false;
  }

  bus->io = *io;
  bus->half_cycle_us = (half_cycle_us == 0U) ? 1U : half_cycle_us;
  bus->initialized = true;
  bus->io.write_sda(bus->io.context, true);
  bus->io.write_scl(bus->io.context, true);
  return true;
}

bool SoftI2c_Write(SoftI2c *bus, uint8_t address_7bit,
                   const uint8_t *data, size_t length)
{
  size_t index;

  if ((bus == NULL) || !bus->initialized || (address_7bit > 0x7FU) ||
      (data == NULL) || (length == 0U))
  {
    return false;
  }

  SoftI2c_Start(bus);
  if (!SoftI2c_WriteByte(bus, (uint8_t)(address_7bit << 1U)))
  {
    SoftI2c_Stop(bus);
    return false;
  }

  for (index = 0U; index < length; index++)
  {
    if (!SoftI2c_WriteByte(bus, data[index]))
    {
      SoftI2c_Stop(bus);
      return false;
    }
  }

  SoftI2c_Stop(bus);
  return true;
}

bool SoftI2c_WriteRead(SoftI2c *bus, uint8_t address_7bit,
                       const uint8_t *write_data, size_t write_length,
                       uint8_t *read_data, size_t read_length)
{
  size_t index;

  if ((bus == NULL) || !bus->initialized || (address_7bit > 0x7FU) ||
      (write_data == NULL) || (write_length == 0U) || (read_data == NULL) ||
      (read_length == 0U))
  {
    return false;
  }

  SoftI2c_Start(bus);
  if (!SoftI2c_WriteByte(bus, (uint8_t)(address_7bit << 1U)))
  {
    SoftI2c_Stop(bus);
    return false;
  }

  for (index = 0U; index < write_length; index++)
  {
    if (!SoftI2c_WriteByte(bus, write_data[index]))
    {
      SoftI2c_Stop(bus);
      return false;
    }
  }

  SoftI2c_Start(bus);
  if (!SoftI2c_WriteByte(bus, (uint8_t)((address_7bit << 1U) | 0x01U)))
  {
    SoftI2c_Stop(bus);
    return false;
  }

  for (index = 0U; index < read_length; index++)
  {
    read_data[index] = SoftI2c_ReadByte(bus, index + 1U < read_length);
  }

  SoftI2c_Stop(bus);
  return true;
}
