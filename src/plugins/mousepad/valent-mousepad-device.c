// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mousepad-device"

#include "config.h"

#include <gio/gio.h>
#include <valent.h>

#include "valent-mousepad-device.h"
#include "valent-mousepad-keydef.h"

#define DEFAULT_DOUBLE_CLICK_TIME (400)
#define DEFAULT_LONG_PRESS_TIME   (500)


struct _ValentMousepadDevice
{
  ValentInputAdapter  parent_instance;

  ValentDevice       *device;

  /* keyboard */
  GArray             *keyboard_keys;
  KeyModifierType     keyboard_modifiers;
  unsigned int        keyboard_flush_id;

  /* pointer */
  unsigned int        pointer_button;
  unsigned int        pointer_presses;
  unsigned int        pointer_releases;
  unsigned int        pointer_doubleclick_id;
  unsigned int        pointer_longpress_id;

  int                 double_click_time;
  int                 long_press_time;
};

G_DEFINE_FINAL_TYPE (ValentMousepadDevice, valent_mousepad_device, VALENT_TYPE_INPUT_ADAPTER)


/*
 * Keyboard
 */
static gboolean
valent_mousepad_device_keyboard_flush (gpointer data)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (data);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GString) key = NULL;
  uint32_t special_key = 0;
  unsigned int n_handled = 0;

  if (self->keyboard_keys->len == 0)
    {
      self->keyboard_flush_id = 0;
      return G_SOURCE_REMOVE;
    }

  for (unsigned int len = self->keyboard_keys->len; n_handled < len; n_handled++)
    {
      uint32_t *keysym = &g_array_index (self->keyboard_keys, uint32_t, n_handled);

      if ((special_key = valent_mousepad_keysym_to_keycode (*keysym)) != 0)
        {
          /* If there are keys to be sent, they need to be sent first */
          if (key != NULL)
            special_key = 0;

          /* Otherwise, we need to send the current key and modifiers */
          n_handled++;
          break;
        }
      else
        {
          gunichar wc = valent_input_keysym_to_unicode (*keysym);

          if (wc == 0)
            {
              g_debug ("%s(): failed to convert keysym \"%u\" to unicode",
                       G_STRFUNC, *keysym);
              continue;
            }

          if (key == NULL)
            key = g_string_new (NULL);

          g_string_append_unichar (key, wc);
        }
    }
  g_array_remove_range (self->keyboard_keys, 0, n_handled);

  /* Build the packet */
  valent_packet_init (&builder, "kdeconnect.mousepad.request");

  if (key != NULL)
    {
      json_builder_set_member_name (builder, "key");
      json_builder_add_string_value (builder, key->str);
    }
  else if (special_key != 0)
    {
      json_builder_set_member_name (builder, "specialKey");
      json_builder_add_int_value (builder, special_key);
    }

  /* Check our supported modifiers */
  if ((self->keyboard_modifiers & KEYMOD_ALT_MASK) != 0)
    {
      json_builder_set_member_name (builder, "alt");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if ((self->keyboard_modifiers & KEYMOD_CONTROL_MASK) != 0)
    {
      json_builder_set_member_name (builder, "ctrl");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if ((self->keyboard_modifiers & KEYMOD_SHIFT_MASK) != 0)
    {
      json_builder_set_member_name (builder, "shift");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if ((self->keyboard_modifiers & KEYMOD_SUPER_MASK) != 0)
    {
      json_builder_set_member_name (builder, "super");
      json_builder_add_boolean_value (builder, TRUE);
    }

  /* Request acknowledgment of the event (disabled until it can be received) */
#if 0
  json_builder_set_member_name (builder, "sendAck");
  json_builder_add_boolean_value (builder, TRUE);
#endif

  packet = valent_packet_end (&builder);
  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);

  /* Clear the source if there's nothing left queued */
  if (self->keyboard_keys->len == 0)
    {
      self->keyboard_flush_id = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static inline void
valent_mousepad_device_keyboard_reset (ValentMousepadDevice *self)
{
  g_assert (VALENT_IS_MOUSEPAD_DEVICE (self));

  g_array_remove_range (self->keyboard_keys, 0, self->keyboard_keys->len);
  g_clear_handle_id (&self->keyboard_flush_id, g_source_remove);
}

/*
 * Pointer
 */
static inline gboolean
valent_mousepad_device_pointer_reset (ValentMousepadDevice *self)
{
  self->pointer_button = 0;
  self->pointer_presses = 0;
  self->pointer_releases = 0;
  g_clear_handle_id (&self->pointer_doubleclick_id, g_source_remove);
  g_clear_handle_id (&self->pointer_longpress_id, g_source_remove);

  return G_SOURCE_REMOVE;
}

static inline gboolean
valent_mousepad_device_pointer_flush (gpointer data)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (data);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOUSEPAD_DEVICE (self));

  /* Ignore unpaired releases */
  if (self->pointer_presses < self->pointer_releases)
    return valent_mousepad_device_pointer_reset (self);

  if (self->pointer_presses == 1 && self->pointer_releases == 1)
    {
      valent_packet_init (&builder, "kdeconnect.mousepad.request");

      switch (self->pointer_button)
        {
        case VALENT_POINTER_PRIMARY:
          json_builder_set_member_name (builder, "singleclick");
          json_builder_add_boolean_value (builder, TRUE);
          break;

        case VALENT_POINTER_MIDDLE:
          json_builder_set_member_name (builder, "middleclick");
          json_builder_add_boolean_value (builder, TRUE);
          break;

        case VALENT_POINTER_SECONDARY:
          json_builder_set_member_name (builder, "rightclick");
          json_builder_add_boolean_value (builder, TRUE);
          break;

        default:
          g_debug ("%s: unknown pointer button %u",
                   G_STRFUNC,
                   self->pointer_button);
        }

      packet = valent_packet_end (&builder);
    }
  else if (self->pointer_button == VALENT_POINTER_PRIMARY && self->pointer_presses == 2)
    {
      valent_packet_init (&builder, "kdeconnect.mousepad.request");
      json_builder_set_member_name (builder, "doubleclick");
      json_builder_add_boolean_value (builder, TRUE);
      packet = valent_packet_end (&builder);
    }

  if (packet != NULL)
    {
      valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
      valent_mousepad_device_pointer_reset (self);
    }

  return G_SOURCE_REMOVE;
}

static inline gboolean
valent_mousepad_device_pointer_longpress (gpointer data)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (data);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOUSEPAD_DEVICE (self));

  valent_packet_init (&builder, "kdeconnect.mousepad.request");
  json_builder_set_member_name (builder, "singlehold");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);

  return valent_mousepad_device_pointer_reset (self);
}

/*
 * ValentInputAdapter
 */
static void
valent_mousepad_device_keyboard_keysym (ValentInputAdapter *adapter,
                                        uint32_t            keysym,
                                        gboolean            state)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (adapter);

  g_assert (VALENT_IS_MOUSEPAD_DEVICE (self));
  g_return_if_fail (keysym != 0);

  /* Track modifiers, but don't send anything */
  if (valent_input_keysym_to_modifier (keysym, state, &self->keyboard_modifiers))
    return;

  // TODO: the KDE Connect protocol doesn't support press and release states
  //       for keyboard input, so only key presses are sent. A solution might
  //       involve matching presses and releases, or an extant convention.
  if (!state)
    return;

  g_array_append_val (self->keyboard_keys, keysym);

  /* If there are modifiers set, the key should be sent immediately */
  if ((self->keyboard_modifiers & KEYMOD_ANY_MASK) != 0)
    {
      valent_mousepad_device_keyboard_flush (self);
      return;
    }

  /* Flush in an idle callback, in case key presses can be sent as a string */
  if (self->keyboard_flush_id == 0)
    self->keyboard_flush_id = g_idle_add (valent_mousepad_device_keyboard_flush,
                                          self);
}

static void
valent_mousepad_device_pointer_axis (ValentInputAdapter *adapter,
                                     double              dx,
                                     double              dy)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (adapter);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOUSEPAD_DEVICE (self));

  valent_packet_init (&builder, "kdeconnect.mousepad.request");
  json_builder_set_member_name (builder, "dx");
  json_builder_add_double_value (builder, dx);
  json_builder_set_member_name (builder, "dy");
  json_builder_add_double_value (builder, dy);
  json_builder_set_member_name (builder, "scroll");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static void
valent_mousepad_device_pointer_button (ValentInputAdapter *adapter,
                                       unsigned int        button,
                                       gboolean            state)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (adapter);

  if (self->pointer_button != button)
    {
      self->pointer_button = button;
      self->pointer_presses = 0;
      self->pointer_releases = 0;
    }

  if (state)
    {
      self->pointer_presses += 1;

      /* Any button press removes the double click timer; the event will either
       * be accepted or rejected based on the current button state. */
      g_clear_handle_id (&self->pointer_doubleclick_id, g_source_remove);
    }
  else
    {
      self->pointer_releases += 1;
    }

  /* Any button event removes the long press timer; the event is accepted if the
   * timeout elapses with the primary button being the only button pressed. */
  g_clear_handle_id (&self->pointer_longpress_id, g_source_remove);

  /* Handle the first press and release for the primary button, to prevent
   * flushing the double click state on the first release. */
  if (self->pointer_button == VALENT_POINTER_PRIMARY && self->pointer_presses == 1)
    {
      /* Double click and long press events both start with the press event */
      if (self->pointer_releases == 0)
        {
          // TODO: what if double-click time < long-press time?
          /* If the timeout elapses, a "singleclick" packet will be sent */
          self->pointer_doubleclick_id =
            g_timeout_add (self->double_click_time,
                           valent_mousepad_device_pointer_flush,
                           self);
          g_source_set_name_by_id (self->pointer_doubleclick_id,
                                   "valent_mousepad_device_pointer_flush");

          /* If the timeout elapses, a "singlehold" packet will be sent */
          self->pointer_longpress_id =
            g_timeout_add (self->long_press_time,
                           valent_mousepad_device_pointer_longpress,
                           self);
            g_source_set_name_by_id (self->pointer_longpress_id,
                                     "valent_mousepad_device_pointer_longpress");
        }
    }
  else
    {
      valent_mousepad_device_pointer_flush (self);
    }
}

static void
valent_mousepad_device_pointer_motion (ValentInputAdapter *adapter,
                                       double              dx,
                                       double              dy)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (adapter);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOUSEPAD_DEVICE (self));

  valent_packet_init (&builder, "kdeconnect.mousepad.request");
  json_builder_set_member_name (builder, "dx");
  json_builder_add_double_value (builder, dx);
  json_builder_set_member_name (builder, "dy");
  json_builder_add_double_value (builder, dy);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
  valent_mousepad_device_pointer_reset (self);
}

#if 0
static void
valent_mousepad_device_pointer_release (ValentInputRemote *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mousepad.request");
  json_builder_set_member_name (builder, "singlerelease");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}
#endif

static void
on_device_state_changed (ValentDevice         *device,
                         GParamSpec           *pspec,
                         ValentMousepadDevice *self)
{
#if 0
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  gboolean available;

  state = valent_device_get_state (device);
  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;
#endif
}

/*
 * ValentObject
 */
static void
valent_mousepad_device_destroy (ValentObject *object)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (object);

  valent_mousepad_device_keyboard_reset (self);
  valent_mousepad_device_pointer_reset (self);

  VALENT_OBJECT_CLASS (valent_mousepad_device_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_mousepad_device_constructed (GObject *object)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (object);

  G_OBJECT_CLASS (valent_mousepad_device_parent_class)->constructed (object);

  self->device = valent_object_get_parent (VALENT_OBJECT (self));
  g_signal_connect_object (self->device,
                           "notify::state",
                           G_CALLBACK (on_device_state_changed),
                           self,
                           G_CONNECT_DEFAULT);
}

static void
valent_mousepad_device_finalize (GObject *object)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (object);

  g_clear_pointer (&self->keyboard_keys, g_array_unref);

  G_OBJECT_CLASS (valent_mousepad_device_parent_class)->finalize (object);
}

static void
valent_mousepad_device_class_init (ValentMousepadDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentInputAdapterClass *input_class = VALENT_INPUT_ADAPTER_CLASS (klass);

  object_class->constructed = valent_mousepad_device_constructed;
  object_class->finalize = valent_mousepad_device_finalize;

  vobject_class->destroy = valent_mousepad_device_destroy;

  input_class->keyboard_keysym = valent_mousepad_device_keyboard_keysym;
  input_class->pointer_axis = valent_mousepad_device_pointer_axis;
  input_class->pointer_button = valent_mousepad_device_pointer_button;
  input_class->pointer_motion = valent_mousepad_device_pointer_motion;
}

static void
valent_mousepad_device_init (ValentMousepadDevice *self)
{
  self->keyboard_keys = g_array_new (FALSE, FALSE, sizeof (uint32_t));
  self->double_click_time = DEFAULT_DOUBLE_CLICK_TIME;
  self->long_press_time = DEFAULT_LONG_PRESS_TIME;
}

/**
 * valent_mousepad_device_new:
 * @device: a `ValentDevice`
 *
 * Get the `ValentMousepadDevice` instance.
 *
 * Returns: (transfer full) (nullable): a `ValentMousepadDevice`
 */
ValentMousepadDevice *
valent_mousepad_device_new (ValentDevice *device)
{
  g_autoptr (ValentContext) context = NULL;
  g_autofree char *iri = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  context = valent_context_new (valent_device_get_context (device),
                                "plugin",
                                "input");
  iri = tracker_sparql_escape_uri_printf ("urn:valent:input:%s",
                                          valent_device_get_id (device));
  return g_object_new (VALENT_TYPE_MOUSEPAD_DEVICE,
                       "iri",     iri,
                       "context", context,
                       "parent",  device,
                       NULL);
}

/**
 * valent_media_player_update_packet:
 * @player: a `ValentMousepadDevice`
 * @packet: a KDE Connect packet
 *
 * A convenience method for updating the internal state of the player from a
 * `kdeconnect.mousepad` packet.
 */
void
valent_mousepad_device_handle_packet (ValentMousepadDevice *player,
                                      JsonNode             *packet)
{
}
