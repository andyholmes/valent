// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-battery-plugin"

#include "config.h"

#include <math.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-battery.h"
#include "valent-battery-plugin.h"


struct _ValentBatteryPlugin
{
  ValentDevicePlugin  parent_instance;

  GSettings         *settings;

  /* Local Battery */
  ValentBattery     *battery;
  unsigned int       battery_watch : 1;

  /* Remote Battery */
  gboolean           charging;
  int                level;
  int                threshold;
  unsigned int       time;
};

static const char * valent_battery_plugin_get_icon_name (ValentBatteryPlugin         *self);
static void         valent_battery_plugin_request_state (ValentBatteryPlugin         *self);
static void         valent_battery_plugin_send_state    (ValentBatteryPlugin         *self);

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
  if (self->level == -1)
    return "battery-missing-symbolic";
  else if (self->level == 100)
    return "battery-full-charged-symbolic";
  else if (self->level < 3)
    return self->charging ? "battery-empty-charging-symbolic" :
                            "battery-empty-symbolic";
  else if (self->level < 10)
    return self->charging ? "battery-caution-charging-symbolic" :
                            "battery-caution-symbolic";
  else if (self->level < 30)
    return self->charging ? "battery-low-charging-symbolic" :
                            "battery-low-symbolic";
  else if (self->level < 60)
    return self->charging ? "battery-good-charging-symbolic" :
                            "battery-good-symbolic";
  else if (self->level >= 60)
    return self->charging ? "battery-full-charging-symbolic" :
                            "battery-full-symbolic";
  else
    return "battery-missing-symbolic";
}

/**
 * valent_battery_plugin_update_estimate:
 * @self: a #ValentBatteryPlugin
 *
 * Recalculate the charge or discharge rate and update the estimated time
 * remaining.
 *
 * `rate` is defined as `seconds / percent`. The updated `rate` is a weighted
 * average of the previous and current rate, favouring the current rate, to
 * account for the possibility of missed packets or changing battery usage.
 *
 *   `final_rate = (previous_rate * 0.4) + (current_rate * 0.6)`
 *
 * The time remaining in seconds is then calcuated by (charging, discharging):
 *
 *   `final_rate * (100 - current_level)`
 *   `final_rate * current_level`
 */
static void
valent_battery_plugin_update_estimate (ValentBatteryPlugin *self)
{
  g_autoptr (GDateTime) now = NULL;
  g_autoptr (GVariant) cache = NULL;
  GVariant *new_state;
  int new_rate, new_time, new_level;
  int rate, time, level;
  int level_delta, time_delta;

  now = g_date_time_new_now_local ();
  new_time = floor (g_date_time_to_unix (now) / 1000);
  new_level = self->level;

  /* Read the cached state */
  if (self->charging)
    cache = g_settings_get_value (self->settings, "charge-rate");
  else
    cache = g_settings_get_value (self->settings, "discharge-rate");

  g_variant_get (cache, "(uui)", &rate, &time, &level);
  time = (finite (time) && time > 0) ? time : new_time;
  level = (finite (level) && level > -1) ? level : new_level;
  rate = (finite (rate) && rate > 0) ? rate : (self->charging ? 54 : 864);

  /* Derive rate from time-delta/level-delta (rate = seconds/percent) */
  level_delta = self->charging ? new_level - level : level - new_level;
  time_delta = new_time - time;
  new_rate = (level_delta && time_delta) ? time_delta / level_delta : rate;

  /* Average if the new rate seems valid, then calculate time remaining */
  if (new_rate && finite (new_rate))
    new_rate = floor ((rate * 0.4) + (new_rate * 0.6));

  if (self->charging)
    self->time = floor (new_rate * (100 - new_level));
  else
    self->time = floor (new_rate * new_level);

  /* Write the cached state */
  new_state = g_variant_new ("(uui)", new_rate, new_time, new_level);

  if (self->charging)
    g_settings_set_value (self->settings, "charge-rate", new_state);
  else
    g_settings_set_value (self->settings, "discharge-rate", new_state);
}

static void
valent_battery_plugin_update_gaction (ValentBatteryPlugin *self)
{
  GAction *action;
  GVariant *state;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  state = g_variant_new ("(bsiu)",
                         self->charging,
                         valent_battery_plugin_get_icon_name (self),
                         self->level,
                         self->time);

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "state");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), state);
}

static void
valent_battery_plugin_show_notification (ValentBatteryPlugin *self,
                                         gboolean             full)
{
  g_autoptr (GNotification) notif = NULL;
  g_autofree char *ntitle = NULL;
  g_autofree char *nbody = NULL;
  g_autoptr (GIcon) nicon = NULL;
  ValentDevice *device;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));

  if (full)
    {
      if (!g_settings_get_boolean (self->settings, "full-notification"))
        return;

      /* TRANSLATORS: eg. Google Pixel: Battery Full */
      ntitle = g_strdup_printf (_("%s: Battery Full"),
                                valent_device_get_name (device));
      nbody = g_strdup (_("Battery Fully Charged"));
      nicon = g_themed_icon_new ("battery-full-charged-symbolic");
    }
  else
    {
      if (!g_settings_get_boolean (self->settings, "low-notification"))
        return;

      /* TRANSLATORS: eg. Google Pixel: Battery Low */
      ntitle = g_strdup_printf (_("%s: Battery Low"),
                                valent_device_get_name (device));
      /* TRANSLATORS: eg. 15% remaining */
      nbody = g_strdup_printf (_("%d%% remaining"), self->level);
      nicon = g_themed_icon_new (valent_battery_plugin_get_icon_name (self));
    }

  /* Create the notification */
  notif = g_notification_new (ntitle);
  g_notification_set_body (notif, nbody);
  g_notification_set_icon (notif, nicon);

  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          "battery-level",
                                          notif);
}

static void
valent_battery_plugin_handle_battery (ValentBatteryPlugin *self,
                                      JsonNode            *packet)
{
  gboolean changed = FALSE;
  gboolean charging = self->charging;
  gint64 level = self->level;
  gint64 threshold = 0;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  valent_packet_get_boolean (packet, "isCharging", &charging);
  valent_packet_get_int (packet, "currentCharge", &level);
  valent_packet_get_int (packet, "thresholdEvent", &threshold);

  /* We get a lot of battery updates, so check if something changed */
  if (self->charging != charging || self->level != level)
    {
      changed = TRUE;
      self->charging = charging;
      self->level = level;
    }

  /* We can always update the time estimate */
  valent_battery_plugin_update_estimate (self);

  if (changed)
    {
      int full, low;

      full = g_settings_get_int (self->settings, "full-notification-level");
      low = g_settings_get_int (self->settings, "low-notification-level");

      /* Battery is now full */
      if (self->level == full &&
          g_settings_get_boolean (self->settings, "full-notification"))
        {
          valent_battery_plugin_show_notification (self, TRUE);
        }

      /* Battery is now low */
      else if ((self->level <= low || threshold) &&
               g_settings_get_boolean (self->settings, "low-notification"))
        {
          self->threshold = self->level;
          valent_battery_plugin_show_notification (self, FALSE);
        }

      /* Battery is no longer low or is charging */
      else if (self->level > self->threshold || self->charging)
        {
          valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self),
                                                  "battery-level");
        }

      /* Notify listening parties */
      valent_battery_plugin_update_gaction (self);
    }
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
static const GActionEntry actions[] = {
    {"state", NULL, NULL, "(false, 'battery-missing-symbolic', -1, uint32 0)", NULL},
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

  valent_device_plugin_toggle_actions (plugin, available);

  if (available)
    {
      valent_battery_plugin_watch_battery (self, TRUE);
      valent_battery_plugin_send_state (self);
      valent_battery_plugin_request_state (self);
    }
  else
    {
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
  self->charging = FALSE;
  self->level = -1;
  self->time = 0;
  self->threshold = 15;
}

