// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mousepad-device"

#include "config.h"

#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-mousepad-device.h"
#include "valent-mousepad-keydef.h"

#define DEFAULT_DOUBLE_CLICK_TIME (400)
#define DEFAULT_LONG_PRESS_TIME   (500)


struct _ValentMousepadDevice
{
  ValentInputAdapter  parent_instance;

  ValentDevice       *device;
  GtkSettings        *settings;

  /* keyboard */
  GArray             *keyboard_keys;
  GdkModifierType     keyboard_modifiers;
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


enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * Keyboard
 */
static inline gboolean
valent_mousepad_device_update_modifiers (ValentMousepadDevice *self,
                                         uint32_t              keysym,
                                         gboolean              state)
{
  switch (keysym)
    {
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
      self->keyboard_modifiers = state
        ? self->keyboard_modifiers | GDK_ALT_MASK
        : self->keyboard_modifiers & ~GDK_ALT_MASK;
      return TRUE;

    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
      self->keyboard_modifiers = state
        ? self->keyboard_modifiers | GDK_CONTROL_MASK
        : self->keyboard_modifiers & ~GDK_CONTROL_MASK;
      return TRUE;

    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
      self->keyboard_modifiers = state
        ? self->keyboard_modifiers | GDK_SHIFT_MASK
        : self->keyboard_modifiers & ~GDK_SHIFT_MASK;
      return TRUE;

    case GDK_KEY_Super_L:
    case GDK_KEY_Super_R:
      self->keyboard_modifiers = state
        ? self->keyboard_modifiers | GDK_SUPER_MASK
        : self->keyboard_modifiers & ~GDK_SUPER_MASK;
      return TRUE;

    /* Return TRUE for known modifiers, even if unsupported */
    case GDK_KEY_Overlay1_Enable:
    case GDK_KEY_Overlay2_Enable:
    case GDK_KEY_Caps_Lock:
    case GDK_KEY_Shift_Lock:
    case GDK_KEY_Meta_L:
    case GDK_KEY_Meta_R:
    case GDK_KEY_Hyper_L:
    case GDK_KEY_Hyper_R:
    case GDK_KEY_Mode_switch:
    case GDK_KEY_ISO_Level3_Shift:
    case GDK_KEY_ISO_Level3_Latch:
    case GDK_KEY_ISO_Level5_Shift:
    case GDK_KEY_ISO_Level5_Latch:
      VALENT_NOTE ("skipping %s", gdk_keyval_name (keysym));
      return TRUE;

    default:
      return FALSE;
    }
}

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

      if ((special_key = valent_mousepad_keyval_to_keycode (*keysym)) != 0)
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
          gunichar wc = gdk_keyval_to_unicode (*keysym);

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
  if ((self->keyboard_modifiers & GDK_ALT_MASK) != 0)
    {
      json_builder_set_member_name (builder, "alt");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if ((self->keyboard_modifiers & GDK_CONTROL_MASK) != 0)
    {
      json_builder_set_member_name (builder, "ctrl");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if ((self->keyboard_modifiers & GDK_SHIFT_MASK) != 0)
    {
      json_builder_set_member_name (builder, "shift");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if ((self->keyboard_modifiers & GDK_SUPER_MASK) != 0)
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

static void
on_pointer_settings_changed (GtkSettings          *settings,
                             GParamSpec           *pspec,
                             ValentMousepadDevice *self)
{
  g_assert (VALENT_IS_MOUSEPAD_DEVICE (self));

  g_object_get (settings,
                "gtk-double-click-time", &self->double_click_time,
                "gtk-long-press-time",   &self->long_press_time,
                NULL);
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
  if (valent_mousepad_device_update_modifiers (self, keysym, state))
    return;

  // TODO: the KDE Connect protocol doesn't support press and release states
  //       for keyboard input, so only key presses are sent. A solution might
  //       involve matching presses and releases, or an extant convention.
  if (!state)
    return;

  g_array_append_val (self->keyboard_keys, keysym);

  /* If there are modifiers set, the key should be sent immediately */
  if ((self->keyboard_modifiers & GDK_MODIFIER_MASK) != 0)
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

/*
 * ValentObject
 */
static void
valent_mousepad_device_destroy (ValentObject *object)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (object);

  if (self->settings != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->settings, self);
      self->settings = NULL;
    }

  valent_mousepad_device_keyboard_reset (self);
  valent_mousepad_device_pointer_reset (self);

  VALENT_OBJECT_CLASS (valent_mousepad_device_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_mousepad_device_finalize (GObject *object)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (object);

  g_clear_object (&self->device);
  g_clear_pointer (&self->keyboard_keys, g_array_unref);

  G_OBJECT_CLASS (valent_mousepad_device_parent_class)->finalize (object);
}

static void
valent_mousepad_device_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mousepad_device_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ValentMousepadDevice *self = VALENT_MOUSEPAD_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mousepad_device_class_init (ValentMousepadDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentInputAdapterClass *input_class = VALENT_INPUT_ADAPTER_CLASS (klass);
  object_class->finalize = valent_mousepad_device_finalize;
  object_class->get_property = valent_mousepad_device_get_property;
  object_class->set_property = valent_mousepad_device_set_property;

  vobject_class->destroy = valent_mousepad_device_destroy;

  input_class->keyboard_keysym = valent_mousepad_device_keyboard_keysym;
  input_class->pointer_axis = valent_mousepad_device_pointer_axis;
  input_class->pointer_button = valent_mousepad_device_pointer_button;
  input_class->pointer_motion = valent_mousepad_device_pointer_motion;

  /**
   * ValentMousepadDevice:device:
   *
   * The [class@Valent.Device] this controller is for.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mousepad_device_init (ValentMousepadDevice *self)
{
  self->keyboard_keys = g_array_new (FALSE, FALSE, sizeof (uint32_t));
  self->double_click_time = DEFAULT_DOUBLE_CLICK_TIME;
  self->long_press_time = DEFAULT_LONG_PRESS_TIME;

  if (gtk_is_initialized ())
    {
      self->settings = gtk_settings_get_default ();
      g_signal_connect_object (self->settings,
                               "notify::gtk-double-click-time",
                               G_CALLBACK (on_pointer_settings_changed),
                               self, 0);
      on_pointer_settings_changed (self->settings, NULL, self);
    }
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
  return g_object_new (VALENT_TYPE_MOUSEPAD_DEVICE,
                       "device", device,
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
