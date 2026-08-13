#include "ecu_app.h"

#include <stddef.h>

static bool ECU_App_QueuePush(uint8_t *queue, volatile uint8_t *head,
                              volatile uint8_t *tail, uint8_t value)
{
  const uint8_t next_head =
      (uint8_t)((*head + 1U) % ECU_APP_FORWARD_QUEUE_CAPACITY);

  if (next_head == *tail)
  {
    return false;
  }

  queue[*head] = value;
  *head = next_head;
  return true;
}

static bool ECU_App_QueuePeek(const uint8_t *queue, volatile uint8_t *head,
                              volatile uint8_t *tail, uint8_t *value)
{
  if (*tail == *head)
  {
    return false;
  }

  *value = queue[*tail];
  return true;
}

static void ECU_App_QueuePop(volatile uint8_t *tail)
{
  *tail = (uint8_t)((*tail + 1U) % ECU_APP_FORWARD_QUEUE_CAPACITY);
}

bool ECU_App_Init(EcuApp *app, const EcuAppIo *io)
{
  if ((app == NULL) || (io == NULL) || (io->start_receive == NULL) ||
      (io->start_external_receive == NULL) || (io->transmit_lora == NULL) ||
      (io->transmit_external == NULL) || (io->write_relay == NULL) ||
      (io->write_lora_mode == NULL))
  {
    return false;
  }

  ECU_Control_Init(&app->control);
  app->io = *io;
  app->lora_mode = ECU_LORA_MODE_NORMAL;
  app->rx_byte = 0U;
  app->external_rx_byte = 0U;
  app->to_lora_head = 0U;
  app->to_lora_tail = 0U;
  app->to_external_head = 0U;
  app->to_external_tail = 0U;
  app->receive_pending = false;
  app->external_receive_pending = false;
  app->receive_start_failure_count = 0U;
  app->receive_error_count = 0U;
  app->unexpected_receive_callback_count = 0U;
  app->external_receive_start_failure_count = 0U;
  app->external_receive_error_count = 0U;
  app->unexpected_external_receive_callback_count = 0U;
  app->lora_forwarded_byte_count = 0U;
  app->external_forwarded_byte_count = 0U;
  app->lora_queue_overflow_count = 0U;
  app->external_queue_overflow_count = 0U;
  app->lora_transmit_failure_count = 0U;
  app->external_transmit_failure_count = 0U;

  app->io.write_relay(app->io.context, false);
  return ECU_App_SetLoraMode(app, ECU_LORA_MODE_NORMAL);
}

bool ECU_App_StartExternalReceive(EcuApp *app)
{
  if (app == NULL)
  {
    return false;
  }

  if (app->external_receive_pending)
  {
    return true;
  }

  if (!app->io.start_external_receive(app->io.context,
                                      &app->external_rx_byte))
  {
    app->external_receive_start_failure_count++;
    return false;
  }

  app->external_receive_pending = true;
  return true;
}

bool ECU_App_StartReceive(EcuApp *app)
{
  if (app == NULL)
  {
    return false;
  }

  if (app->receive_pending)
  {
    return true;
  }

  if (!app->io.start_receive(app->io.context, &app->rx_byte))
  {
    app->receive_start_failure_count++;
    return false;
  }

  app->receive_pending = true;
  return true;
}

bool ECU_App_OnReceiveComplete(EcuApp *app)
{
  bool command_accepted;

  if (app == NULL)
  {
    return false;
  }

  if (!app->receive_pending)
  {
    app->unexpected_receive_callback_count++;
    return false;
  }

  app->receive_pending = false;
  command_accepted = ECU_Control_HandleByte(&app->control, app->rx_byte);
  app->io.write_relay(app->io.context,
                      ECU_Control_ShouldEnergizeRelay(&app->control));
  if (!ECU_App_QueuePush(app->to_external_queue, &app->to_external_head,
                         &app->to_external_tail, app->rx_byte))
  {
    app->external_queue_overflow_count++;
  }
  (void)ECU_App_StartReceive(app);
  return command_accepted;
}

bool ECU_App_OnExternalReceiveComplete(EcuApp *app)
{
  bool queued;

  if (app == NULL)
  {
    return false;
  }

  if (!app->external_receive_pending)
  {
    app->unexpected_external_receive_callback_count++;
    return false;
  }

  app->external_receive_pending = false;
  queued = ECU_App_QueuePush(app->to_lora_queue, &app->to_lora_head,
                             &app->to_lora_tail, app->external_rx_byte);
  if (!queued)
  {
    app->lora_queue_overflow_count++;
  }
  (void)ECU_App_StartExternalReceive(app);
  return queued;
}

bool ECU_App_OnReceiveError(EcuApp *app)
{
  if (app == NULL)
  {
    return false;
  }

  app->receive_error_count++;
  app->receive_pending = false;
  return ECU_App_StartReceive(app);
}

bool ECU_App_OnExternalReceiveError(EcuApp *app)
{
  if (app == NULL)
  {
    return false;
  }

  app->external_receive_error_count++;
  app->external_receive_pending = false;
  return ECU_App_StartExternalReceive(app);
}

void ECU_App_Process(EcuApp *app)
{
  uint8_t value;

  if (app == NULL)
  {
    return;
  }

  if (ECU_App_QueuePeek(app->to_lora_queue, &app->to_lora_head,
                        &app->to_lora_tail, &value))
  {
    if (app->io.transmit_lora(app->io.context, value))
    {
      ECU_App_QueuePop(&app->to_lora_tail);
      app->lora_forwarded_byte_count++;
    }
    else
    {
      app->lora_transmit_failure_count++;
    }
  }

  if (ECU_App_QueuePeek(app->to_external_queue, &app->to_external_head,
                        &app->to_external_tail, &value))
  {
    if (app->io.transmit_external(app->io.context, value))
    {
      ECU_App_QueuePop(&app->to_external_tail);
      app->external_forwarded_byte_count++;
    }
    else
    {
      app->external_transmit_failure_count++;
    }
  }
}

bool ECU_App_SetLoraMode(EcuApp *app, EcuLoraMode mode)
{
  bool md0;
  bool md1;

  if (app == NULL)
  {
    return false;
  }

  switch (mode)
  {
    case ECU_LORA_MODE_NORMAL:
      md0 = false;
      md1 = false;
      break;

    case ECU_LORA_MODE_CONFIG:
      md0 = true;
      md1 = true;
      break;

    default:
      return false;
  }

  app->io.write_lora_mode(app->io.context, md0, md1);
  app->lora_mode = mode;
  return true;
}
