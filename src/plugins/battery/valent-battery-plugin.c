// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-battery-plugin"

#include "config.h"

#include <math.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <valent.h>

#include "valent-battery.h"
#include "valent-battery-plugin.h"

/* Defaults are 90m charge, 1d discharge (seconds/percent) */
#define DEFAULT_CHARGE_RATE    (90*60/100)
#define DEFAULT_DISCHARGE_RATE (24*60*60/100)


struct _ValentBatteryPlugin
{
  ValentDevicePlugin  parent_instance;

  /* Local Battery */
  ValentBattery      *battery;
  unsigned int        battery_watch : 1;

  /* Remote Battery */
  gboolean            charging;
  const char         *icon_name;
  gboolean            is_present;
  double              percentage;
  int64_t             time_to_full;
  int64_t             time_to_empty;
  int64_t             charge_rate;
  int64_t             discharge_rate;
  int64_t             timestamp;
};

static const char * valent_battery_plugin_get_icon_name (ValentBatteryPlugin *self);
static void         valent_battery_plugin_send_state    (ValentBatteryPlugin *self);

G_DEFINE_FINAL_TYPE (ValentBatteryPlugin, valent_battery_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/*
 * Local Battery
 */
static void
on_battery_changed (ValentBattery       *battery,
                    ValentBatteryPlugin *self)
{
  g_assert (VALENT_IS_BATTERY (battery));
  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  valent_battery_plugin_send_state (self);
}

static void
valent_battery_plugin_watch_battery (ValentBatteryPlugin *self,
                                     gboolean             watch)
{
  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  if (self->battery_watch == watch)
    return;

  if (self->battery == NULL)
    self->battery = valent_battery_get_default ();

  if (watch)
    {
      g_signal_connect_object (self->battery,
                               "changed",
                               G_CALLBACK (on_battery_changed),
                               self, 0);
      on_battery_changed (self->battery, self);
      self->battery_watch = TRUE;
    }
  else
    {
      g_signal_handlers_disconnect_by_data (self->battery, self);
      self->battery_watch = FALSE;
    }
}

static void
valent_battery_plugin_send_state (ValentBatteryPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  GSettings *settings;
  int current_charge;
  gboolean is_charging;
  unsigned int threshold_event;

  g_return_if_fail (VALENT_IS_BATTERY_PLUGIN (self));

  if (!valent_battery_is_present (self->battery))
    return;

  /* If the level is zero or less it's probably bogus, so send nothing */
  if (valent_battery_current_charge (self->battery) <= 0)
    return;

  settings = valent_extension_get_settings (VALENT_EXTENSION (self));

  if (!g_settings_get_boolean (settings, "share-state"))
    return;

  current_charge = valent_battery_current_charge (self->battery);
  is_charging = valent_battery_is_charging (self->battery);
  threshold_event = valent_battery_threshold_event (self->battery);

  valent_packet_init (&builder, "kdeconnect.battery");
  json_builder_set_member_name (builder, "currentCharge");
  json_builder_add_int_value (builder, current_charge);
  json_builder_set_member_name (builder, "isCharging");
  json_builder_add_boolean_value (builder, is_charging);
  json_builder_set_member_name (builder, "thresholdEvent");
  json_builder_add_int_value (builder, threshold_event);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}


/*
 * Remote Battery
 */
static const char *
valent_battery_plugin_get_icon_name (ValentBatteryPlugin *self)
{
  if (!self->is_present)
    return "battery-missing-symbolic";

  if (self->percentage >= 100.0)
    return "battery-full-charged-symbolic";

  if (self->percentage < 5.0)
    return self->charging
      ? "battery-empty-charging-symbolic"
      : "battery-empty-symbolic";

  if (self->percentage < 20.0)
    return self->charging
      ? "battery-caution-charging-symbolic"
      : "battery-caution-symbolic";

  if (self->percentage < 30.0)
    return self->charging
      ? "battery-low-charging-symbolic"
      : "battery-low-symbolic";

  if (self->percentage < 60.0)
    return self->charging
      ? "battery-good-charging-symbolic"
      : "battery-good-symbolic";

  return self->charging
    ? "battery-full-charging-symbolic"
    : "battery-full-symbolic";
}

static void
valent_battery_plugin_update_estimate (ValentBatteryPlugin *self,
                                       int64_t              current_charge,
                                       gboolean             is_charging)
{
  double rate;
  double percentage;
  double timestamp;

  g_return_if_fail (current_charge >= 0);

  percentage = CLAMP (current_charge, 0.0, 100.0);
  timestamp = floor (valent_timestamp_ms () / 1000);
  rate = is_charging ? self->charge_rate : self->discharge_rate;

  if (self->is_present)
    {
      double percentage_delta;
      double timestamp_delta;
      double new_rate;

      percentage_delta = ABS (percentage - self->percentage);
      timestamp_delta = timestamp - self->timestamp;

      if (percentage_delta > 0 && timestamp_delta > 0)
        {
          new_rate = timestamp_delta / percentage_delta;
          rate = floor ((rate * 0.4) + (new_rate * 0.6));
        }
    }

  if (is_charging)
    {
      self->charge_rate = (int64_t)rate;
      self->time_to_empty = 0;
      self->time_to_full = (int64_t)floor (self->charge_rate * (100.0 - percentage));
    }
  else
    {
      self->discharge_rate = (int64_t)rate;
      self->time_to_empty = (int64_t)floor (self->discharge_rate * percentage);
      self->time_to_full = 0;
    }
  self->timestamp = (int64_t)timestamp;
}

static void
valent_battery_plugin_update_gaction (ValentBatteryPlugin *self)
{
  GVariantDict dict;
  GVariant *state;
  GAction *action;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "charging", "b", self->charging);
  g_variant_dict_insert (&dict, "percentage", "d", self->percentage);
  g_variant_dict_insert (&dict, "icon-name", "s", self->icon_name);
  g_variant_dict_insert (&dict, "is-present", "b", self->is_present);
  g_variant_dict_insert (&dict, "time-to-empty", "x", self->time_to_empty);
  g_variant_dict_insert (&dict, "time-to-full", "x", self->time_to_full);
  state = g_variant_dict_end (&dict);

  /* Update the state, even if we're disabling the action */
  action = g_action_map_lookup_action (G_ACTION_MAP (self), "state");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), self->is_present);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), state);
}

static void
valent_battery_plugin_update_notification (ValentBatteryPlugin *self,
                                           int                  threshold_event)
{
  g_autoptr (GNotification) notification = NULL;
  g_autofree char *title = NULL;
  g_autofree char *body = NULL;
  g_autoptr (GIcon) icon = NULL;
  ValentDevice *device;
  const char *device_name;
  GSettings *settings;
  double full, low;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  device_name = valent_device_get_name (device);
  settings = valent_extension_get_settings (VALENT_EXTENSION (self));

  full = g_settings_get_double (settings, "full-notification-level");
  low = g_settings_get_double (settings, "low-notification-level");

  if (self->percentage >= full)
    {
      if (!g_settings_get_boolean (settings, "full-notification"))
        return;

      /* TRANSLATORS: This is <device name>: Fully Charged */
      title = g_strdup_printf (_("%s: Fully Charged"), device_name);
      /* TRANSLATORS: When the battery level is at maximum */
      body = g_strdup (_("Battery Fully Charged"));
      icon = g_themed_icon_new ("battery-full-charged-symbolic");
    }

  /* Battery is no longer low or is charging */
  else if (self->percentage > low || self->charging)
    {
      valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self),
                                              "battery-level");
      return;
    }

  /* Battery is now low */
  else if (self->percentage <= low || threshold_event == 1)
    {
      unsigned int total_minutes;
      unsigned int minutes;
      unsigned int hours;

      if (!g_settings_get_boolean (settings, "low-notification"))
        return;

      total_minutes = (unsigned int)floor (self->time_to_empty / 60);
      minutes = total_minutes % 60;
      hours = (unsigned int)floor (total_minutes / 60);

      /* TRANSLATORS: This is <device name>: Battery Low */
      title = g_strdup_printf (_("%s: Battery Low"), device_name);
      /* TRANSLATORS: This is <percentage> (<hours>:<minutes> Remaining) */
      body = g_strdup_printf (_("%g%% (%d∶%02d Remaining)"),
                              self->percentage, hours, minutes);
      icon = g_themed_icon_new ("battery-caution-symbolic");
    }

  notification = g_notification_new (title);
  g_notification_set_body (notification, body);
  g_notification_set_icon (notification, icon);

  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          "battery-level",
                                          notification);
}

static void
valent_battery_plugin_handle_battery (ValentBatteryPlugin *self,
                                      JsonNode            *packet)
{
  gboolean is_charging;
  int64_t current_charge;
  int64_t threshold_event;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_boolean (packet, "isCharging", &is_charging))
    is_charging = self->charging;

  if (!valent_packet_get_int (packet, "currentCharge", &current_charge))
    current_charge = (int64_t)self->percentage;

  if (!valent_packet_get_int (packet, "thresholdEvent", &threshold_event))
    threshold_event = 0;

  /* We get a lot of battery updates, so check if something changed */
  if (self->charging == is_charging &&
      G_APPROX_VALUE (self->percentage, current_charge, 0.1))
    return;

  /* If `current_charge` is `-1`, either there is no battery or statistics are
   * unavailable. Otherwise update the estimate before the instance properties
   * so that the time/percentage deltas can be calculated. */
  if (current_charge >= 0)
    valent_battery_plugin_update_estimate (self, current_charge, is_charging);

  self->charging = is_charging;
  self->percentage = CLAMP (current_charge, 0.0, 100.0);
  self->is_present = current_charge >= 0;
  self->icon_name = valent_battery_plugin_get_icon_name (self);

  valent_battery_plugin_update_gaction (self);
  valent_battery_plugin_update_notification (self, threshold_event);
}

/*
 * GActions
 */
static void
battery_state_action (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  // No-op to make the state read-only
}

static const GActionEntry actions[] = {
    {"state",  NULL, NULL, "@a{sv} {}", battery_state_action},
};

/*
 * ValentDevicePlugin
 */
static void
valent_battery_plugin_update_state (ValentDevicePlugin *plugin,
                                    ValentDeviceState   state)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  if (available)
    {
      valent_battery_plugin_update_gaction (self);
      valent_battery_plugin_watch_battery (self, TRUE);
    }
  else
    {
      valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);
      valent_battery_plugin_watch_battery (self, FALSE);
    }
}

static void
valent_battery_plugin_handle_packet (ValentDevicePlugin *plugin,
                                     const char         *type,
                                     JsonNode           *packet)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (plugin);

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  /* The remote battery state changed */
  if (g_str_equal (type, "kdeconnect.battery"))
    valent_battery_plugin_handle_battery (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * ValentObject
 */
static void
valent_battery_plugin_destroy (ValentObject *object)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (object);

  valent_battery_plugin_watch_battery (self, FALSE);

  VALENT_OBJECT_CLASS (valent_battery_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_battery_plugin_constructed (GObject *object)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (object);

  G_OBJECT_CLASS (valent_battery_plugin_parent_class)->constructed (object);

  g_action_map_add_action_entries (G_ACTION_MAP (object),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   object);
  valent_battery_plugin_update_gaction (self);
}

static void
valent_battery_plugin_class_init (ValentBatteryPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_battery_plugin_constructed;

  vobject_class->destroy = valent_battery_plugin_destroy;

  plugin_class->handle_packet = valent_battery_plugin_handle_packet;
  plugin_class->update_state = valent_battery_plugin_update_state;
}

static void
valent_battery_plugin_init (ValentBatteryPlugin *self)
{
  self->icon_name = "battery-missing-symbolic";
  self->charge_rate = DEFAULT_CHARGE_RATE;
  self->discharge_rate = DEFAULT_DISCHARGE_RATE;
}

