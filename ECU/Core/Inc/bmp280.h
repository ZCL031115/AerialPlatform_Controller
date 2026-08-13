#ifndef BMP280_H
#define BMP280_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
  BMP280_CHIP_ID = 0x58U
};

typedef enum
{
  BMP280_STATUS_OK = 0,
  BMP280_STATUS_IO_ERROR,
  BMP280_STATUS_BAD_CHIP_ID,
  BMP280_STATUS_TIMEOUT,
  BMP280_STATUS_BAD_CALIBRATION,
  BMP280_STATUS_INVALID_ARGUMENT
} Bmp280Status;

typedef bool (*Bmp280ReadRegistersFn)(void *context, uint8_t start_register,
                                      uint8_t *data, size_t length);
typedef bool (*Bmp280WriteRegisterFn)(void *context, uint8_t address,
                                      uint8_t value);
typedef void (*Bmp280DelayMsFn)(void *context, uint32_t milliseconds);

typedef struct
{
  Bmp280ReadRegistersFn read_registers;
  Bmp280WriteRegisterFn write_register;
  Bmp280DelayMsFn delay_ms;
  void *context;
} Bmp280Io;

typedef struct
{
  uint16_t t1;
  int16_t t2;
  int16_t t3;
  uint16_t p1;
  int16_t p2;
  int16_t p3;
  int16_t p4;
  int16_t p5;
  int16_t p6;
  int16_t p7;
  int16_t p8;
  int16_t p9;
} Bmp280Calibration;

typedef struct
{
  int32_t raw_temperature;
  int32_t raw_pressure;
  int32_t temperature_centi_c;
  uint32_t pressure_q24_8_pa;
  double temperature_c;
  double pressure_pa;
} Bmp280Measurement;

typedef struct
{
  Bmp280Io io;
  Bmp280Calibration calibration;
  int32_t t_fine;
  uint8_t chip_id;
  bool initialized;
} Bmp280;

Bmp280Status BMP280_Init(Bmp280 *device, const Bmp280Io *io);
Bmp280Status BMP280_ReadMeasurement(Bmp280 *device,
                                    Bmp280Measurement *measurement);
Bmp280Status BMP280_ReadStatus(Bmp280 *device, uint8_t *status_register);

#ifdef __cplusplus
}
#endif

#endif
