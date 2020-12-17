// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-battery-plugin"

#include "config.h"

#include <math.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-power.h>

#include "valent-battery-plugin.h"


struct _ValentBatteryPlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  GSettings         *settings;

  /* Local Battery */
  ValentPower       *power;
  unsigned long      power_changed_id;

  /* Remote Battery */
  gboolean           charging;
  int                level;
  int                threshold;
  unsigned int       time;
};

static const char * valent_battery_plugin_get_icon_name (ValentBatteryPlugin         *self);
static void         valent_battery_plugin_request_state (ValentBatteryPlugin         *self);
static void         valent_battery_plugin_send_state    (ValentBatteryPlugin         *self);

static void         valent_device_plugin_iface_init     (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentBatteryPlugin, valent_battery_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


/**
 * valent_battery_plugin_update_estimate:
 * @self: a #ValentBatteryPlugin
 *
 * Recalculate the charge or dischare rate and update the estimated time
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
  gint new_rate, new_time, new_level;
  gint rate, time, level;
  gint level_delta, time_delta;

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
  GVariant *state;
  GActionGroup *group;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  group = valent_device_get_actions (self->device);
  state = g_variant_new ("(bsiu)",
                         self->charging,
                         valent_battery_plugin_get_icon_name (self),
                         self->level,
                         self->time);
  g_action_group_change_action_state (group, "battery", state);
}

static void
on_power_changed (ValentPower         *power,
                  ValentBatteryPlugin *self)
{
  g_assert (VALENT_IS_POWER (power));
  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  valent_battery_plugin_send_state (self);
}

static void
connect_power (ValentBatteryPlugin *self,
               gboolean             connect)
{
  if (self->power == NULL)
    self->power = valent_power_get_default ();

  if (connect && self->power_changed_id == 0)
    {
      self->power_changed_id = g_signal_connect (self->power,
                                                 "changed",
                                                 G_CALLBACK (on_power_changed),
                                                 self);
    }
  else if (!connect)
    {
      g_clear_signal_handler (&self->power_changed_id, self->power);
    }
}

/**
 * valent_battery_plugin_get_icon_name:
 * @self: a #ValentBatteryPlugin
 *
 * Get the symbolic icon name representing the current state of the battery.
 *
 * Returns: (transfer none): a symbolic icon name
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

static void
valent_battery_plugin_show_notification (ValentBatteryPlugin *self,
                                         gboolean             full)
{
  g_autoptr (GNotification) notif = NULL;
  g_autofree char *ntitle = NULL;
  g_autofree char *nbody = NULL;
  g_autoptr (GIcon) nicon = NULL;
  GActionGroup *actions;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  if (full)
    {
      if (!g_settings_get_boolean (self->settings, "full-notification"))
        return;

      /* TRANSLATORS: eg. Google Pixel: Battery Full */
      ntitle = g_strdup_printf (_("%s: Battery Full"),
                                valent_device_get_name (self->device));
      nbody = g_strdup (_("Battery Fully Charged"));
      nicon = g_themed_icon_new ("battery-full-charged-symbolic");
    }
  else
    {
      if (!g_settings_get_boolean (self->settings, "low-notification"))
        return;

      /* TRANSLATORS: eg. Google Pixel: Battery Low */
      ntitle = g_strdup_printf (_("%s: Battery Low"),
                                valent_device_get_name (self->device));
      /* TRANSLATORS: eg. 15% remaining */
      nbody = g_strdup_printf (_("%d%% remaining"), self->level);
      nicon = g_themed_icon_new (valent_battery_plugin_get_icon_name (self));
    }

  /* Create the notification */
  notif = g_notification_new (ntitle);
  g_notification_set_body (notif, nbody);
  g_notification_set_icon (notif, nicon);

  /* If the action is available, add a button to ring the device */
  actions = valent_device_get_actions (self->device);

  if (g_action_group_get_action_enabled (actions, "ring"))
    valent_notification_add_device_button (notif, self->device, _("Ring"), "ring", NULL);

  valent_device_show_notification (self->device, "battery-level", notif);
}

/**
 * Packet Handlers
 */
static void
valent_battery_plugin_handle_battery (ValentDevicePlugin *plugin,
                                      JsonNode           *packet)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (plugin);
  JsonObject *body;
  gboolean changed = FALSE;
  gboolean charging;
  gint64 level;
  gint64 threshold;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);
  charging = json_object_get_boolean_member_with_default (body, "isCharging",
                                                          self->charging);
  level = json_object_get_int_member_with_default (body, "currentCharge",
                                                   self->level);
  threshold = json_object_get_int_member_with_default (body, "thresholdEvent",
                                                       0);

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
          valent_device_hide_notification (self->device, "battery-level");
        }

      /* Notify listening parties */
      valent_battery_plugin_update_gaction (self);
    }
}

static void
valent_battery_plugin_handle_battery_request (ValentBatteryPlugin *self)
{
  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  valent_battery_plugin_send_state (self);
}

/**
 * valent_battery_plugin_request_state:
 * @self: a #ValentBatteryPlugin
 *
 * Send a request for the remote device's battery state.
 */
static void
valent_battery_plugin_request_state (ValentBatteryPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_BATTERY_PLUGIN (self));

  builder = valent_packet_start("kdeconnect.battery.request");
  json_builder_set_member_name (builder, "request");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/**
 * valent_battery_plugin_send_state:
 * @self: a #ValentBatteryPlugin
 *
 * Send the local device's battery state.
 */
static void
valent_battery_plugin_send_state (ValentBatteryPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_BATTERY_PLUGIN (self));

  if (!g_settings_get_boolean (self->settings, "share-state"))
    return;

  builder = valent_packet_start("kdeconnect.battery");
  json_builder_set_member_name (builder, "currentCharge");
  json_builder_add_int_value (builder, valent_power_get_battery_level (self->power));
  json_builder_set_member_name (builder, "isCharging");
  json_builder_add_boolean_value (builder, valent_power_get_battery_charging (self->power));
  json_builder_set_member_name (builder, "thresholdEvent");
  json_builder_add_int_value (builder, valent_power_get_battery_warning (self->power));
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/*
 * GActions
 */
static const GActionEntry actions[] = {
    {"battery", NULL, NULL, "(false, 'battery-missing-symbolic', -1, uint32 0)", NULL},
};

/*
 * ValentDevicePlugin
 */
static void
valent_battery_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (plugin);
  const char *device_id;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  /* Setup GSettings */
  device_id = valent_device_get_id (self->device);
  self->settings = valent_device_plugin_new_settings (device_id, "battery");

  /* Register GActions */
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));
}

static void
valent_battery_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (plugin);

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin, actions, G_N_ELEMENTS (actions));

  /* Dispose GSettings */
  g_clear_object (&self->settings);

  /* Drop power */
  connect_power (self, FALSE);
}

static void
valent_battery_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  g_assert (VALENT_IS_BATTERY_PLUGIN (self));

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  /* GActions */
  valent_device_plugin_toggle_actions (plugin,
                                       actions, G_N_ELEMENTS (actions),
                                       available);

  if (available)
    {
      connect_power (self, TRUE);
      valent_battery_plugin_send_state (self);
      valent_battery_plugin_request_state (self);
    }
  else
    {
      connect_power (self, FALSE);
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
    valent_battery_plugin_handle_battery (plugin, packet);

  /* A request for the local battery state */
  else if (g_strcmp0 (type, "kdeconnect.battery.request") == 0)
    valent_battery_plugin_handle_battery_request (self);

  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_battery_plugin_enable;
  iface->disable = valent_battery_plugin_disable;
  iface->handle_packet = valent_battery_plugin_handle_packet;
  iface->update_state = valent_battery_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_battery_plugin_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (object);

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
valent_battery_plugin_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ValentBatteryPlugin *self = VALENT_BATTERY_PLUGIN (object);

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
valent_battery_plugin_init (ValentBatteryPlugin *self)
{
  self->power = valent_power_get_default ();

  self->charging = FALSE;
  self->level = -1;
  self->time = 0;
  self->threshold = 15;
}

static void
valent_battery_plugin_class_init (ValentBatteryPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_battery_plugin_get_property;
  object_class->set_property = valent_battery_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

