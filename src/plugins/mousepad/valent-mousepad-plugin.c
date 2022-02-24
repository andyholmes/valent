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
  ValentDevicePlugin     parent_instance;

  ValentInput          *input;

  ValentMousepadDialog *dialog;

  unsigned int          local_state : 1;
  unsigned int          remote_state : 1;
};

static void valent_mousepad_plugin_send_echo (ValentMousepadPlugin       *self,
                                              JsonNode                   *packet);

G_DEFINE_TYPE (ValentMousepadPlugin, valent_mousepad_plugin, VALENT_TYPE_DEVICE_PLUGIN)


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
  const char *key;
  gint64 keycode;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  /* Pointer movement */
  if (json_object_has_member (body, "dx") || json_object_has_member (body, "dy"))
    {
      double dx, dy;

      dx = json_object_get_double_member_with_default (body, "dx", 0.0);
      dy = json_object_get_double_member_with_default (body, "dy", 0.0);

      if (valent_packet_check_field (packet, "scroll"))
        valent_input_pointer_axis (self->input, dx, dy);
      else
        valent_input_pointer_motion (self->input, dx, dy);
    }

  /* Keyboard Event */
  else if ((has_key = valent_packet_get_string (packet, "key", &key)) ||
           (has_special = valent_packet_get_int (packet, "specialKey", &keycode)))
    {
      GdkModifierType mask;
      unsigned int keyval;

      mask = event_to_mask (body);

      if (has_key)
        {
          gunichar codepoint;

          codepoint = g_utf8_get_char_validated (key, -1);
          keyval = gdk_unicode_to_keyval (codepoint);
          valent_input_keyboard_action (self->input, keyval, mask);
        }
      else if (has_special)
        {
          if ((keyval = valent_mousepad_keycode_to_keyval (keycode)) != 0)
            valent_input_keyboard_action (self->input, keyval, mask);
        }

      if (valent_packet_check_field (packet, "sendAck"))
        valent_mousepad_plugin_send_echo (self, packet);
    }

  else if (valent_packet_check_field (packet, "singleclick"))
    valent_input_pointer_click (self->input, VALENT_POINTER_PRIMARY);

  else if (valent_packet_check_field (packet, "doubleclick"))
    {
      valent_input_pointer_click (self->input, VALENT_POINTER_PRIMARY);
      valent_input_pointer_click (self->input, VALENT_POINTER_PRIMARY);
    }

  else if (valent_packet_check_field (packet, "middleclick"))
    valent_input_pointer_click (self->input, VALENT_POINTER_MIDDLE);

  else if (valent_packet_check_field (packet, "rightclick"))
    valent_input_pointer_click (self->input, VALENT_POINTER_SECONDARY);

  else if (valent_packet_check_field (packet, "singlehold"))
    valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, TRUE);

  /* Not used by kdeconnect-android, hold is released with a regular click */
  else if (valent_packet_check_field (packet, "singlerelease"))
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
  const char *key;
  gint64 keycode;

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
  if (valent_packet_get_int (packet, "specialKey", &keycode))
    {
      unsigned int keyval;

      /* Ensure key is in range or we'll choke */
      if ((keyval = valent_mousepad_keycode_to_keyval (keycode)) != 0)
        valent_mousepad_dialog_echo_special (self->dialog, keyval, mask);
    }

  /* A printable character */
  else if (valent_packet_get_string (packet, "key", &key))
    {
      valent_mousepad_dialog_echo_key (self->dialog, key, mask);
    }
}

static void
handle_mousepad_keyboardstate (ValentMousepadPlugin *self,
                               JsonNode             *packet)
{
  gboolean state;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* Update the remote keyboard state */
  if (!valent_packet_get_boolean (packet, "state", &state))
    {
      g_warning ("%s(): expected \"state\" field holding a boolean", G_STRFUNC);
      return;
    }

  if (self->remote_state != state)
    {
      GAction *action;

      self->remote_state = state;
      action = g_action_map_lookup_action (G_ACTION_MAP (self), "event");
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

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
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

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
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

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
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

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
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
      ValentDevice *device;

      device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
      self->dialog = valent_mousepad_dialog_new (device);
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
  {"dialog", dialog_action, NULL,    NULL, NULL},
  {"event",  event_action,  "a{sv}", NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Remote Input"), "device.mousepad.dialog", "input-keyboard-symbolic"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_mousepad_plugin_enable (ValentDevicePlugin *plugin)
{
  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
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

  valent_device_plugin_toggle_actions (plugin, available);
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

/*
 * GObject
 */
static void
valent_mousepad_plugin_class_init (ValentMousepadPluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_mousepad_plugin_enable;
  plugin_class->disable = valent_mousepad_plugin_disable;
  plugin_class->handle_packet = valent_mousepad_plugin_handle_packet;
  plugin_class->update_state = valent_mousepad_plugin_update_state;
}

static void
valent_mousepad_plugin_init (ValentMousepadPlugin *self)
{
  self->input = valent_input_get_default ();
}

