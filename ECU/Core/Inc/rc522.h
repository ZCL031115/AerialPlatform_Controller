#ifndef RC522_H
#define RC522_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
  RC522_UID_SIZE = 4U,
  RC522_TAG_TYPE_SIZE = 2U
};

typedef enum
{
  RC522_STATUS_OK = 0,
  RC522_STATUS_NO_CARD,
  RC522_STATUS_TIMEOUT,
  RC522_STATUS_COMMUNICATION_ERROR,
  RC522_STATUS_PROTOCOL_ERROR,
  RC522_STATUS_BUFFER_TOO_SMALL,
  RC522_STATUS_INVALID_ARGUMENT
} Rc522Status;

typedef uint8_t (*Rc522ReadRegisterFn)(void *context, uint8_t address);
typedef void (*Rc522WriteRegisterFn)(void *context, uint8_t address,
                                     uint8_t value);
typedef void (*Rc522WriteResetFn)(void *context, bool high);
typedef void (*Rc522DelayUsFn)(void *context, uint32_t microseconds);

typedef struct
{
  Rc522ReadRegisterFn read_register;
  Rc522WriteRegisterFn write_register;
  Rc522WriteResetFn write_reset;
  Rc522DelayUsFn delay_us;
  void *context;
} Rc522Io;

typedef struct
{
  Rc522Io io;
  uint32_t communication_timeout_loops;
  bool initialized;
} Rc522;

bool RC522_Init(Rc522 *device, const Rc522Io *io);
Rc522Status RC522_ResetAndConfigure(Rc522 *device);
Rc522Status RC522_Request(Rc522 *device, uint8_t request_code,
                          uint8_t tag_type[RC522_TAG_TYPE_SIZE]);
Rc522Status RC522_Anticollision(Rc522 *device,
                                uint8_t uid[RC522_UID_SIZE]);
Rc522Status RC522_Select(Rc522 *device,
                         const uint8_t uid[RC522_UID_SIZE]);
Rc522Status RC522_Halt(Rc522 *device);
Rc522Status RC522_ReadUid(Rc522 *device, uint8_t uid[RC522_UID_SIZE]);
uint8_t RC522_ReadVersion(Rc522 *device);

#ifdef __cplusplus
}
#endif

#endif
