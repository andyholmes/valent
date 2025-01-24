// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mousepad-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-mousepad-device.h"
#include "valent-mousepad-keydef.h"
#include "valent-mousepad-plugin.h"


struct _ValentMousepadPlugin
{
  ValentDevicePlugin    parent_instance;

  ValentInput          *input;
  ValentMousepadDevice *adapter;

  unsigned int          local_state : 1;
  unsigned int          remote_state : 1;
};

static void   valent_mousepad_plugin_send_echo (ValentMousepadPlugin *self,
                                                JsonNode             *packet);

G_DEFINE_FINAL_TYPE (ValentMousepadPlugin, valent_mousepad_plugin, VALENT_TYPE_DEVICE_PLUGIN)


static KeyModifierType
event_to_mask (JsonObject *body)
{
  KeyModifierType mask = 0;

  if (json_object_get_boolean_member_with_default (body, "alt", FALSE))
    mask |= KEYMOD_ALT_MASK;

  if (json_object_get_boolean_member_with_default (body, "ctrl", FALSE))
    mask |= KEYMOD_CONTROL_MASK;

  if (json_object_get_boolean_member_with_default (body, "shift", FALSE))
    mask |= KEYMOD_SHIFT_MASK;

  if (json_object_get_boolean_member_with_default (body, "super", FALSE))
    mask |= KEYMOD_SUPER_MASK;

  return mask;
}

static inline void
keyboard_mask (ValentInput  *input,
               unsigned int  mask,
               gboolean      lock)
{
  if (mask & KEYMOD_ALT_MASK)
    valent_input_keyboard_keysym (input, KEYSYM_Alt_L, lock);

  if (mask & KEYMOD_CONTROL_MASK)
    valent_input_keyboard_keysym (input, KEYSYM_Control_L, lock);

  if (mask & KEYMOD_SHIFT_MASK)
    valent_input_keyboard_keysym (input, KEYSYM_Shift_L, lock);

  if (mask & KEYMOD_SUPER_MASK)
    valent_input_keyboard_keysym (input, KEYSYM_Super_L, lock);
}

/*
 * Packet Handlers
 */
static void
valent_mousepad_plugin_handle_mousepad_request (ValentMousepadPlugin *self,
                                                JsonNode             *packet)
{
  JsonObject *body;
  const char *key;
  int64_t keycode;

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
  else if (valent_packet_get_string (packet, "key", &key))
    {
      KeyModifierType mask;
      const char *next;
      gunichar codepoint;

      /* Lock modifiers */
      if ((mask = event_to_mask (body)) != 0)
        keyboard_mask (self->input, mask, TRUE);

      /* Input each keysym */
      next = key;

      while ((codepoint = g_utf8_get_char (next)) != 0)
        {
          uint32_t keysym;

          keysym = valent_input_unicode_to_keysym (codepoint);
          valent_input_keyboard_keysym (self->input, keysym, TRUE);
          valent_input_keyboard_keysym (self->input, keysym, FALSE);

          next = g_utf8_next_char (next);
        }

      /* Unlock modifiers */
      if (mask != 0)
        keyboard_mask (self->input, mask, FALSE);

      /* Send ack, if requested */
      if (valent_packet_check_field (packet, "sendAck"))
        valent_mousepad_plugin_send_echo (self, packet);
    }
  else if (valent_packet_get_int (packet, "specialKey", &keycode))
    {
      KeyModifierType mask;
      uint32_t keysym;

      if ((keysym = valent_mousepad_keycode_to_keysym (keycode)) == 0)
        {
          g_debug ("%s(): expected \"specialKey\" field holding a keycode",
                   G_STRFUNC);
          return;
        }

      /* Lock modifiers */
      if ((mask = event_to_mask (body)) != 0)
        keyboard_mask (self->input, mask, TRUE);

      /* Input each keysym */
      valent_input_keyboard_keysym (self->input, keysym, TRUE);
      valent_input_keyboard_keysym (self->input, keysym, FALSE);

      /* Unlock modifiers */
      if (mask != 0)
        keyboard_mask (self->input, mask, FALSE);

      /* Send ack, if requested */
      if (valent_packet_check_field (packet, "sendAck"))
        valent_mousepad_plugin_send_echo (self, packet);
    }

  else if (valent_packet_check_field (packet, "singleclick"))
    {
      valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, TRUE);
      valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, FALSE);
    }

  else if (valent_packet_check_field (packet, "doubleclick"))
    {
      valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, TRUE);
      valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, FALSE);
      valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, TRUE);
      valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, FALSE);
    }

  else if (valent_packet_check_field (packet, "middleclick"))
    {
      valent_input_pointer_button (self->input, VALENT_POINTER_MIDDLE, TRUE);
      valent_input_pointer_button (self->input, VALENT_POINTER_MIDDLE, FALSE);
    }

  else if (valent_packet_check_field (packet, "rightclick"))
    {
      valent_input_pointer_button (self->input, VALENT_POINTER_SECONDARY, TRUE);
      valent_input_pointer_button (self->input, VALENT_POINTER_SECONDARY, FALSE);
    }

  else if (valent_packet_check_field (packet, "singlehold"))
    {
      valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, TRUE);
    }

  /* Not used by kdeconnect-android, hold is released with a regular click */
  else if (valent_packet_check_field (packet, "singlerelease"))
    {
      valent_input_pointer_button (self->input, VALENT_POINTER_PRIMARY, FALSE);
    }

  else
    {
      g_debug ("%s: unknown input", G_STRFUNC);
    }
}

static void
valent_mousepad_plugin_handle_mousepad_echo (ValentMousepadPlugin *self,
                                             JsonNode             *packet)
{
  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  VALENT_NOTE ("Not implemented");
}

static void
valent_mousepad_plugin_handle_mousepad_keyboardstate (ValentMousepadPlugin *self,
                                                      JsonNode             *packet)
{
  gboolean state;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  /* Update the remote keyboard state */
  if (!valent_packet_get_boolean (packet, "state", &state))
    {
      g_debug ("%s(): expected \"state\" field holding a boolean",
               G_STRFUNC);
      return;
    }

  if (self->remote_state != state)
    {
      GAction *action;

      self->remote_state = state;
      action = g_action_map_lookup_action (G_ACTION_MAP (self), "event");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), self->remote_state);

      if (self->remote_state && self->adapter == NULL)
        {
          ValentDevice *device = NULL;

          device = valent_resource_get_source (VALENT_RESOURCE (self));
          self->adapter = valent_mousepad_device_new (device);
          valent_component_export_adapter (VALENT_COMPONENT (self->input),
                                           VALENT_EXTENSION (self->adapter));
        }
      else if (!self->remote_state && self->adapter != NULL)
        {
          valent_component_unexport_adapter (VALENT_COMPONENT (self->input),
                                             VALENT_EXTENSION (self->adapter));
          g_clear_object (&self->adapter);
        }
    }
}

/*
 * Packet Providers
 */
static void
valent_mousepad_plugin_mousepad_request_keyboard (ValentMousepadPlugin *self,
                                                  uint32_t              keysym,
                                                  KeyModifierType       mask)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  uint32_t special_key = 0;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.mousepad.request");

  if ((special_key = valent_mousepad_keysym_to_keycode (keysym)) != 0)
    {
      json_builder_set_member_name (builder, "specialKey");
      json_builder_add_int_value (builder, special_key);
    }
  else
    {
      g_autoptr (GError) error = NULL;
      g_autofree char *key = NULL;
      gunichar wc;

      wc = valent_input_keysym_to_unicode (keysym);
      key = g_ucs4_to_utf8 (&wc, 1, NULL, NULL, &error);

      if (key == NULL)
        {
          g_warning ("Converting %u to string: %s", keysym, error->message);
          return;
        }

      json_builder_set_member_name (builder, "key");
      json_builder_add_string_value (builder, key);
    }

  if (mask & KEYMOD_ALT_MASK)
    {
      json_builder_set_member_name (builder, "alt");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if (mask & KEYMOD_CONTROL_MASK)
    {
      json_builder_set_member_name (builder, "ctrl");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if (mask & KEYMOD_SHIFT_MASK)
    {
      json_builder_set_member_name (builder, "shift");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if (mask & KEYMOD_SUPER_MASK)
    {
      json_builder_set_member_name (builder, "super");
      json_builder_add_boolean_value (builder, TRUE);
    }

  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_mousepad_plugin_mousepad_request_pointer (ValentMousepadPlugin *self,
                                                 double                dx,
                                                 double                dy,
                                                 gboolean              axis)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.mousepad.request");

  json_builder_set_member_name (builder, "dx");
  json_builder_add_double_value (builder, dx);
  json_builder_set_member_name (builder, "dy");
  json_builder_add_double_value (builder, dy);

  if (axis)
    {
      json_builder_set_member_name (builder, "scroll");
      json_builder_add_boolean_value (builder, TRUE);
    }

  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_mousepad_plugin_send_echo (ValentMousepadPlugin *self,
                                  JsonNode             *packet)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;
  JsonObjectIter iter;
  const char *name;
  JsonNode *node;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.mousepad.echo");

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

  response = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
}

static void
valent_mousepad_plugin_mousepad_keyboardstate (ValentMousepadPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_MOUSEPAD_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.mousepad.keyboardstate");
  json_builder_set_member_name (builder, "state");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/*
 * GActions
 */
static void
valent_mousepad_plugin_toggle_actions (ValentMousepadPlugin *self,
                                       gboolean              available)
{
  GAction *action;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "event");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                               available && self->remote_state);
}

static void
mousepad_event_action (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (user_data);
  GVariantDict dict;
  double dx, dy;
  uint32_t keysym;

  g_assert (VALENT_IS_MOUSEPAD_PLUGIN (self));
  g_return_if_fail (self->remote_state);

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
      KeyModifierType mask = 0;

      g_variant_dict_lookup (&dict, "mask", "u", &mask);
      valent_mousepad_plugin_mousepad_request_keyboard (self, keysym, mask);
    }
  else
    g_warning ("%s(): unknown event type", G_STRFUNC);

  g_variant_dict_clear (&dict);
}

static const GActionEntry actions[] = {
  {"event",  mousepad_event_action,  "a{sv}", NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
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
  else
    self->remote_state = FALSE;

  if (!self->remote_state && self->adapter != NULL)
    {
      valent_component_unexport_adapter (VALENT_COMPONENT (self->input),
                                         VALENT_EXTENSION (self->adapter));
      g_clear_object (&self->adapter);
    }

  valent_mousepad_plugin_toggle_actions (self, available);
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
  if (g_str_equal (type, "kdeconnect.mousepad.request"))
    valent_mousepad_plugin_handle_mousepad_request (self, packet);

  /* A confirmation of input we requested */
  else if (g_str_equal (type, "kdeconnect.mousepad.echo"))
    valent_mousepad_plugin_handle_mousepad_echo (self, packet);

  /* The remote keyboard is ready/not ready for input */
  else if (g_str_equal (type, "kdeconnect.mousepad.keyboardstate"))
    valent_mousepad_plugin_handle_mousepad_keyboardstate (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * ValentObject
 */
static void
valent_mousepad_plugin_destroy (ValentObject *object)
{
  ValentMousepadPlugin *self = VALENT_MOUSEPAD_PLUGIN (object);
  ValentComponent *component = NULL;

  if (self->adapter != NULL)
    {
      component = VALENT_COMPONENT (valent_input_get_default ());
      valent_component_unexport_adapter (component, VALENT_EXTENSION (self->adapter));
      valent_object_destroy (VALENT_OBJECT (self->adapter));
      g_clear_object (&self->adapter);
    }

  VALENT_OBJECT_CLASS (valent_mousepad_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_mousepad_plugin_constructed (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  G_OBJECT_CLASS (valent_mousepad_plugin_parent_class)->constructed (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
}

static void
valent_mousepad_plugin_class_init (ValentMousepadPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_mousepad_plugin_constructed;

  vobject_class->destroy = valent_mousepad_plugin_destroy;

  plugin_class->handle_packet = valent_mousepad_plugin_handle_packet;
  plugin_class->update_state = valent_mousepad_plugin_update_state;
}

static void
valent_mousepad_plugin_init (ValentMousepadPlugin *self)
{
  self->input = valent_input_get_default ();
}

