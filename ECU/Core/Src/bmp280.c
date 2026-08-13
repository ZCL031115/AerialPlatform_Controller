#include "bmp280.h"

#include <stddef.h>

enum
{
  BMP280_REG_CALIBRATION = 0x88U,
  BMP280_REG_CHIP_ID = 0xD0U,
  BMP280_REG_RESET = 0xE0U,
  BMP280_REG_STATUS = 0xF3U,
  BMP280_REG_CONTROL_MEASUREMENT = 0xF4U,
  BMP280_REG_CONFIG = 0xF5U,
  BMP280_REG_PRESSURE_MSB = 0xF7U,
  BMP280_RESET_COMMAND = 0xB6U,
  BMP280_STATUS_MEASURING_MASK = 0x08U,
  BMP280_STATUS_IMAGE_UPDATE_MASK = 0x01U,
  BMP280_CALIBRATION_LENGTH = 24U,
  BMP280_MEASUREMENT_LENGTH = 6U,
  BMP280_RESET_POLL_LIMIT = 100U
};

static uint16_t BMP280_DecodeUnsigned16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static int16_t BMP280_DecodeSigned16(const uint8_t *data)
{
  return (int16_t)BMP280_DecodeUnsigned16(data);
}

static bool BMP280_ReadRegisters(const Bmp280 *device, uint8_t address,
                                 uint8_t *data, size_t length)
{
  return device->io.read_registers(device->io.context, address, data, length);
}

static bool BMP280_WriteRegister(const Bmp280 *device, uint8_t address,
                                 uint8_t value)
{
  return device->io.write_register(device->io.context, address, value);
}

static void BMP280_DecodeCalibration(Bmp280Calibration *calibration,
                                     const uint8_t *data)
{
  calibration->t1 = BMP280_DecodeUnsigned16(&data[0]);
  calibration->t2 = BMP280_DecodeSigned16(&data[2]);
  calibration->t3 = BMP280_DecodeSigned16(&data[4]);
  calibration->p1 = BMP280_DecodeUnsigned16(&data[6]);
  calibration->p2 = BMP280_DecodeSigned16(&data[8]);
  calibration->p3 = BMP280_DecodeSigned16(&data[10]);
  calibration->p4 = BMP280_DecodeSigned16(&data[12]);
  calibration->p5 = BMP280_DecodeSigned16(&data[14]);
  calibration->p6 = BMP280_DecodeSigned16(&data[16]);
  calibration->p7 = BMP280_DecodeSigned16(&data[18]);
  calibration->p8 = BMP280_DecodeSigned16(&data[20]);
  calibration->p9 = BMP280_DecodeSigned16(&data[22]);
}

static int32_t BMP280_CompensateTemperature(Bmp280 *device,
                                            int32_t raw_temperature)
{
  int32_t var1;
  int32_t var2;

  var1 = (((raw_temperature >> 3) -
           ((int32_t)device->calibration.t1 << 1)) *
          (int32_t)device->calibration.t2) >>
         11;
  var2 = (((((raw_temperature >> 4) - (int32_t)device->calibration.t1) *
             ((raw_temperature >> 4) - (int32_t)device->calibration.t1)) >>
            12) *
           (int32_t)device->calibration.t3) >>
          14;
  device->t_fine = var1 + var2;
  return (device->t_fine * 5 + 128) >> 8;
}

static uint32_t BMP280_CompensatePressure(const Bmp280 *device,
                                          int32_t raw_pressure)
{
  int64_t var1;
  int64_t var2;
  int64_t pressure;

  var1 = (int64_t)device->t_fine - 128000;
  var2 = var1 * var1 * (int64_t)device->calibration.p6;
  var2 += (var1 * (int64_t)device->calibration.p5) * 131072;
  var2 += (int64_t)device->calibration.p4 * 34359738368LL;
  var1 = ((var1 * var1 * (int64_t)device->calibration.p3) >> 8) +
         ((var1 * (int64_t)device->calibration.p2) * 4096);
  var1 = (((((int64_t)1 << 47) + var1) *
           (int64_t)device->calibration.p1) >>
          33);
  if (var1 == 0)
  {
    return 0U;
  }

  pressure = 1048576 - (int64_t)raw_pressure;
  pressure = (((pressure << 31) - var2) * 3125) / var1;
  var1 = ((int64_t)device->calibration.p9 * (pressure >> 13) *
          (pressure >> 13)) >>
         25;
  var2 = ((int64_t)device->calibration.p8 * pressure) >> 19;
  pressure = ((pressure + var1 + var2) >> 8) +
             ((int64_t)device->calibration.p7 * 16);
  return (pressure < 0) ? 0U : (uint32_t)pressure;
}

Bmp280Status BMP280_Init(Bmp280 *device, const Bmp280Io *io)
{
  uint8_t chip_id;
  uint8_t status;
  uint8_t calibration_data[BMP280_CALIBRATION_LENGTH];
  uint32_t poll;

  if ((device == NULL) || (io == NULL) || (io->read_registers == NULL) ||
      (io->write_register == NULL) || (io->delay_ms == NULL))
  {
    return BMP280_STATUS_INVALID_ARGUMENT;
  }

  device->io = *io;
  device->initialized = false;
  device->t_fine = 0;
  device->chip_id = 0U;

  if (!BMP280_ReadRegisters(device, BMP280_REG_CHIP_ID, &chip_id, 1U))
  {
    return BMP280_STATUS_IO_ERROR;
  }
  device->chip_id = chip_id;
  if (chip_id != BMP280_CHIP_ID)
  {
    return BMP280_STATUS_BAD_CHIP_ID;
  }

  if (!BMP280_WriteRegister(device, BMP280_REG_RESET, BMP280_RESET_COMMAND))
  {
    return BMP280_STATUS_IO_ERROR;
  }
  device->io.delay_ms(device->io.context, 2U);

  for (poll = 0U; poll < BMP280_RESET_POLL_LIMIT; poll++)
  {
    if (!BMP280_ReadRegisters(device, BMP280_REG_STATUS, &status, 1U))
    {
      return BMP280_STATUS_IO_ERROR;
    }
    if ((status & BMP280_STATUS_IMAGE_UPDATE_MASK) == 0U)
    {
      break;
    }
    device->io.delay_ms(device->io.context, 1U);
  }
  if (poll == BMP280_RESET_POLL_LIMIT)
  {
    return BMP280_STATUS_TIMEOUT;
  }

  if (!BMP280_ReadRegisters(device, BMP280_REG_CALIBRATION,
                            calibration_data, sizeof(calibration_data)))
  {
    return BMP280_STATUS_IO_ERROR;
  }
  BMP280_DecodeCalibration(&device->calibration, calibration_data);
  if (device->calibration.p1 == 0U)
  {
    return BMP280_STATUS_BAD_CALIBRATION;
  }

  if (!BMP280_WriteRegister(device, BMP280_REG_CONFIG, 0x10U) ||
      !BMP280_WriteRegister(device, BMP280_REG_CONTROL_MEASUREMENT, 0x2FU))
  {
    return BMP280_STATUS_IO_ERROR;
  }

  device->initialized = true;
  return BMP280_STATUS_OK;
}

Bmp280Status BMP280_ReadMeasurement(Bmp280 *device,
                                    Bmp280Measurement *measurement)
{
  uint8_t data[BMP280_MEASUREMENT_LENGTH];

  if ((device == NULL) || !device->initialized || (measurement == NULL))
  {
    return BMP280_STATUS_INVALID_ARGUMENT;
  }

  if (!BMP280_ReadRegisters(device, BMP280_REG_PRESSURE_MSB, data,
                            sizeof(data)))
  {
    return BMP280_STATUS_IO_ERROR;
  }

  measurement->raw_pressure =
      ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) |
      ((int32_t)data[2] >> 4);
  measurement->raw_temperature =
      ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) |
      ((int32_t)data[5] >> 4);

  measurement->temperature_centi_c =
      BMP280_CompensateTemperature(device, measurement->raw_temperature);
  measurement->pressure_q24_8_pa =
      BMP280_CompensatePressure(device, measurement->raw_pressure);
  measurement->temperature_c =
      (double)measurement->temperature_centi_c / 100.0;
  measurement->pressure_pa =
      (double)measurement->pressure_q24_8_pa / 256.0;
  return BMP280_STATUS_OK;
}

Bmp280Status BMP280_ReadStatus(Bmp280 *device, uint8_t *status_register)
{
  if ((device == NULL) || !device->initialized || (status_register == NULL))
  {
    return BMP280_STATUS_INVALID_ARGUMENT;
  }

  if (!BMP280_ReadRegisters(device, BMP280_REG_STATUS, status_register, 1U))
  {
    return BMP280_STATUS_IO_ERROR;
  }

  return BMP280_STATUS_OK;
}
