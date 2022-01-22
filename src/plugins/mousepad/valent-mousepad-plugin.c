// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mousepad-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-input.h>

#include "valent-mousepad-dialog.h"
#include "valent-mousepad-keydef.h"
#include "valent-mousepad-plugin.h"


struct _ValentMousepadPlugin
{
  PeasExtensionBase     parent_instance;

  ValentDevice         *device;
  ValentInput          *input;

  ValentMousepadDialog *dialog;

  unsigned int          local_state : 1;
  unsigned int          remote_state : 1;
};

static void valent_mousepad_plugin_send_echo (ValentMousepadPlugin       *self,
                                              JsonNode                   *packet);

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMousepadPlugin, valent_mousepad_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


/**
 * event_to_mask:
 * @body: a #JsonObject
 *
 * Convert a mousepad event described by @body into a #GdkModifierType.
 *
 * Returns: #GdkModifierType
 */
static GdkModifierType
event_to_mask (JsonObject *body)
{
  GdkModifierType mask = 0;

  if (json_object_get_boolean_member_with_default (body, "alt", FALSE))
    mask |= GDK_ALT_MASK;

  if (json_object_get_boolean_member_with_default (body, "ctrl", FALSE))
    mask |= GDK_CONTROL_MASK;

  if (json_object_get_boolean_member_with_default (body, "shift", FALSE))
    mask |= GDK_SHIFT_MASK;

  if (json_object_get_boolean_member_with_default (body, "super", FALSE))
    mask |= GDK_SUPER_MASK;

  return mask;
}

/*
 * Packet Handlers
 */
static void
handle_mousepad_request (ValentMousepadPlugin *self,
                         JsonNode             *packet)
{
  JsonObject *body;
  gboolean has_key;
  gboolean has_special;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  /* Pointer movement */
  if (json_object_has_member (body, "dx") || json_object_has_member (body, "dy"))
    {
      double dx, dy;

      dx = json_object_get_double_member_with_default (body, "dx", 0.0);
      dy = json_object_get_double_member_with_default (body, "dy", 0.0);

      if (valent_packet_check_boolean (body, "scroll"))
        valent_input_pointer_axis (self->input, dx, dy);
      else
        valent_input_pointer_motion (self->input, dx, dy);
    }

  /* Keyboard Event */
  else if ((has_key = json_object_has_member (body, "key")) ||
           (has_special = json_object_has_member (body, "specialKey")))
    {
      GdkModifierType mask;
      unsigned int keyval;

      mask = event_to_mask (body);

      if (has_key)
        {
          const char *key;
          gunichar codepoint;

          key = json_object_get_string_member_with_default (body, "key", NULL);
          codepoint = g_utf8_get_char_validated (key, -1);
          keyval = gdk_unicode_to_keyval (codepoint);
          valent_input_keyboard_action (self->input, keyval, mask);
        }
      else if (has_special)
        {
          gint64 keycode;

          keycode = json_object_get_int_member_with_default (body, "specialKey", 0);

          if ((keyval = valent_mousepad_keycode_to_keyval (keycode)) != 0)
            valent_input_keyboard_action (self->input, keyval, mask);
        }

      if (json_object_get_boolean_member_with_default (body, "sendAck", FALSE))
        valent_mousepad_plugin_send_echo (self, packet);
    }

  else if (valent_packet_check_boolean (body, "singleclick"))
    valent_input_pointer_click (self->input, VALENT_POINTER_PRIMARY);

  else if (valent_packet_check_boolean (body, "doubleclick"))
    {
      valent_input_pointer_click (self->input, VALENT_POINTER_PRIMARY);
      valent_input_pointer_click (self->input, VALENT_POINTER_PRIMARY);
    }

  else if (valent_packet_check_boolean (body, "middleclick"))
    valent_input_pointer_click (self->input, VALENT_POINTER_MIDDLE);

  else if (valent_packet_check_boolean (body, "rightclick"))
    valent_input_pointer_click (self->input, VALENT_POINTER_SECONDARY);

  else if (valent_packet_check_boolean (body, "singlehold"))
    valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, TRUE);

  /* Not used by kdeconnect-android, hold is released with a regular click */
  else if (valent_packet_check_boolean (body, "singlerelease"))
    valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, FALSE);

  else
    g_debug ("%s: unknown input", G_STRFUNC);
}

static void
handle_mousepad_echo (ValentMousepadPlugin *self,
                      JsonNode             *packet)
{
  JsonObject *body;
  GdkModifierType mask = 0;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* There's no input dialog open, so we weren't expecting any echo */
  if G_UNLIKELY (self->dialog == NULL)
    {
      g_debug ("Unexpected echo");
      return;
    }

  body = valent_packet_get_body (packet);
  mask = event_to_mask (body);

  /* Backspace is effectively a printable character */
  if (json_object_has_member (body, "specialKey"))
    {
      gint64 keycode;
      unsigned int keyval;

      /* Ensure key is in range or we'll choke */
      keycode = json_object_get_int_member_with_default (body, "specialKey", 0);

      if ((keyval = valent_mousepad_keycode_to_keyval (keycode)) != 0)
        valent_mousepad_dialog_echo_special (self->dialog, keyval, mask);
    }

  /* A printable character */
  else if (json_object_has_member (body, "key"))
    {
      const char *key;

      key = json_object_get_string_member_with_default (body, "key", NULL);

      if (key != NULL)
        valent_mousepad_dialog_echo_key (self->dialog, key, mask);
    }
}

static void
handle_mousepad_keyboardstate (ValentMousepadPlugin *self,
                               JsonNode             *packet)
{
  JsonObject *body;
  gboolean state;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* Update the remote keyboard state */
  body = valent_packet_get_body (packet);
  state = valent_packet_check_boolean (body, "state");

  if (self->remote_state != state)
    {
      GActionGroup *actions;
      GAction *action;

      self->remote_state = state;
      actions = valent_device_get_actions (self->device);
      action = g_action_map_lookup_action (G_ACTION_MAP (actions), "mousepad-event");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), self->remote_state);
    }
}

/*
 * Packet Providers
 */
static void
valent_mousepad_plugin_mousepad_request_keyboard (ValentMousepadPlugin *self,
                                                  unsigned int          keyval,
                                                  GdkModifierType       mask)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  unsigned int special_key = 0;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.mousepad.request");

  if ((special_key = valent_mousepad_keyval_to_keycode (keyval)) != 0)
    {
      json_builder_set_member_name (builder, "specialKey");
      json_builder_add_int_value (builder, special_key);
    }
  else
    {
      g_autoptr (GError) error = NULL;
      g_autofree char *key = NULL;
      gunichar wc;

      wc = gdk_keyval_to_unicode (keyval);
      key = g_ucs4_to_utf8 (&wc, 1, NULL, NULL, &error);

      if (key == NULL)
        {
          g_warning ("Converting %s to string: %s",
                     gdk_keyval_name (keyval),
                     error->message);
          g_object_unref (builder);
          return;
        }

      json_builder_set_member_name (builder, "key");
      json_builder_add_string_value (builder, key);
    }

  if (mask & GDK_ALT_MASK)
    {
      json_builder_set_member_name (builder, "alt");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if (mask & GDK_CONTROL_MASK)
    {
      json_builder_set_member_name (builder, "ctrl");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if (mask & GDK_SHIFT_MASK)
    {
      json_builder_set_member_name (builder, "shift");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if (mask & GDK_SUPER_MASK)
    {
      json_builder_set_member_name (builder, "super");
      json_builder_add_boolean_value (builder, TRUE);
    }

  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_mousepad_plugin_mousepad_request_pointer (ValentMousepadPlugin *self,
                                                 double                dx,
                                                 double                dy,
                                                 gboolean              axis)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.mousepad.request");

  json_builder_set_member_name (builder, "dx");
  json_builder_add_double_value (builder, dx);
  json_builder_set_member_name (builder, "dy");
  json_builder_add_double_value (builder, dy);

  if (axis)
    {
      json_builder_set_member_name (builder, "scroll");
      json_builder_add_boolean_value (builder, TRUE);
    }

  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_mousepad_plugin_send_echo (ValentMousepadPlugin *self,
                                  JsonNode             *packet)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) response = NULL;
  JsonObjectIter iter;
  const char *name;
  JsonNode *node;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.mousepad.echo");

  json_object_iter_init (&iter, valent_packet_get_body (packet));

  while (json_object_iter_next (&iter, &name, &node))
    {
      if (g_strcmp0 (name, "sendAck") == 0)
        continue;

      json_builder_set_member_name (builder, name);
      json_builder_add_value (builder, json_node_ref (node));
    }

  json_builder_set_member_name (builder, "isAck");
  json_builder_add_boolean_value (builder, TRUE);

  response = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, response);
}

static void
valent_mousepad_plugin_mousepad_keyboardstate (ValentMousepadPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_MOUSEPAD_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.mousepad.keyboardstate");
  json_builder_set_member_name (builder, "state");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/*
 * GActions
 */
static void
dialog_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (user_data);

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  /* Create dialog if necessary */
  if (self->dialog == NULL)
    {
      self->dialog = valent_mousepad_dialog_new (self->device);
      g_object_add_weak_pointer (G_OBJECT (self->dialog),
                                 (gpointer) &self->dialog);
    }

  gtk_window_present_with_time (GTK_WINDOW (self->dialog),
                                GDK_CURRENT_TIME);
}

static void
event_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (user_data);
  GVariantDict dict;
  double dx, dy;
  unsigned int keysym;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  if (!self->remote_state)
    return;

  g_variant_dict_init (&dict, parameter);

  if (g_variant_dict_lookup (&dict, "dx", "d", &dx) &&
      g_variant_dict_lookup (&dict, "dy", "d", &dy))
    {
      gboolean scroll = FALSE;

      g_variant_dict_lookup (&dict, "scroll", "b", &scroll);
      valent_mousepad_plugin_mousepad_request_pointer (self, dx, dy, scroll);
    }
  else if (g_variant_dict_lookup (&dict, "keysym", "u", &keysym))
    {
      GdkModifierType mask = 0;

      g_variant_dict_lookup (&dict, "mask", "u", &mask);
      valent_mousepad_plugin_mousepad_request_keyboard (self, keysym, mask);
    }
  else
    g_warning ("%s(): unknown event type", G_STRFUNC);

  g_variant_dict_clear (&dict);
}

static const GActionEntry actions[] = {
  {"mousepad-dialog", dialog_action, NULL,    NULL, NULL},
  {"mousepad-event",  event_action,  "a{sv}", NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Remote Input"), "device.mousepad-dialog", "input-keyboard-symbolic"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_mousepad_plugin_enable (ValentDevicePlugin *plugin)
{
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_mousepad_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (plugin);

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  /* Destroy the input dialog if necessary */
  if (self->dialog != NULL)
    gtk_window_destroy (GTK_WINDOW (self->dialog));

  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));
}

static void
valent_mousepad_plugin_update_state (ValentDevicePlugin *plugin,
                                     ValentDeviceState   state)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  if (available)
    valent_mousepad_plugin_mousepad_keyboardstate (self);

  valent_device_plugin_toggle_actions (plugin,
                                       actions,
                                       G_N_ELEMENTS (actions),
                                       available);
}

static void
valent_mousepad_plugin_handle_packet (ValentDevicePlugin *plugin,
                                      const char         *type,
                                      JsonNode           *packet)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (plugin);

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  /* A request to simulate input */
  if (g_strcmp0 (type, "kdeconnect.mousepad.request") == 0)
    handle_mousepad_request (self, packet);

  /* A confirmation of input we requested */
  else if (g_strcmp0 (type, "kdeconnect.mousepad.echo") == 0)
    handle_mousepad_echo (self, packet);

  /* The remote keyboard is ready/not ready for input */
  else if (g_strcmp0 (type, "kdeconnect.mousepad.keyboardstate") == 0)
    handle_mousepad_keyboardstate (self, packet);

  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_mousepad_plugin_enable;
  iface->disable = valent_mousepad_plugin_disable;
  iface->handle_packet = valent_mousepad_plugin_handle_packet;
  iface->update_state = valent_mousepad_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_mousepad_plugin_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (object);

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
valent_mousepad_plugin_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mousepad_plugin_class_init (ValentMousepadPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_mousepad_plugin_get_property;
  object_class->set_property = valent_mousepad_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_mousepad_plugin_init (ValentMousepadPlugin *self)
{
  self->input = valent_input_get_default ();
}

