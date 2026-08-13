#include "pcu_app.h"

#include <stddef.h>
#include <string.h>

static bool PCU_App_QueuePush(uint8_t *queue, volatile uint8_t *head,
                              volatile uint8_t *tail, uint8_t value)
{
  const uint8_t next_head =
      (uint8_t)((*head + 1U) % PCU_APP_FORWARD_QUEUE_CAPACITY);

  if (next_head == *tail)
  {
    return false;
  }

  queue[*head] = value;
  *head = next_head;
  return true;
}

static bool PCU_App_QueuePeek(const uint8_t *queue, volatile uint8_t *head,
                              volatile uint8_t *tail, uint8_t *value)
{
  if (*tail == *head)
  {
    return false;
  }

  *value = queue[*tail];
  return true;
}

static void PCU_App_QueuePop(volatile uint8_t *tail)
{
  *tail = (uint8_t)((*tail + 1U) % PCU_APP_FORWARD_QUEUE_CAPACITY);
}

bool PCU_App_Init(PcuApp *app, const PcuAppIo *io, bool estop_input_active)
{
  if ((app == NULL) || (io == NULL) ||
      (io->start_controller_receive == NULL) ||
      (io->start_lora_receive == NULL) ||
      (io->transmit_controller == NULL) || (io->transmit_lora == NULL))
  {
    return false;
  }

  memset(app, 0, sizeof(*app));
  app->io = *io;
  PCU_Estop_Init(&app->estop, estop_input_active);
  return true;
}

bool PCU_App_StartControllerReceive(PcuApp *app)
{
  if (app == NULL)
  {
    return false;
  }
  if (app->controller_receive_pending)
  {
    return true;
  }
  if (!app->io.start_controller_receive(app->io.context,
                                         &app->controller_rx_byte))
  {
    app->controller_receive_start_failure_count++;
    return false;
  }

  app->controller_receive_pending = true;
  return true;
}

bool PCU_App_StartLoraReceive(PcuApp *app)
{
  if (app == NULL)
  {
    return false;
  }
  if (app->lora_receive_pending)
  {
    return true;
  }
  if (!app->io.start_lora_receive(app->io.context, &app->lora_rx_byte))
  {
    app->lora_receive_start_failure_count++;
    return false;
  }

  app->lora_receive_pending = true;
  return true;
}

bool PCU_App_OnControllerReceiveComplete(PcuApp *app)
{
  bool queued = false;

  if (app == NULL)
  {
    return false;
  }
  if (!app->controller_receive_pending)
  {
    app->unexpected_controller_callback_count++;
    return false;
  }

  app->controller_receive_pending = false;
  if (app->controller_rx_byte == PCU_COMMAND_RESTORE)
  {
    uint8_t command;

    if (app->estop.input_active)
    {
      (void)PCU_Estop_RequestReset(&app->estop, &command);
    }
    else
    {
      queued = PCU_App_QueuePush(app->to_lora_queue, &app->to_lora_head,
                                 &app->to_lora_tail,
                                 app->controller_rx_byte);
      if (!queued)
      {
        app->to_lora_queue_overflow_count++;
      }
    }
  }
  else
  {
    queued = PCU_App_QueuePush(app->to_lora_queue, &app->to_lora_head,
                               &app->to_lora_tail,
                               app->controller_rx_byte);
  }

  if (!queued && (app->controller_rx_byte != PCU_COMMAND_RESTORE))
  {
    app->to_lora_queue_overflow_count++;
  }
  (void)PCU_App_StartControllerReceive(app);
  return queued;
}

bool PCU_App_OnLoraReceiveComplete(PcuApp *app)
{
  bool queued;

  if (app == NULL)
  {
    return false;
  }
  if (!app->lora_receive_pending)
  {
    app->unexpected_lora_callback_count++;
    return false;
  }

  app->lora_receive_pending = false;
  queued = PCU_App_QueuePush(app->to_controller_queue,
                             &app->to_controller_head,
                             &app->to_controller_tail, app->lora_rx_byte);
  if (!queued)
  {
    app->to_controller_queue_overflow_count++;
  }
  (void)PCU_App_StartLoraReceive(app);
  return queued;
}

bool PCU_App_OnControllerReceiveError(PcuApp *app)
{
  if (app == NULL)
  {
    return false;
  }

  app->controller_receive_error_count++;
  app->controller_receive_pending = false;
  return PCU_App_StartControllerReceive(app);
}

bool PCU_App_OnLoraReceiveError(PcuApp *app)
{
  if (app == NULL)
  {
    return false;
  }

  app->lora_receive_error_count++;
  app->lora_receive_pending = false;
  return PCU_App_StartLoraReceive(app);
}

bool PCU_App_UpdateEstopInput(PcuApp *app, bool input_active)
{
  bool changed;

  if (app == NULL)
  {
    return false;
  }

  changed = app->estop.input_active != input_active;
  (void)PCU_Estop_UpdateInput(&app->estop, input_active);
  if (changed)
  {
    app->estop_input_transition_count++;
  }
  return changed;
}

void PCU_App_Process(PcuApp *app, uint32_t tick_ms)
{
  uint8_t value;

  if (app == NULL)
  {
    return;
  }

  if (PCU_Estop_Process(&app->estop, tick_ms, &value))
  {
    if (app->io.transmit_lora(app->io.context, value))
    {
      app->local_estop_transmitted_count++;
    }
    else
    {
      app->local_estop_transmit_failure_count++;
    }
  }

  if (PCU_App_QueuePeek(app->to_lora_queue, &app->to_lora_head,
                        &app->to_lora_tail, &value))
  {
    if ((value == PCU_COMMAND_RESTORE) && app->estop.input_active)
    {
      uint8_t command;

      (void)PCU_Estop_RequestReset(&app->estop, &command);
      PCU_App_QueuePop(&app->to_lora_tail);
    }
    else if (app->io.transmit_lora(app->io.context, value))
    {
      PCU_App_QueuePop(&app->to_lora_tail);
      app->controller_to_lora_forwarded_count++;
      if (value == PCU_COMMAND_RESTORE)
      {
        uint8_t command;

        (void)PCU_Estop_RequestReset(&app->estop, &command);
      }
    }
    else
    {
      app->lora_transmit_failure_count++;
    }
  }

  if (PCU_App_QueuePeek(app->to_controller_queue, &app->to_controller_head,
                        &app->to_controller_tail, &value))
  {
    if (app->io.transmit_controller(app->io.context, value))
    {
      PCU_App_QueuePop(&app->to_controller_tail);
      app->lora_to_controller_forwarded_count++;
    }
    else
    {
      app->controller_transmit_failure_count++;
    }
  }
}
