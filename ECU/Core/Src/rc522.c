#include "rc522.h"

#include <stddef.h>

enum
{
  RC522_REG_COMMAND = 0x01U,
  RC522_REG_COM_I_EN = 0x02U,
  RC522_REG_DIV_IRQ = 0x05U,
  RC522_REG_COM_IRQ = 0x04U,
  RC522_REG_ERROR = 0x06U,
  RC522_REG_STATUS2 = 0x08U,
  RC522_REG_FIFO_DATA = 0x09U,
  RC522_REG_FIFO_LEVEL = 0x0AU,
  RC522_REG_CONTROL = 0x0CU,
  RC522_REG_BIT_FRAMING = 0x0DU,
  RC522_REG_COLLISION = 0x0EU,
  RC522_REG_MODE = 0x11U,
  RC522_REG_TX_CONTROL = 0x14U,
  RC522_REG_TX_AUTO = 0x15U,
  RC522_REG_RX_SELECT = 0x17U,
  RC522_REG_CRC_RESULT_HIGH = 0x21U,
  RC522_REG_CRC_RESULT_LOW = 0x22U,
  RC522_REG_RF_CONFIG = 0x26U,
  RC522_REG_TIMER_MODE = 0x2AU,
  RC522_REG_TIMER_PRESCALER = 0x2BU,
  RC522_REG_TIMER_RELOAD_HIGH = 0x2CU,
  RC522_REG_TIMER_RELOAD_LOW = 0x2DU,
  RC522_REG_VERSION = 0x37U,
  RC522_COMMAND_IDLE = 0x00U,
  RC522_COMMAND_CALCULATE_CRC = 0x03U,
  RC522_COMMAND_TRANSCEIVE = 0x0CU,
  RC522_COMMAND_SOFT_RESET = 0x0FU,
  RC522_PICC_REQUEST_ALL = 0x52U,
  RC522_PICC_ANTICOLLISION = 0x93U,
  RC522_PICC_HALT = 0x50U,
  RC522_MAX_FIFO_LENGTH = 64U,
  RC522_DEFAULT_TIMEOUT_LOOPS = 1000U,
  RC522_CRC_TIMEOUT_LOOPS = 255U
};

static uint8_t RC522_ReadRegister(const Rc522 *device, uint8_t address)
{
  return device->io.read_register(device->io.context, address);
}

static void RC522_WriteRegister(const Rc522 *device, uint8_t address,
                                uint8_t value)
{
  device->io.write_register(device->io.context, address, value);
}

static void RC522_SetBitMask(const Rc522 *device, uint8_t address, uint8_t mask)
{
  RC522_WriteRegister(device, address,
                      (uint8_t)(RC522_ReadRegister(device, address) | mask));
}

static void RC522_ClearBitMask(const Rc522 *device, uint8_t address,
                              uint8_t mask)
{
  RC522_WriteRegister(
      device, address,
      (uint8_t)(RC522_ReadRegister(device, address) & (uint8_t)(~mask)));
}

static Rc522Status RC522_Communicate(Rc522 *device, uint8_t command,
                                     const uint8_t *input,
                                     size_t input_length, uint8_t *output,
                                     size_t output_capacity,
                                     uint32_t *output_length_bits)
{
  uint8_t interrupt_enable;
  uint8_t wait_for;
  uint8_t interrupt_value = 0U;
  uint8_t fifo_length;
  uint8_t last_bits;
  uint32_t poll;
  size_t index;
  Rc522Status status;

  if ((device == NULL) || !device->initialized ||
      ((input == NULL) && (input_length != 0U)) ||
      (input_length > RC522_MAX_FIFO_LENGTH) || (output == NULL) ||
      (output_length_bits == NULL))
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  if (command != RC522_COMMAND_TRANSCEIVE)
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  interrupt_enable = 0x77U;
  wait_for = 0x30U;
  *output_length_bits = 0U;

  RC522_WriteRegister(device, RC522_REG_COM_I_EN,
                      (uint8_t)(interrupt_enable | 0x80U));
  RC522_ClearBitMask(device, RC522_REG_COM_IRQ, 0x80U);
  RC522_WriteRegister(device, RC522_REG_COMMAND, RC522_COMMAND_IDLE);
  RC522_SetBitMask(device, RC522_REG_FIFO_LEVEL, 0x80U);

  for (index = 0U; index < input_length; index++)
  {
    RC522_WriteRegister(device, RC522_REG_FIFO_DATA, input[index]);
  }

  RC522_WriteRegister(device, RC522_REG_COMMAND, command);
  RC522_SetBitMask(device, RC522_REG_BIT_FRAMING, 0x80U);

  for (poll = 0U; poll < device->communication_timeout_loops; poll++)
  {
    interrupt_value = RC522_ReadRegister(device, RC522_REG_COM_IRQ);
    if (((interrupt_value & 0x01U) != 0U) ||
        ((interrupt_value & wait_for) != 0U))
    {
      break;
    }
  }

  RC522_ClearBitMask(device, RC522_REG_BIT_FRAMING, 0x80U);

  if (poll == device->communication_timeout_loops)
  {
    status = RC522_STATUS_TIMEOUT;
  }
  else if ((RC522_ReadRegister(device, RC522_REG_ERROR) & 0x1BU) != 0U)
  {
    status = RC522_STATUS_COMMUNICATION_ERROR;
  }
  else if ((interrupt_value & 0x01U) != 0U)
  {
    status = RC522_STATUS_NO_CARD;
  }
  else
  {
    fifo_length = RC522_ReadRegister(device, RC522_REG_FIFO_LEVEL);
    last_bits = (uint8_t)(RC522_ReadRegister(device, RC522_REG_CONTROL) &
                          0x07U);
    *output_length_bits = (last_bits == 0U)
                              ? ((uint32_t)fifo_length * 8U)
                              : (((uint32_t)(fifo_length - 1U) * 8U) +
                                 (uint32_t)last_bits);

    status = ((size_t)fifo_length > output_capacity)
                 ? RC522_STATUS_BUFFER_TOO_SMALL
                 : RC522_STATUS_OK;

    for (index = 0U; index < (size_t)fifo_length; index++)
    {
      const uint8_t value =
          RC522_ReadRegister(device, RC522_REG_FIFO_DATA);
      if (index < output_capacity)
      {
        output[index] = value;
      }
    }
  }

  RC522_SetBitMask(device, RC522_REG_CONTROL, 0x80U);
  RC522_WriteRegister(device, RC522_REG_COMMAND, RC522_COMMAND_IDLE);
  return status;
}

static Rc522Status RC522_CalculateCrc(Rc522 *device, const uint8_t *data,
                                      size_t length, uint8_t result[2])
{
  uint32_t poll;
  size_t index;

  if ((device == NULL) || !device->initialized || (data == NULL) ||
      (length == 0U) || (length > RC522_MAX_FIFO_LENGTH) ||
      (result == NULL))
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  RC522_ClearBitMask(device, RC522_REG_DIV_IRQ, 0x04U);
  RC522_WriteRegister(device, RC522_REG_COMMAND, RC522_COMMAND_IDLE);
  RC522_SetBitMask(device, RC522_REG_FIFO_LEVEL, 0x80U);

  for (index = 0U; index < length; index++)
  {
    RC522_WriteRegister(device, RC522_REG_FIFO_DATA, data[index]);
  }

  RC522_WriteRegister(device, RC522_REG_COMMAND,
                      RC522_COMMAND_CALCULATE_CRC);

  for (poll = 0U; poll < RC522_CRC_TIMEOUT_LOOPS; poll++)
  {
    if ((RC522_ReadRegister(device, RC522_REG_DIV_IRQ) & 0x04U) != 0U)
    {
      result[0] = RC522_ReadRegister(device, RC522_REG_CRC_RESULT_LOW);
      result[1] = RC522_ReadRegister(device, RC522_REG_CRC_RESULT_HIGH);
      return RC522_STATUS_OK;
    }
  }

  return RC522_STATUS_TIMEOUT;
}

bool RC522_Init(Rc522 *device, const Rc522Io *io)
{
  if ((device == NULL) || (io == NULL) || (io->read_register == NULL) ||
      (io->write_register == NULL) || (io->write_reset == NULL) ||
      (io->delay_us == NULL))
  {
    return false;
  }

  device->io = *io;
  device->communication_timeout_loops = RC522_DEFAULT_TIMEOUT_LOOPS;
  device->initialized = true;
  return true;
}

Rc522Status RC522_ResetAndConfigure(Rc522 *device)
{
  uint32_t poll;
  uint8_t tx_control;

  if ((device == NULL) || !device->initialized)
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  device->io.write_reset(device->io.context, true);
  device->io.delay_us(device->io.context, 1U);
  device->io.write_reset(device->io.context, false);
  device->io.delay_us(device->io.context, 1U);
  device->io.write_reset(device->io.context, true);
  device->io.delay_us(device->io.context, 1U);

  RC522_WriteRegister(device, RC522_REG_COMMAND, RC522_COMMAND_SOFT_RESET);
  for (poll = 0U; poll < device->communication_timeout_loops; poll++)
  {
    if ((RC522_ReadRegister(device, RC522_REG_COMMAND) & 0x10U) == 0U)
    {
      break;
    }
  }

  if (poll == device->communication_timeout_loops)
  {
    return RC522_STATUS_TIMEOUT;
  }

  RC522_WriteRegister(device, RC522_REG_MODE, 0x3DU);
  RC522_WriteRegister(device, RC522_REG_TIMER_RELOAD_LOW, 30U);
  RC522_WriteRegister(device, RC522_REG_TIMER_RELOAD_HIGH, 0U);
  RC522_WriteRegister(device, RC522_REG_TIMER_MODE, 0x8DU);
  RC522_WriteRegister(device, RC522_REG_TIMER_PRESCALER, 0x3EU);
  RC522_WriteRegister(device, RC522_REG_TX_AUTO, 0x40U);

  RC522_ClearBitMask(device, RC522_REG_STATUS2, 0x08U);
  RC522_WriteRegister(device, RC522_REG_MODE, 0x3DU);
  RC522_WriteRegister(device, RC522_REG_RX_SELECT, 0x86U);
  RC522_WriteRegister(device, RC522_REG_RF_CONFIG, 0x7FU);
  RC522_WriteRegister(device, RC522_REG_TIMER_RELOAD_LOW, 30U);
  RC522_WriteRegister(device, RC522_REG_TIMER_RELOAD_HIGH, 0U);
  RC522_WriteRegister(device, RC522_REG_TIMER_MODE, 0x8DU);
  RC522_WriteRegister(device, RC522_REG_TIMER_PRESCALER, 0x3EU);
  device->io.delay_us(device->io.context, 2U);

  tx_control = RC522_ReadRegister(device, RC522_REG_TX_CONTROL);
  if ((tx_control & 0x03U) != 0x03U)
  {
    RC522_WriteRegister(device, RC522_REG_TX_CONTROL,
                        (uint8_t)(tx_control | 0x03U));
  }

  return RC522_STATUS_OK;
}

Rc522Status RC522_Request(Rc522 *device, uint8_t request_code,
                          uint8_t tag_type[RC522_TAG_TYPE_SIZE])
{
  uint8_t response[RC522_TAG_TYPE_SIZE] = {0U};
  uint32_t response_bits = 0U;
  Rc522Status status;

  if ((device == NULL) || !device->initialized || (tag_type == NULL))
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  RC522_ClearBitMask(device, RC522_REG_STATUS2, 0x08U);
  RC522_WriteRegister(device, RC522_REG_BIT_FRAMING, 0x07U);
  RC522_SetBitMask(device, RC522_REG_TX_CONTROL, 0x03U);

  status = RC522_Communicate(device, RC522_COMMAND_TRANSCEIVE, &request_code,
                             1U, response, sizeof(response), &response_bits);
  if (status != RC522_STATUS_OK)
  {
    return status;
  }

  if (response_bits != 16U)
  {
    return RC522_STATUS_PROTOCOL_ERROR;
  }

  tag_type[0] = response[0];
  tag_type[1] = response[1];
  return RC522_STATUS_OK;
}

Rc522Status RC522_Anticollision(Rc522 *device,
                                uint8_t uid[RC522_UID_SIZE])
{
  static const uint8_t command[2] = {RC522_PICC_ANTICOLLISION, 0x20U};
  uint8_t response[5] = {0U};
  uint8_t checksum = 0U;
  uint8_t index;
  uint32_t response_bits = 0U;
  Rc522Status status;

  if ((device == NULL) || !device->initialized || (uid == NULL))
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  RC522_ClearBitMask(device, RC522_REG_STATUS2, 0x08U);
  RC522_WriteRegister(device, RC522_REG_BIT_FRAMING, 0x00U);
  RC522_ClearBitMask(device, RC522_REG_COLLISION, 0x80U);

  status = RC522_Communicate(device, RC522_COMMAND_TRANSCEIVE, command,
                             sizeof(command), response, sizeof(response),
                             &response_bits);
  RC522_SetBitMask(device, RC522_REG_COLLISION, 0x80U);
  if (status != RC522_STATUS_OK)
  {
    return status;
  }

  if (response_bits != 40U)
  {
    return RC522_STATUS_PROTOCOL_ERROR;
  }

  for (index = 0U; index < RC522_UID_SIZE; index++)
  {
    uid[index] = response[index];
    checksum ^= response[index];
  }

  return (checksum == response[RC522_UID_SIZE])
             ? RC522_STATUS_OK
             : RC522_STATUS_PROTOCOL_ERROR;
}

Rc522Status RC522_Select(Rc522 *device,
                         const uint8_t uid[RC522_UID_SIZE])
{
  uint8_t command[9] = {RC522_PICC_ANTICOLLISION, 0x70U};
  uint8_t response[3] = {0U};
  uint8_t index;
  uint32_t response_bits = 0U;
  Rc522Status status;

  if ((device == NULL) || !device->initialized || (uid == NULL))
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  for (index = 0U; index < RC522_UID_SIZE; index++)
  {
    command[index + 2U] = uid[index];
    command[6] ^= uid[index];
  }

  status = RC522_CalculateCrc(device, command, 7U, &command[7]);
  if (status != RC522_STATUS_OK)
  {
    return status;
  }

  RC522_ClearBitMask(device, RC522_REG_STATUS2, 0x08U);
  status = RC522_Communicate(device, RC522_COMMAND_TRANSCEIVE, command,
                             sizeof(command), response, sizeof(response),
                             &response_bits);
  if (status != RC522_STATUS_OK)
  {
    return status;
  }

  return (response_bits == 24U) ? RC522_STATUS_OK
                                : RC522_STATUS_PROTOCOL_ERROR;
}

Rc522Status RC522_Halt(Rc522 *device)
{
  uint8_t command[4] = {RC522_PICC_HALT, 0U, 0U, 0U};
  uint8_t response[1] = {0U};
  uint32_t response_bits = 0U;
  Rc522Status status;

  if ((device == NULL) || !device->initialized)
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  status = RC522_CalculateCrc(device, command, 2U, &command[2]);
  if (status != RC522_STATUS_OK)
  {
    return status;
  }

  status = RC522_Communicate(device, RC522_COMMAND_TRANSCEIVE, command,
                             sizeof(command), response, sizeof(response),
                             &response_bits);
  if ((status == RC522_STATUS_NO_CARD) || (status == RC522_STATUS_TIMEOUT))
  {
    return RC522_STATUS_OK;
  }

  return status;
}

Rc522Status RC522_ReadUid(Rc522 *device, uint8_t uid[RC522_UID_SIZE])
{
  uint8_t tag_type[RC522_TAG_TYPE_SIZE];
  Rc522Status status;

  if ((device == NULL) || !device->initialized || (uid == NULL))
  {
    return RC522_STATUS_INVALID_ARGUMENT;
  }

  status = RC522_Request(device, RC522_PICC_REQUEST_ALL, tag_type);
  if (status != RC522_STATUS_OK)
  {
    return status;
  }

  status = RC522_Anticollision(device, uid);
  if (status != RC522_STATUS_OK)
  {
    return status;
  }

  return RC522_Select(device, uid);
}

uint8_t RC522_ReadVersion(Rc522 *device)
{
  if ((device == NULL) || !device->initialized)
  {
    return 0U;
  }

  return RC522_ReadRegister(device, RC522_REG_VERSION);
}
