// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mousepad-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-input.h>

#include "valent-input-dialog.h"
#include "valent-mousepad-plugin.h"


struct _ValentMousepadPlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  ValentInput       *input;

  ValentInputDialog *input_dialog;

  unsigned int       local_state : 1;
  unsigned int       remote_state : 1;
  gint64             remote_state_id;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMousepadPlugin, valent_mousepad_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

static unsigned int special_keys[65536] = { 0, };


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

static void
init_special_keys (void)
{
  special_keys[GDK_KEY_BackSpace] =    1;
  special_keys[GDK_KEY_Tab] =          2;
  special_keys[GDK_KEY_Linefeed] =     3;
  special_keys[GDK_KEY_Left] =         4;
  special_keys[GDK_KEY_Up] =           5;
  special_keys[GDK_KEY_Right] =        6;
  special_keys[GDK_KEY_Down] =         7;
  special_keys[GDK_KEY_Page_Up] =      8;
  special_keys[GDK_KEY_Page_Down] =    9;
  special_keys[GDK_KEY_Home] =        10;
  special_keys[GDK_KEY_End] =         11;
  special_keys[GDK_KEY_Return] =      12;
  special_keys[GDK_KEY_Delete] =      13;
  special_keys[GDK_KEY_Escape] =      14;
  special_keys[GDK_KEY_Sys_Req] =     15;
  special_keys[GDK_KEY_Scroll_Lock] = 16;
  special_keys[GDK_KEY_F1] =          21;
  special_keys[GDK_KEY_F2] =          22;
  special_keys[GDK_KEY_F3] =          23;
  special_keys[GDK_KEY_F4] =          24;
  special_keys[GDK_KEY_F5] =          25;
  special_keys[GDK_KEY_F6] =          26;
  special_keys[GDK_KEY_F7] =          27;
  special_keys[GDK_KEY_F8] =          28;
  special_keys[GDK_KEY_F9] =          29;
  special_keys[GDK_KEY_F10] =         30;
  special_keys[GDK_KEY_F11] =         31;
  special_keys[GDK_KEY_F12] =         32;
}

static unsigned int
specialkey_to_keysym (gint64 specialkey)
{
  static const unsigned int keymap[] = {
    0,                   // 0 (Invalid)
    GDK_KEY_BackSpace,   // 1
    GDK_KEY_Tab,         // 2
    GDK_KEY_Linefeed,    // 3
    GDK_KEY_Left,        // 4
    GDK_KEY_Up,          // 5
    GDK_KEY_Right,       // 6
    GDK_KEY_Down,        // 7
    GDK_KEY_Page_Up,     // 8
    GDK_KEY_Page_Down,   // 9
    GDK_KEY_Home,        // 10
    GDK_KEY_End,         // 11
    GDK_KEY_Return,      // 12
    GDK_KEY_Delete,      // 13
    GDK_KEY_Escape,      // 14
    GDK_KEY_Sys_Req,     // 15
    GDK_KEY_Scroll_Lock, // 16
    0,                   // 17
    0,                   // 18
    0,                   // 19
    0,                   // 20
    GDK_KEY_F1,          // 21
    GDK_KEY_F2,          // 22
    GDK_KEY_F3,          // 23
    GDK_KEY_F4,          // 24
    GDK_KEY_F5,          // 25
    GDK_KEY_F6,          // 26
    GDK_KEY_F7,          // 27
    GDK_KEY_F8,          // 28
    GDK_KEY_F9,          // 29
    GDK_KEY_F10,         // 30
    GDK_KEY_F11,         // 31
    GDK_KEY_F12,         // 32
  };

  if (specialkey < 0 || specialkey > 32)
    return 0;

  return keymap[specialkey];
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

          if ((keyval = specialkey_to_keysym (keycode)) != 0)
            valent_input_keyboard_action (self->input, keyval, mask);
        }
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
  if G_UNLIKELY (self->input_dialog == NULL)
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

      if ((keyval = specialkey_to_keysym (keycode)) != 0)
        valent_input_dialog_echo_special (self->input_dialog, keyval, mask);
      else
        g_warning ("specialKey is out of range: %li", keycode);
    }

  /* A printable character */
  else if (json_object_has_member (body, "key"))
    {
      const char *key;

      key = json_object_get_string_member_with_default (body, "key", NULL);

      if (key != NULL)
        valent_input_dialog_echo_key (self->input_dialog, key, mask);
    }
}

static void
handle_mousepad_keyboardstate (ValentMousepadPlugin *self,
                               JsonNode             *packet)
{
  JsonObject *root;
  JsonObject *body;
  gint64 id;
  gboolean state;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  root = json_node_get_object (packet);
  id = json_object_get_int_member (root, "id");

  /* TODO: ensure we don't get packets out of order */
  if (id < self->remote_state_id)
    {
      g_debug ("%s: received keyboard state out of order", G_STRFUNC);
      return;
    }

  self->remote_state_id = id;

  /* Update the remote keyboard state */
  body = json_object_get_object_member (root, "body");
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
                                                  unsigned int          keysym,
                                                  GdkModifierType       mask)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  builder = valent_packet_start("kdeconnect.mousepad.request");

  if (keysym <= G_MAXUINT16 && special_keys[keysym] != 0)
    {
      json_builder_set_member_name (builder, "specialKey");
      json_builder_add_int_value (builder, special_keys[keysym]);
    }
  else
    {
      char key[6] = { 0, };

      g_unichar_to_utf8 (gdk_keyval_to_unicode (keysym), key);
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

  builder = valent_packet_start("kdeconnect.mousepad.request");

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
valent_mousepad_plugin_mousepad_echo (ValentMousepadPlugin *self,
                                      JsonNode             *packet)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) response = NULL;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  builder = valent_packet_start("kdeconnect.mousepad.echo");
  response = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, response);
}

static void
valent_mousepad_plugin_mousepad_keyboardstate (ValentMousepadPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_MOUSEPAD_PLUGIN (self));

  builder = valent_packet_start("kdeconnect.mousepad.keyboardstate");
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
  ValentMousepadPlugin *self = user_data;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  /* Create dialog if necessary */
  if (self->input_dialog == NULL)
    {
      self->input_dialog = valent_input_dialog_new (self);
      g_object_add_weak_pointer (G_OBJECT (self->input_dialog),
                                 (gpointer) &self->input_dialog);
    }

  gtk_window_present_with_time (GTK_WINDOW (self->input_dialog),
                                GDK_CURRENT_TIME);
}

static void
event_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  ValentMousepadPlugin *self = user_data;
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
    g_warning ("Unknown event type");
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
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (plugin);

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  /* Register GActions */
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));

  /* Register GMenu items */
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_mousepad_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (plugin);

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  /* Unregister GMenu items */
  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));

  if (self->input_dialog != NULL)
    gtk_window_destroy (GTK_WINDOW (self->input_dialog));
}

static void
valent_mousepad_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  if (available)
    valent_mousepad_plugin_mousepad_keyboardstate (self);

  /* GActions */
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
  init_special_keys ();
}

static void
valent_mousepad_plugin_init (ValentMousepadPlugin *self)
{
  self->input = valent_input_get_default ();
}

