#ifndef BMP280_HAL_H
#define BMP280_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bmp280.h"

#include <stdbool.h>

bool BMP280_HAL_Init(void);
Bmp280Status BMP280_HAL_ReadMeasurement(Bmp280Measurement *measurement);
const Bmp280 *BMP280_HAL_GetDevice(void);

#ifdef __cplusplus
}
#endif

#endif
