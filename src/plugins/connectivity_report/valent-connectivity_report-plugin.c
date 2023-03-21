// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-connectivity_report-plugin"

#include "config.h"

#include <math.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-connectivity_report-plugin.h"
#include "valent-telephony.h"


struct _ValentConnectivityReportPlugin
{
  ValentDevicePlugin  parent_instance;

  /* Local Modems */
  ValentTelephony    *telephony;
  unsigned int        telephony_watch : 1;
};

static void   valent_connectivity_report_plugin_request_state (ValentConnectivityReportPlugin *self);
static void   valent_connectivity_report_plugin_send_state    (ValentConnectivityReportPlugin *self);

G_DEFINE_FINAL_TYPE (ValentConnectivityReportPlugin, valent_connectivity_report_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/*
 * Local Modems
 */
static void
on_telephony_changed (ValentTelephony                *telephony,
                      ValentConnectivityReportPlugin *self)
{
  g_assert (VALENT_IS_TELEPHONY (telephony));
  g_assert (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (self));

  valent_connectivity_report_plugin_send_state (self);
}


static void
valent_connectivity_report_plugin_watch_telephony (ValentConnectivityReportPlugin *self,
                                                   gboolean                        watch)
{
  g_assert (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (self));

  if (self->telephony_watch == watch)
    return;

  if (self->telephony == NULL)
    self->telephony = valent_telephony_get_default ();

  if (watch)
    {
      g_signal_connect (self->telephony,
                        "changed",
                        G_CALLBACK (on_telephony_changed),
                        self);
      self->telephony_watch = TRUE;
    }
  else
    {
      g_signal_handlers_disconnect_by_data (self->telephony, self);
      self->telephony_watch = FALSE;
    }
}

static void
valent_connectivity_report_plugin_handle_connectivity_report_request (ValentConnectivityReportPlugin *self,
                                                                      JsonNode                       *packet)
{
  g_assert (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  valent_connectivity_report_plugin_send_state (self);
}

static void
valent_connectivity_report_plugin_send_state (ValentConnectivityReportPlugin *self)
{
  GSettings *settings;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (JsonNode) signal_node = NULL;

  g_return_if_fail (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (self));

  settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

  if (!g_settings_get_boolean (settings, "share-state"))
    return;

  signal_node = valent_telephony_get_signal_strengths (self->telephony);

  valent_packet_init (&builder, "kdeconnect.connectivity_report");
  json_builder_set_member_name (builder, "signalStrengths");
  json_builder_add_value (builder, g_steal_pointer (&signal_node));
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}


/*
 * Remote Modems
 */
static const char *
get_network_type_icon (const char *network_type)
{
  if (strcmp (network_type, "GSM") == 0 ||
      strcmp (network_type, "CDMA") == 0 ||
      strcmp (network_type, "iDEN") == 0)
    return "network-cellular-2g-symbolic";

  if (strcmp (network_type, "UMTS") == 0 ||
      strcmp (network_type, "CDMA2000") == 0)
    return "network-cellular-3g-symbolic";

  if (strcmp (network_type, "EDGE") == 0)
    return "network-cellular-edge-symbolic";

  if (strcmp (network_type, "GPRS") == 0)
    return "network-cellular-gprs-symbolic";

  if (strcmp (network_type, "HSPA") == 0)
    return "network-cellular-hspa-symbolic";

  if (strcmp (network_type, "LTE") == 0)
    return "network-cellular-4g-symbolic";

  if (strcmp (network_type, "5G") == 0)
    return "network-cellular-5g-symbolic";

  return "network-cellular-symbolic";
}

static const char *
get_signal_strength_icon (double signal_strength)
{
  if (signal_strength >= 4.0)
    return "network-cellular-signal-excellent-symbolic";

  if (signal_strength >= 3.0)
    return "network-cellular-signal-good-symbolic";

  if (signal_strength >= 2.0)
    return "network-cellular-signal-ok-symbolic";

  if (signal_strength >= 1.0)
    return "network-cellular-signal-weak-symbolic";

  if (signal_strength >= 0.0)
    return "network-cellular-signal-none-symbolic";

  return "network-cellular-offline-symbolic";
}

static void
get_status_labels (double   signal_strength,
                   char   **status_title,
                   char   **status_body)
{
  if (signal_strength >= 1.0)
    {
      /* TRANSLATORS: When the mobile network signal is available */
      *status_title = g_strdup (_("Mobile Network"));
      /* TRANSLATORS: The mobile network signal strength (e.g. "Signal Strength (25%)") */
      *status_body = g_strdup_printf (_("Signal Strength %f%%"),
                                      floor (signal_strength * 20.0));
    }
  else if (signal_strength >= 0.0)
    {
      /* TRANSLATORS: When no mobile service is available */
      *status_title = g_strdup (_("No Service"));
      /* TRANSLATORS: When no mobile network signal is available */
      *status_body = g_strdup (_("No mobile network service"));
    }
  else
    {
      /* TRANSLATORS: When no mobile service is available */
      *status_title = g_strdup (_("No Service"));
      /* TRANSLATORS: When the device is missing a SIM card */
      *status_body = g_strdup (_("No SIM"));
    }
}

static void
valent_connectivity_report_plugin_handle_connectivity_report (ValentConnectivityReportPlugin *self,
                                                              JsonNode                       *packet)
{
  GAction *action;
  GVariant *state;
  GSettings *settings;
  GVariantBuilder builder;
  GVariantBuilder signals_builder;
  JsonObject *signal_strengths;
  JsonObjectIter iter;
  const char *signal_id;
  JsonNode *signal_node;
  double average_strength = 0.0;
  double n_nodes = 0;
  gboolean is_online = FALSE;
  const char *status_icon;
  g_autofree char *status_title = NULL;
  g_autofree char *status_body = NULL;

  g_assert (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_object (packet, "signalStrengths", &signal_strengths))
    {
      g_debug ("%s(): expected \"signalStrengths\" field holding an object",
               G_STRFUNC);
      return;
    }

  settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

  /* Add each signal */
  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_init (&signals_builder, G_VARIANT_TYPE_VARDICT);

  json_object_iter_init (&iter, signal_strengths);

  while (json_object_iter_next (&iter, &signal_id, &signal_node))
    {
      GVariantBuilder signal_builder;
      JsonObject *signal_obj;
      const char *network_type;
      gint64 signal_strength;
      const char *icon_name;

      if G_UNLIKELY (json_node_get_value_type (signal_node) != JSON_TYPE_OBJECT)
        {
          g_debug ("%s(): expected entry value holding an object",
                   G_STRFUNC);
          continue;
        }

      /* Extract the signal information */
      signal_obj = json_node_get_object (signal_node);
      network_type = json_object_get_string_member_with_default (signal_obj,
                                                                 "networkType",
                                                                 "Unknown");
      signal_strength = json_object_get_int_member_with_default (signal_obj,
                                                                 "signalStrength",
                                                                 -1);
      icon_name = get_network_type_icon (network_type);

      /* Ignore offline modems (`-1`) when determining the average strength */
      if (signal_strength >= 0)
        {
          average_strength = (n_nodes * average_strength + signal_strength) /
                             (n_nodes + 1);
          n_nodes += 1;
          is_online = TRUE;
        }

      /* Add the signal to the `signal_strengths` dictionary */
      g_variant_builder_init (&signal_builder, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_add (&signal_builder, "{sv}", "network-type",
                             g_variant_new_string (network_type));
      g_variant_builder_add (&signal_builder, "{sv}", "signal-strength",
                             g_variant_new_int64 (signal_strength));
      g_variant_builder_add (&signal_builder, "{sv}", "icon-name",
                             g_variant_new_string (icon_name));
      g_variant_builder_add (&signals_builder, "{sv}", signal_id,
                             g_variant_builder_end (&signal_builder));
    }

  g_variant_builder_add (&builder, "{sv}", "signal-strengths",
                         g_variant_builder_end (&signals_builder));

  /* Set the status properties */
  status_icon = get_signal_strength_icon (is_online ? average_strength : -1);
  get_status_labels (is_online ? average_strength : -1,
                     &status_title,
                     &status_body);

  g_variant_builder_add (&builder, "{sv}", "icon-name",
                         g_variant_new_string (status_icon));
  g_variant_builder_add (&builder, "{sv}", "title",
                         g_variant_new_string (status_title));
  g_variant_builder_add (&builder, "{sv}", "body",
                         g_variant_new_string (status_body));

  state = g_variant_builder_end (&builder);

  /* Update the GAction */
  action = g_action_map_lookup_action (G_ACTION_MAP (self), "state");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                               json_object_get_size (signal_strengths) > 0);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), state);

  /* Notify if necessary */
  if (average_strength > 0.0)
    {
      valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self),
                                              "offline");
    }
  else if (g_settings_get_boolean (settings, "offline-notification"))
    {
      ValentDevice *device;
      g_autoptr (GNotification) notification = NULL;
      g_autoptr (GIcon) icon = NULL;
      g_autofree char *title = NULL;
      g_autofree char *body = NULL;
      const char *device_name;

      device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
      device_name = valent_device_get_name (device);

      /* TRANSLATORS: The connectivity notification title (e.g. "PinePhone: No Service") */
      title = g_strdup_printf (_("%s: %s"), device_name, status_title);
      /* TRANSLATORS: The connectivity notification body (e.g. "No mobile network service") */
      body = g_strdup (status_body);
      icon = g_themed_icon_new (status_icon);

      notification = g_notification_new (title);
      g_notification_set_body (notification, body);
      g_notification_set_icon (notification, icon);
      valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                              "offline",
                                              notification);
    }
}

static void
valent_connectivity_report_plugin_request_state (ValentConnectivityReportPlugin *self)
{
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (self));

  packet = valent_packet_new ("kdeconnect.connectivity_report.request");
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
valent_connectivity_report_plugin_enable (ValentDevicePlugin *plugin)
{
  g_assert (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (plugin));

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
}

static void
valent_connectivity_report_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentConnectivityReportPlugin *self = VALENT_CONNECTIVITY_REPORT_PLUGIN (plugin);

  g_assert (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (self));

  /* We're about to be disposed, so stop watching the network for changes */
  valent_connectivity_report_plugin_watch_telephony (self, FALSE);
}

static void
valent_connectivity_report_plugin_update_state (ValentDevicePlugin *plugin,
                                                ValentDeviceState   state)
{
  ValentConnectivityReportPlugin *self = VALENT_CONNECTIVITY_REPORT_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  if (available)
    {
      valent_connectivity_report_plugin_watch_telephony (self, TRUE);
      valent_connectivity_report_plugin_send_state (self);
      valent_connectivity_report_plugin_request_state (self);
    }
  else
    {
      valent_connectivity_report_plugin_watch_telephony (self, FALSE);
      valent_device_plugin_toggle_actions (plugin, available);
    }
}

static void
valent_connectivity_report_plugin_handle_packet (ValentDevicePlugin *plugin,
                                                 const char         *type,
                                                 JsonNode           *packet)
{
  ValentConnectivityReportPlugin *self = VALENT_CONNECTIVITY_REPORT_PLUGIN (plugin);

  g_assert (VALENT_IS_CONNECTIVITY_REPORT_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  /* A remote connectivity report */
  if (strcmp (type, "kdeconnect.connectivity_report") == 0)
    valent_connectivity_report_plugin_handle_connectivity_report (self, packet);

  /* A request for a local connectivity report */
  else if (strcmp (type, "kdeconnect.connectivity_report.request") == 0)
    valent_connectivity_report_plugin_handle_connectivity_report_request (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_connectivity_report_plugin_class_init (ValentConnectivityReportPluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_connectivity_report_plugin_enable;
  plugin_class->disable = valent_connectivity_report_plugin_disable;
  plugin_class->handle_packet = valent_connectivity_report_plugin_handle_packet;
  plugin_class->update_state = valent_connectivity_report_plugin_update_state;
}

static void
valent_connectivity_report_plugin_init (ValentConnectivityReportPlugin *self)
{
}

