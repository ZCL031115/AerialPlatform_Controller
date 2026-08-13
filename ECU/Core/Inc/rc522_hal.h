#ifndef RC522_HAL_H
#define RC522_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rc522.h"

#include <stdbool.h>
#include <stdint.h>

bool RC522_HAL_Init(void);
Rc522Status RC522_HAL_ReadUid(uint8_t uid[RC522_UID_SIZE]);
Rc522Status RC522_HAL_Halt(void);
uint8_t RC522_HAL_GetVersion(void);
const Rc522 *RC522_HAL_GetDevice(void);

#ifdef __cplusplus
}
#endif

#endif
