// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-battery-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <math.h>

#include "valent-battery.h"
#include "valent-battery-plugin.h"

/* Defaults are 90m charge, 1d discharge (seconds/percent) */
#define DEFAULT_CHARGE_RATE    (90*60/100)
#define DEFAULT_DISCHARGE_RATE (24*60*60/100)


struct _ValentBatteryPlugin
{
  ValentDevicePlugin  parent_instance;

  GSettings          *settings;

  /* Local Battery */
  ValentBattery      *battery;
  unsigned int        battery_watch : 1;

  /* Remote Battery */
  gboolean            charging;
  const char         *icon_name;
  gboolean            is_present;
  double              percentage;
  gint64              time_to_full;
  gint64              time_to_empty;
  gint64              charge_rate;
  gint64              discharge_rate;
  gint64              timestamp;
};

static const char * valent_battery_plugin_get_icon_name (ValentBatteryPlugin *self);
static void         valent_battery_plugin_request_state (ValentBatteryPlugin *self);
static void         valent_battery_plugin_send_state    (ValentBatteryPlugin *self);

G_DEFINE_TYPE (ValentBatteryPlugin, valent_battery_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/*
 * Local Battery
 */
static void
on_battery_changed (ValentBattery       *battery,
                    ValentBatteryPlugin *self)
{
  g_assert (VALENT_IS_BATTERY (battery));
  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  if (valent_battery_get_level (battery) > 0)
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
      g_signal_connect (self->battery,
                       "changed",
                       G_CALLBACK (on_battery_changed),
                       self);
      self->battery_watch = TRUE;
    }
  else
    {
      g_signal_handlers_disconnect_by_data (self->battery, self);
      self->battery_watch = FALSE;
    }
}

static void
valent_battery_plugin_handle_battery_request (ValentBatteryPlugin *self,
                                              JsonNode            *packet)
{
  g_assert (VALENT_IS_BATTERY_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (valent_packet_check_field (packet, "request"))
    valent_battery_plugin_send_state (self);
}

static void
valent_battery_plugin_send_state (ValentBatteryPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_BATTERY_PLUGIN (self));

  if (!g_settings_get_boolean (self->settings, "share-state"))
    return;

  builder = valent_packet_start ("kdeconnect.battery");
  json_builder_set_member_name (builder, "currentCharge");
  json_builder_add_int_value (builder, valent_battery_get_level (self->battery));
  json_builder_set_member_name (builder, "isCharging");
  json_builder_add_boolean_value (builder, valent_battery_get_charging (self->battery));
  json_builder_set_member_name (builder, "thresholdEvent");
  json_builder_add_int_value (builder, valent_battery_get_threshold (self->battery));
  packet = valent_packet_finish (builder);

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

  if (self->percentage == 100)
    return "battery-full-charged-symbolic";

  if (self->percentage < 5)
    return self->charging
      ? "battery-empty-charging-symbolic"
      : "battery-empty-symbolic";

  if (self->percentage < 20)
    return self->charging
      ? "battery-caution-charging-symbolic"
      : "battery-caution-symbolic";

  if (self->percentage < 30)
    return self->charging
      ? "battery-low-charging-symbolic"
      : "battery-low-symbolic";

  if (self->percentage < 60)
    return self->charging
      ? "battery-good-charging-symbolic"
      : "battery-good-symbolic";

  return self->charging
    ? "battery-full-charging-symbolic"
    : "battery-full-symbolic";
}

static void
valent_battery_plugin_update_estimate (ValentBatteryPlugin *self,
                                       gint64               current_charge,
                                       gboolean             is_charging)
{
  gint64 rate;
  double percentage;
  gint64 timestamp;

  g_return_if_fail (current_charge >= 0);

  percentage = CLAMP (current_charge, 0.0, 100.0);
  timestamp = floor (valent_timestamp_ms () / 1000);
  rate = is_charging ? self->charge_rate : self->discharge_rate;

  /* If the battery is present, we must have a timestamp and charge level to
   * calculate the deltas and derive the (dis)charge rate. */
  if (self->is_present)
    {
      double percentage_delta;
      gint64 timestamp_delta;
      gint64 new_rate;

      percentage_delta = ABS (percentage - self->percentage);
      timestamp_delta = timestamp - self->timestamp;
      new_rate = timestamp_delta / percentage_delta;
      rate = floor ((rate * 0.4) + (new_rate * 0.6));
    }

  /* Update the estimate and related values */
  if (is_charging)
    {
      self->charge_rate = rate;
      self->time_to_empty = 0;
      self->time_to_full = floor (self->charge_rate * (100.0 - percentage));
      self->timestamp = timestamp;
    }
  else
    {
      self->discharge_rate = rate;
      self->time_to_empty = floor (self->discharge_rate * percentage);
      self->time_to_full = 0;
      self->timestamp = timestamp;
    }
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
  double full, low;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
  device_name = valent_device_get_name (device);

  full = g_settings_get_double (self->settings, "full-notification-level");
  low = g_settings_get_double (self->settings, "low-notification-level");

  if (self->percentage == full)
    {
      if (!g_settings_get_boolean (self->settings, "full-notification"))
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
      gint64 total_minutes;
      int minutes;
      int hours;

      if (!g_settings_get_boolean (self->settings, "low-notification"))
        return;

      total_minutes = floor (self->time_to_empty / 60);
      minutes = total_minutes % 60;
      hours = floor (total_minutes / 60);

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
  gint64 current_charge;
  gint64 threshold_event;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_boolean (packet, "isCharging", &is_charging))
    is_charging = self->charging;

  if (!valent_packet_get_int (packet, "currentCharge", &current_charge))
    current_charge = self->percentage;

  if (!valent_packet_get_int (packet, "thresholdEvent", &threshold_event))
    threshold_event = 0;

  /* We get a lot of battery updates, so check if something changed */
  if (self->charging == is_charging && self->percentage == current_charge)
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

static void
valent_battery_plugin_request_state (ValentBatteryPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.battery.request");
  json_builder_set_member_name (builder, "request");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/*
 * GActions
 */
static void
state_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  // No-op to make the state read-only
}

static const GActionEntry actions[] = {
    {"state", NULL, NULL, "@a{sv} {}", state_action},
};

/*
 * ValentDevicePlugin
 */
static void
valent_battery_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (plugin);
  ValentDevice *device;
  const char *device_id;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  device = valent_device_plugin_get_device (plugin);
  device_id = valent_device_get_id (device);
  self->settings = valent_device_plugin_new_settings (device_id, "battery");

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_battery_plugin_update_gaction (self);
}

static void
valent_battery_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (plugin);

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  /* We're about to be disposed, so stop watching the battery for changes */
  valent_battery_plugin_watch_battery (self, FALSE);

  g_clear_object (&self->settings);
}

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
      valent_battery_plugin_send_state (self);
      valent_battery_plugin_request_state (self);
    }
  else
    {
      valent_device_plugin_toggle_actions (plugin, available);
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
  if (g_strcmp0 (type, "kdeconnect.battery") == 0)
    valent_battery_plugin_handle_battery (self, packet);

  /* A request for the local battery state */
  else if (g_strcmp0 (type, "kdeconnect.battery.request") == 0)
    valent_battery_plugin_handle_battery_request (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_battery_plugin_class_init (ValentBatteryPluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_battery_plugin_enable;
  plugin_class->disable = valent_battery_plugin_disable;
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

