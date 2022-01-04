// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "valent-core-enums.h"

#include "valent-channel.h"
#include "valent-data.h"
#include "valent-debug.h"
#include "valent-device.h"
#include "valent-device-plugin.h"
#include "valent-device-private.h"
#include "valent-macros.h"
#include "valent-object-utils.h"
#include "valent-packet.h"
#include "valent-transfer.h"
#include "valent-utils.h"

#define VALENT_DEVICE_DESKTOP    "desktop"
#define VALENT_DEVICE_LAPTOP     "laptop"
#define VALENT_DEVICE_SMARTPHONE "phone"
#define VALENT_DEVICE_TABLET     "tablet"
#define VALENT_DEVICE_TELEVISION "tv"
#define PAIR_REQUEST_TIMEOUT     30


/**
 * SECTION:valentdevice
 * @short_description: An object representing a remote device
 * @title: ValentDevice
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * The #ValentDevice object represents a remote device such as a smartphone,
 * tablet or desktop.
 *
 * #ValentDevice implements #GActionGroup and #GActionMap, while providing a
 * #GMenu. #ValentDevicePlugin implementations can add #GActions and #GMenuItems
 * to expose plugin activities.
 */

struct _ValentDevice
{
  GObject              parent_instance;

  ValentData          *data;
  GSettings           *settings;

  /* Device Properties */
  gboolean             connected;
  char                *id;
  char                *name;
  gboolean             paired;
  char                *type;
  char               **incoming_capabilities;
  char               **outgoing_capabilities;

  /* Channel */
  ValentChannel       *channel;
  unsigned int         incoming_pair;
  unsigned int         outgoing_pair;

  /* Plugins */
  PeasEngine          *engine;
  GHashTable          *plugins;
  GHashTable          *handlers;

  /* Actions & Menu */
  GSimpleActionGroup  *actions;
  GMenu               *menu;
};

static void valent_device_set_connected  (ValentDevice *device,
                                          gboolean      connected);
static void valent_device_reload_plugins (ValentDevice *device);
static void valent_device_update_plugins (ValentDevice *device);

G_DEFINE_TYPE (ValentDevice, valent_device, G_TYPE_OBJECT)


enum {
  PROP_0,
  PROP_CONNECTED,
  PROP_DATA,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_NAME,
  PROP_PAIRED,
  PROP_STATE,
  PROP_TYPE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  PLUGIN_ADDED,
  PLUGIN_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/*
 * Private plugin methods
 */
typedef struct
{
  ValentDevice   *device;
  PeasPluginInfo *info;
  PeasExtension  *extension;
  GSettings      *settings;
} DevicePlugin;

static void
device_plugin_free (gpointer data)
{
  DevicePlugin *plugin = data;

  plugin->device = NULL;
  plugin->info = NULL;
  g_clear_object (&plugin->settings);

  /* We guarantee calling valent_device_plugin_disable() */
  if (plugin->extension != NULL)
    {
      valent_device_plugin_disable (VALENT_DEVICE_PLUGIN (plugin->extension));
      g_clear_object (&plugin->extension);
    }

  g_clear_pointer (&plugin, g_free);
}

static void
valent_device_enable_plugin (ValentDevice *device,
                             DevicePlugin *plugin)
{
  g_auto (GStrv) incoming = NULL;
  unsigned int n_capabilities = 0;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (plugin != NULL);

  /* Instantiate the plugin */
  plugin->extension = peas_engine_create_extension (device->engine,
                                                    plugin->info,
                                                    VALENT_TYPE_DEVICE_PLUGIN,
                                                    "device", device,
                                                    NULL);

  if (plugin->extension == NULL)
    return;

  /* Register packet handlers */
  if ((incoming = valent_device_plugin_get_incoming (plugin->info)) != NULL)
    n_capabilities = g_strv_length (incoming);

  for (unsigned int i = 0; i < n_capabilities; i++)
    {
      g_hash_table_insert (device->handlers,
                           g_strdup (incoming[i]),
                           plugin->extension);
    }

  /* Invoke the plugin vfunc */
  valent_device_plugin_enable (VALENT_DEVICE_PLUGIN (plugin->extension));
  valent_device_plugin_update_state (VALENT_DEVICE_PLUGIN (plugin->extension));
}

static void
valent_device_disable_plugin (ValentDevice *device,
                              DevicePlugin *plugin)
{
  g_auto (GStrv) incoming = NULL;
  unsigned int len;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (plugin != NULL);

  /* Unregister packet handlers */
  incoming = valent_device_plugin_get_incoming (plugin->info);
  len = incoming ? g_strv_length (incoming) : 0;

  for (unsigned int i = 0; i < len; i++)
    g_hash_table_remove (device->handlers, incoming[i]);

  /* Invoke the plugin vfunc */
  valent_device_plugin_disable (VALENT_DEVICE_PLUGIN (plugin->extension));
  g_clear_object (&plugin->extension);
}

static void
on_enabled_changed (GSettings    *settings,
                    const char   *key,
                    DevicePlugin *plugin)
{
  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL);
  g_assert (plugin != NULL);

  if (g_settings_get_boolean (settings, "enabled"))
    valent_device_enable_plugin (plugin->device, plugin);
  else
    valent_device_disable_plugin (plugin->device, plugin);
}


/*
 * Private pairing methods
 */
static gboolean
valent_device_reset_pair (gpointer object)
{
  ValentDevice *device = object;

  g_assert (VALENT_IS_DEVICE (device));

  if (device->incoming_pair > 0)
    {
      valent_device_hide_notification (device, "pair-request");
      g_source_remove (device->incoming_pair);
      device->incoming_pair = 0;
    }

  if (device->outgoing_pair > 0)
    {
      g_source_remove (device->outgoing_pair);
      device->outgoing_pair = 0;
    }

  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);

  return G_SOURCE_REMOVE;
}

static void
send_pair_cb (ValentChannel *channel,
              GAsyncResult  *result,
              ValentDevice  *device)
{
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DEVICE (device));

  if (!valent_channel_write_packet_finish (channel, result, &error))
    {
      VALENT_DEBUG ("%s: %s", device->name, error->message);

      valent_device_reset_pair (device);

      if (device->channel == channel)
        valent_device_set_channel (device, NULL);
    }

  g_object_unref (device);
}

static void
valent_device_send_pair (ValentDevice *device,
                         gboolean      pair)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GCancellable) cancellable = NULL;

  g_assert (VALENT_IS_DEVICE (device));

  if (!device->connected)
    return;

  builder = valent_packet_start ("kdeconnect.pair");
  json_builder_set_member_name (builder, "pair");
  json_builder_add_boolean_value (builder, pair);
  packet = valent_packet_finish (builder);

  valent_channel_write_packet (device->channel,
                               packet,
                               cancellable,
                               (GAsyncReadyCallback)send_pair_cb,
                               g_object_ref (device));
}

static void
valent_device_notify_pair (ValentDevice *device)
{
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autofree char *title = NULL;
  const char *body;

  g_assert (VALENT_IS_DEVICE (device));

  if G_UNLIKELY (!device->connected)
    return;

  title = g_strdup_printf (_("Pairing request from %s"), device->name);
  notification = g_notification_new (title);

  if ((body = valent_channel_get_verification_key (device->channel)) != NULL)
    g_notification_set_body (notification, body);

  icon = g_themed_icon_new (APPLICATION_ID);
  g_notification_set_icon (notification, icon);

  g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_URGENT);

  g_notification_add_button_with_target (notification, _("Reject"), "app.device",
                                         "(ssbv)",
                                         device->id,
                                         "unpair",
                                         FALSE,
                                         g_variant_new ("s", ""));

  g_notification_add_button_with_target (notification, _("Accept"), "app.device",
                                         "(ssbv)",
                                         device->id,
                                         "pair",
                                         FALSE,
                                         g_variant_new ("s", ""));

  /* Show the pairing notification and set a timeout for 30s */
  valent_device_show_notification (device, "pair-request", notification);
  device->incoming_pair = g_timeout_add_seconds (PAIR_REQUEST_TIMEOUT,
                                                 valent_device_reset_pair,
                                                 device);
}

static void
valent_device_handle_pair (ValentDevice *device,
                           JsonNode     *packet)
{
  JsonObject *body;
  JsonNode *node;
  gboolean pair;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  if G_UNLIKELY ((node = json_object_get_member (body, "pair")) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_BOOLEAN)
    {
      g_warning ("%s: malformed pair packet", device->name);
      return;
    }

  pair = json_node_get_boolean (node);

  /* Device is requesting pairing or accepting our request */
  if (pair)
    {
      /* The device is accepting our request */
      if (device->outgoing_pair > 0)
        {
          VALENT_DEBUG ("Pairing accepted by %s", device->name);
          valent_device_set_paired (device, TRUE);
        }

      /* The device thinks we're unpaired */
      else if (device->paired)
        {
          valent_device_send_pair (device, TRUE);
          valent_device_set_paired (device, TRUE);
        }

      /* The device is requesting pairing */
      else
        {
          VALENT_DEBUG ("Pairing requested by %s", device->name);
          valent_device_notify_pair (device);
        }
    }

  /* Device is requesting unpairing or rejecting our request */
  else
    {
      VALENT_DEBUG ("Pairing rejected by %s", device->name);
      valent_device_set_paired (device, FALSE);
    }

  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
}

/*
 * Private identity methods
 */
static GStrv
dup_capabilities (JsonObject *body,
                  const char *member)
{
  g_auto (GStrv) strv = NULL;
  JsonNode *node;
  JsonArray *array;
  unsigned int len;

  node = json_object_get_member (body, member);

  if G_UNLIKELY (node == NULL || !JSON_NODE_HOLDS_ARRAY (node))
    return NULL;

  array = json_node_get_array (node);
  len = json_array_get_length (array);
  strv = g_new (char *, len + 1);

  for (unsigned int i = 0; i < len; i++)
    {
      JsonNode *element = json_array_get_element (array, i);

      if G_UNLIKELY (!JSON_NODE_HOLDS_VALUE (element) ||
                     json_node_get_value_type (element) != G_TYPE_STRING)
        return NULL;

      strv[i] = json_node_dup_string (element);
    }
  strv[len] = NULL;

  return g_steal_pointer (&strv);
}

static void
valent_device_handle_identity (ValentDevice *device,
                               JsonNode     *packet)
{
  JsonObject *body;
  const char *device_name;
  const char *device_type;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  /* Check if the name changed */
  device_name = json_object_get_string_member_with_default (body,
                                                            "deviceName",
                                                            "Unnamed");

  if (g_strcmp0 (device->name, device_name) != 0)
    {
      g_clear_pointer (&device->name, g_free);
      device->name = g_strdup (device_name);
      g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_NAME]);
    }

  /* "type" shouldn't ever change, but we check anyways */
  device_type = json_object_get_string_member_with_default (body,
                                                            "deviceType",
                                                            "desktop");

  if (g_strcmp0 (device->type, device_type) != 0)
    {
      g_clear_pointer (&device->type, g_free);
      device->type = g_strdup (device_type);
      g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_ICON_NAME]);
      g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_TYPE]);
    }

  /* Generally, these should be static, but could change if the connection type
   * changes between eg. TCP and Bluetooth */
  g_clear_pointer (&device->incoming_capabilities, g_strfreev);
  device->incoming_capabilities = dup_capabilities (body, "incomingCapabilities");

  g_clear_pointer (&device->outgoing_capabilities, g_strfreev);
  device->outgoing_capabilities = dup_capabilities (body, "outgoingCapabilities");

  /* Recheck plugins and load or unload if capabilities have changed */
  valent_device_reload_plugins (device);
}


/*
 * Stock GActions
 */
static void
pair_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentDevice *device = VALENT_DEVICE (user_data);

  /* We're accepting an incoming pair request */
  if (device->incoming_pair > 0)
    {
      valent_device_send_pair (device, TRUE);
      valent_device_set_paired (device, TRUE);
    }

  /* We're initiating an outgoing pair request */
  else if (!device->paired)
    {
      valent_device_reset_pair (device);
      valent_device_send_pair (device, TRUE);
      device->outgoing_pair = g_timeout_add_seconds (PAIR_REQUEST_TIMEOUT,
                                                     valent_device_reset_pair,
                                                     device);
      VALENT_DEBUG ("Pair request sent to %s", device->name);
    }

  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
}

static void
unpair_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  ValentDevice *device = VALENT_DEVICE (user_data);

  if (device->connected)
    valent_device_send_pair (device, FALSE);

  valent_device_set_paired (device, FALSE);

  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
}

/* GActions */
static const GActionEntry actions[] = {
  { "pair",   pair_action,   NULL, NULL, NULL },
  { "unpair", unpair_action, NULL, NULL, NULL }
};

/*
 * ValentEngine callbacks
 */
static void
on_load_plugin (PeasEngine     *engine,
                PeasPluginInfo *info,
                ValentDevice   *device)
{
  DevicePlugin *plugin;
  const char *module;
  g_autofree char *path = NULL;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE (device));

  if (!valent_device_supports_plugin (device, info))
    return;

  if (g_hash_table_contains (device->plugins, info))
    return;

  /* Register the plugin & data (hash tables are ref owners) */
  module = peas_plugin_info_get_module_name (info);
  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/plugin/%s/",
                          device->id, module);

  plugin = g_new0 (DevicePlugin, 1);
  plugin->device = device;
  plugin->info = info;
  plugin->settings = g_settings_new_with_path ("ca.andyholmes.Valent.Plugin",
                                               path);

  g_signal_connect (plugin->settings,
                    "changed::enabled",
                    G_CALLBACK (on_enabled_changed),
                    plugin);

  g_hash_table_insert (device->plugins, info, plugin);
  g_signal_emit (G_OBJECT (device), signals [PLUGIN_ADDED], 0, info);

  VALENT_DEBUG ("%s: %s", device->name, module);

  /* Init plugin as appropriate */
  if (g_settings_get_boolean (plugin->settings, "enabled"))
    valent_device_enable_plugin (device, plugin);
}

static void
on_unload_plugin (PeasEngine     *engine,
                  PeasPluginInfo *info,
                  ValentDevice   *device)
{
  DevicePlugin *plugin;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE (device));

  if ((plugin = g_hash_table_lookup (device->plugins, info)) == NULL)
    return;

  VALENT_DEBUG ("%s: %s", device->name,
                    peas_plugin_info_get_module_name (info));

  g_hash_table_remove (device->plugins, info);
  g_signal_emit (G_OBJECT (device), signals [PLUGIN_REMOVED], 0, info);
}

/*
 * GObject
 */
static void
valent_device_constructed (GObject *object)
{
  ValentDevice *self = VALENT_DEVICE (object);
  g_autofree char *path = NULL;
  g_autofree char *download_folder = NULL;
  const GList *plugins = NULL;

  /* We must at least have a device ID */
  g_assert (self->id != NULL);

  /* Data Manager */
  if (self->data == NULL)
    self->data = valent_data_new (self->id, NULL);

  /* GSettings*/
  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/", self->id);
  self->settings = g_settings_new_with_path ("ca.andyholmes.Valent.Device", path);
  self->paired = g_settings_get_boolean (self->settings, "paired");

  download_folder = g_settings_get_string (self->settings, "download-folder");

  if (strlen (download_folder) == 0)
    {
      g_clear_pointer (&download_folder, g_free);
      download_folder = valent_data_get_directory (G_USER_DIRECTORY_DOWNLOAD);
      g_settings_set_string (self->settings, "download-folder", download_folder);
    }

  /* Load plugins and watch for changes */
  plugins = peas_engine_get_plugin_list (self->engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    on_load_plugin (self->engine, iter->data, self);

  g_signal_connect_after (self->engine,
                          "load-plugin",
                          G_CALLBACK (on_load_plugin),
                          self);
  g_signal_connect (self->engine,
                    "unload-plugin",
                    G_CALLBACK (on_unload_plugin),
                    self);

  G_OBJECT_CLASS (valent_device_parent_class)->constructed (object);
}

static void
valent_device_dispose (GObject *object)
{
  ValentDevice *self = VALENT_DEVICE (object);

  valent_device_reset_pair (self);
  valent_device_set_channel (self, NULL);

  /* Plugins */
  g_signal_handlers_disconnect_by_data (self->engine, self);
  g_clear_pointer (&self->handlers, g_hash_table_unref);
  g_clear_pointer (&self->plugins, g_hash_table_unref);

  G_OBJECT_CLASS (valent_device_parent_class)->dispose (object);
}

static void
valent_device_finalize (GObject *object)
{
  ValentDevice *self = VALENT_DEVICE (object);

  /* Device Properties */
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->type, g_free);
  g_clear_pointer (&self->incoming_capabilities, g_strfreev);
  g_clear_pointer (&self->outgoing_capabilities, g_strfreev);

  /* Channel */
  g_clear_object (&self->channel);
  g_clear_object (&self->data);
  g_clear_object (&self->settings);

  /* GAction/GMenu */
  g_clear_object (&self->actions);
  g_clear_object (&self->menu);

  G_OBJECT_CLASS (valent_device_parent_class)->finalize (object);
}

static void
valent_device_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ValentDevice *self = VALENT_DEVICE (object);

  switch (prop_id)
    {
    case PROP_CONNECTED:
      g_value_set_boolean (value, self->connected);
      break;

    case PROP_DATA:
      g_value_set_object (value, self->data);
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, valent_device_get_icon_name (self));
      break;

    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_PAIRED:
      g_value_set_boolean (value, self->paired);
      break;

    case PROP_STATE:
      g_value_set_flags (value, valent_device_get_state (self));
      break;

    case PROP_TYPE:
      g_value_set_string (value, self->type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ValentDevice *self = VALENT_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DATA:
      self->data = g_value_dup_object (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_init (ValentDevice *self)
{
  /* Plugins */
  self->engine = valent_get_engine ();
  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, device_plugin_free);
  self->handlers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* GAction/GMenu */
  self->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  self->menu = g_menu_new();
}

static void
valent_device_class_init (ValentDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_device_constructed;
  object_class->dispose = valent_device_dispose;
  object_class->finalize = valent_device_finalize;
  object_class->get_property = valent_device_get_property;
  object_class->set_property = valent_device_set_property;

  /**
   * ValentDevice:connected:
   *
   * The "connected" property indicates whether the device is connected or not.
   * This means an active connection that has been authenticated as being from
   * the correct device, identities exchanged, encrypted and any other steps
   * appropriate for the connection type.
   */
  properties [PROP_CONNECTED] =
    g_param_spec_boolean ("connected",
                          "Connected",
                          "Whether the device is connected",
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:data:
   *
   * The "data" property is the #ValentData for the device.
   */
  properties [PROP_DATA] =
    g_param_spec_object ("data",
                         "Data Manager",
                         "The data manager for this device manager",
                         VALENT_TYPE_DATA,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:icon-name:
   *
   * An icon name string for the device type.
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Icon name representing the device",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:id:
   *
   * A unique string for the device, usually the hostname.
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "Unique id for the device",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:name:
   *
   * The user-visible label for the device.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name representing the device",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:paired:
   *
   * Whether the device is paired with regard to the KDE Connect protocol,
   * regardless of the underlying transport protocol (eg. Bluetooth).
   */
  properties [PROP_PAIRED] =
    g_param_spec_boolean ("paired",
                          "Paired",
                          "Whether the device is paired",
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:state:
   *
   * The state of the device.
   */
  properties [PROP_STATE] =
    g_param_spec_flags ("state",
                        "State",
                        "State of device",
                        VALENT_TYPE_DEVICE_STATE,
                        VALENT_DEVICE_STATE_NONE,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentDevice:type:
   *
   * A string hint, roughly describing the type of the device. It's generally
   * only useful for things like selecting an icon since the device will
   * describe its true capabilities by other means.
   *
   * The current possible values include `desktop`, `laptop`, `smartphone`,
   * `tablet` and `tv`.
   */
  properties [PROP_TYPE] =
    g_param_spec_string ("type",
                         "Type",
                         "Type of device (eg. phone, tablet, laptop)",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentDevice::plugin-added:
   * @device: an #ValentDevice
   * @info: an #PeasPluginInfo
   *
   * The "plugin-added" signal is emitted when a supported plugin implementing
   * the #ValentDevicePlugin interface has been loaded by the #PeasEngine.
   *
   * Alternatively, it may be emitted if @device sends an identity packet
   * indicating that the plugin's capabilities are now supported. This may
   * happen if the connection type changes, such as a Bluetooth connection where
   * SFTP is not supported being replaced with a TCP connection.
   */
  signals [PLUGIN_ADDED] =
    g_signal_new ("plugin-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, PEAS_TYPE_PLUGIN_INFO);
  g_signal_set_va_marshaller (signals [PLUGIN_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__BOXEDv);

  /**
   * ValentDevice::plugin-removed:
   * @device: an #ValentDevice
   * @info: an #PeasPluginInfo
   *
   * The "plugin-removed" signal is emitted when a supported plugin implementing
   * the #ValentDevicePlugin interface has been unloaded from the #PeasEngine.
   *
   * Alternatively, it may be emitted if @device sends an identity packet
   * indicating that the plugin's capabilities are not supported.
   */
  signals [PLUGIN_REMOVED] =
    g_signal_new ("plugin-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, PEAS_TYPE_PLUGIN_INFO);
  g_signal_set_va_marshaller (signals [PLUGIN_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__BOXEDv);
}

/**
 * valent_device_new:
 * @id: (not nullable): The unique id for this device
 *
 * Construct a new device for @id.
 */
ValentDevice *
valent_device_new (const char *id)
{
  return g_object_new (VALENT_TYPE_DEVICE,
                       "id", id,
                       NULL);
}

static void
queue_packet_cb (ValentChannel *channel,
                 GAsyncResult  *result,
                 ValentDevice  *device)
{
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DEVICE (device));

  if (!valent_channel_write_packet_finish (channel, result, &error))
    {
      if G_UNLIKELY (error && error->domain != G_IO_ERROR)
        VALENT_DEBUG ("%s: %s", device->name, error->message);

      if (device->channel == channel)
        valent_device_set_channel (device, NULL);
    }

  g_object_unref (device);
}

/**
 * valent_device_queue_packet:
 * @device: a #ValentDevice
 * @packet: a #JsonNode packet
 *
 * Push @packet onto the outgoing packet queue for the #ValentChannel of @device.
 * For cancellable or failable packet transfer, see valent_device_send_packet().
 */
void
valent_device_queue_packet (ValentDevice *device,
                            JsonNode     *packet)
{
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (VALENT_IS_PACKET (packet));

  if G_UNLIKELY (!device->connected)
    {
      g_warning ("%s(): %s is disconnected, discarding \"%s\"",
                 G_STRFUNC,
                 device->name,
                 valent_packet_get_type (packet));
      return;
    }

  if G_UNLIKELY (!device->paired)
    {
      g_critical ("%s(): %s is unpaired, discarding \"%s\"",
                  G_STRFUNC,
                  device->name,
                  valent_packet_get_type (packet));
      return;
    }

  VALENT_DEBUG_PKT (packet, device->name);
  valent_channel_write_packet (device->channel,
                               packet,
                               NULL,
                               (GAsyncReadyCallback)queue_packet_cb,
                               g_object_ref (device));
}

static void
send_packet_cb (ValentChannel *channel,
                GAsyncResult  *result,
                gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentDevice *device = g_task_get_source_object (task);
  GError *error = NULL;

  g_assert (VALENT_IS_DEVICE (device));

  if (!valent_channel_write_packet_finish (channel, result, &error))
    {
      g_task_return_error (task, error);

      if (device->channel == channel)
        valent_device_set_channel (device, NULL);
    }
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * valent_device_send_packet:
 * @device: a #ValentDevice
 * @packet: a #JsonNode packet
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Send @packet over the current packet channel. Call
 * valent_device_send_packet_finish() to get the result.
 */
void
valent_device_send_packet (ValentDevice        *device,
                           JsonNode            *packet,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (VALENT_IS_PACKET (packet));

  if G_UNLIKELY (!device->connected)
    return g_task_report_new_error (device,
                                    callback,
                                    user_data,
                                    valent_device_send_packet,
                                    G_IO_ERROR,
                                    G_IO_ERROR_NOT_CONNECTED,
                                    "%s is disconnected", device->name);

  if G_UNLIKELY (!device->paired)
    return g_task_report_new_error (device,
                                    callback,
                                    user_data,
                                    valent_device_send_packet,
                                    G_IO_ERROR,
                                    G_IO_ERROR_PERMISSION_DENIED,
                                    "%s is unpaired", device->name);

  task = g_task_new (device, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_device_send_packet);

  VALENT_DEBUG_PKT (packet, device->name);
  valent_channel_write_packet (device->channel,
                               packet,
                               cancellable,
                               (GAsyncReadyCallback)send_packet_cb,
                               g_steal_pointer (&task));
}

/**
 * valent_device_send_packet_finish:
 * @device: a #ValentDevice
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started with valent_device_send_packet().
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_device_send_packet_finish (ValentDevice  *device,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, device), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * valent_device_show_notification:
 * @device: a #ValentDevice
 * @id: an id for the notification
 * @notification: a #GNotification
 *
 * A simple convenience function for showing a local #GNotification with @id
 * prepended with the device id, to allow ostensibly the same notification for a
 * specific device to be shown.
 */
void
valent_device_show_notification (ValentDevice  *device,
                                 const char    *id,
                                 GNotification *notification)
{
  GApplication *application = g_application_get_default ();
  g_autofree char *notification_id = NULL;

  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (id != NULL);
  g_return_if_fail (G_IS_NOTIFICATION (notification));

  if G_UNLIKELY (application == NULL)
    return;

  /* Prefix the GNotification Id with the deviceId */
  notification_id = g_strdup_printf ("%s|%s", device->id, id);
  g_application_send_notification (application, notification_id, notification);
}

/**
 * valent_device_hide_notification:
 * @device: a #ValentDevice
 * @id: an id for the notification
 *
 * A simple convenience function for hiding a local #GNotification with @id
 * prepended with the device id, to allow ostensibly the same notification for a
 * specific device to be hidden.
 */
void
valent_device_hide_notification (ValentDevice *device,
                                 const char   *id)
{
  GApplication *application = g_application_get_default ();
  g_autofree char *notification_id = NULL;

  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (id != NULL);

  if G_UNLIKELY (application == NULL)
    return;

  notification_id = g_strdup_printf ("%s|%s", device->id, id);
  g_application_withdraw_notification (application, notification_id);
}

/**
 * valent_device_get_actions:
 * @device: a #ValentDevice
 *
 * Get the action group for the device.
 *
 * Returns: (transfer none): the #GActionGroup
 */
GActionGroup *
valent_device_get_actions (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return G_ACTION_GROUP (device->actions);
}

/**
 * valent_device_get_channel:
 * @device: a #ValentDevice
 *
 * Get the device #ValentChannel if connected, or %NULL if disconnected.
 *
 * Returns: (transfer none) (nullable): a #ValentChannel
 */
ValentChannel *
valent_device_get_channel (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return device->channel;
}

static void
read_packet_cb (ValentChannel *channel,
                GAsyncResult  *result,
                ValentDevice  *device)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DEVICE (device));

  packet = valent_channel_read_packet_finish (channel, result, &error);

  /* On success, queue another read before handling the packet */
  if (packet != NULL)
    {
      valent_channel_read_packet (channel,
                                  NULL,
                                  (GAsyncReadyCallback)read_packet_cb,
                                  g_object_ref (device));

      valent_device_handle_packet (device, packet);
    }

  /* On failure, drop our reference if it's still the active channel */
  else
    {
      if G_UNLIKELY (error && error->domain != G_IO_ERROR)
        VALENT_DEBUG ("%s: %s", device->name, error->message);

      if (device->channel == channel)
        valent_device_set_channel (device, NULL);
    }

  g_object_unref (device);
}

/**
 * valent_device_set_channel:
 * @device: A #ValentDevice
 * @channel: (nullable): A #ValentChannel
 *
 * Sets the active packet exchange channel for @device to @channel.
 */
void
valent_device_set_channel (ValentDevice  *device,
                           ValentChannel *channel)
{
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (channel == NULL || VALENT_IS_CHANNEL (channel));

  if (device->channel == channel)
    return;

  /* If there's an active channel, close it asynchronously and drop our
   * reference so the task holds the final reference. */
  if (device->channel)
    {
      valent_channel_close_async (device->channel, NULL, NULL, NULL);
      g_clear_object (&device->channel);
    }

  /* If there's a new channel handle the peer identity and queue the first read
   * before calling valent_device_set_connected(). */
  if (g_set_object (&device->channel, channel))
    {
      JsonNode *peer_identity;

      /* Handle the peer identity packet */
      peer_identity = valent_channel_get_peer_identity (channel);
      valent_device_handle_identity (device, peer_identity);

      /* Start receiving packets */
      valent_channel_read_packet (channel,
                                  NULL,
                                  (GAsyncReadyCallback)read_packet_cb,
                                  g_object_ref (device));
    }

  valent_device_set_connected (device, VALENT_IS_CHANNEL (device->channel));
}

/**
 * valent_device_get_connected:
 * @device: a #ValentDevice
 *
 * Whether the device is connected.
 *
 * Returns: %TRUE if the device has an active connection.
 */
gboolean
valent_device_get_connected (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), FALSE);

  return device->connected;
}

/**
 * valent_device_set_connected:
 * @device: a #ValentDevice
 * @connected: whether the device is connected or not
 *
 * Set the connected state for @device.
 */
static void
valent_device_set_connected (ValentDevice *device,
                             gboolean      connected)
{
  g_return_if_fail (VALENT_IS_DEVICE (device));

  if (device->connected == connected)
    return;

  /* Ensure plugins are updated before emitting */
  device->connected = connected;
  valent_device_update_plugins (device);

  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_CONNECTED]);
  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
}

/**
 * valent_device_get_data:
 * @device: a #ValentDevice
 *
 * Gets the #ValentData for @device.
 *
 * Returns: (transfer none): a #ValentData
 */
ValentData *
valent_device_get_data (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return device->data;
}

/**
 * valent_device_get_icon_name:
 * @device: a #ValentDevice
 *
 * Gets the symbolic icon name of the device.
 *
 * Returns: (transfer none): the icon name.
 */
const char *
valent_device_get_icon_name (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  if (g_strcmp0 (device->type, "phone") == 0)
    return "smartphone-symbolic";

  if (g_strcmp0 (device->type, "tablet") == 0)
    return "tablet-symbolic";

  if (g_strcmp0 (device->type, "laptop") == 0)
    return "laptop-symbolic";

  if (g_strcmp0 (device->type, "tv") == 0)
    return "tv-symbolic";

  return "computer-symbolic";
}

/**
 * valent_device_get_id:
 * @device: a #ValentDevice
 *
 * Gets the unique id of the device.
 *
 * Returns: (transfer none): the device id.
 */
const char *
valent_device_get_id (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);
  g_return_val_if_fail (device->id != NULL, NULL);

  return device->id;
}

/**
 * valent_device_get_menu:
 * @device: a #ValentDevice
 *
 * Get the #GMenuModel for @device. Plugins may add items and submenus to this
 * when they want to expose actions with presentation details like a label or
 * icon.
 *
 * Returns: (transfer none): a #GMenuModel
 */
GMenuModel *
valent_device_get_menu (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return G_MENU_MODEL (device->menu);
}

/**
 * valent_device_get_name:
 * @device: a #ValentDevice
 *
 * Gets the name of the device, or %NULL if unset.
 *
 * Returns: (transfer none): the name, or NULL.
 */
const char *
valent_device_get_name (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  return device->name;
}

/**
 * valent_device_get_paired:
 * @device: a #ValentDevice
 *
 * Whether the device is paired.
 *
 * Returns: %TRUE if the device is paired.
 */
gboolean
valent_device_get_paired (ValentDevice *device)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), FALSE);

  return device->paired;
}

/**
 * valent_device_set_paired:
 * @device: a #ValentDevice
 * @paired: boolean
 *
 * Set the paired state for @device.
 *
 * NOTE: since valent_device_update_plugins() will be called as a side effect,
 * this must be called after valent_device_send_pair().
 */
void
valent_device_set_paired (ValentDevice *device,
                          gboolean      paired)
{
  g_assert (VALENT_IS_DEVICE (device));

  /* If nothing's changed, only reset pending pair timeouts */
  valent_device_reset_pair (device);

  if (device->paired == paired)
    return;

  /* FIXME: If we're connected store/clear connection data */
  if (paired && device->channel != NULL)
    valent_channel_store_data (device->channel, device->data);
  else if (!paired)
    valent_data_clear_data (device->data);

  /* Ensure plugins are updated before emitting */
  device->paired = paired;
  valent_device_update_plugins (device);

  /* Notify */
  g_settings_set_boolean (device->settings, "paired", paired);
  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_PAIRED]);
  valent_object_notify_by_pspec (G_OBJECT (device), properties [PROP_STATE]);
}

/**
 * valent_device_get_plugins:
 * @device: a #ValentDevice
 *
 * Get a list of the loaded plugins for @device.
 *
 * Returns: (transfer container) (element-type Peas.PluginInfo): a #GPtrArray
 */
GPtrArray *
valent_device_get_plugins (ValentDevice *device)
{
  GHashTableIter iter;
  gpointer info;
  GPtrArray *plugins;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  plugins = g_ptr_array_new ();

  g_hash_table_iter_init (&iter, device->plugins);

  while (g_hash_table_iter_next (&iter, &info, NULL))
    g_ptr_array_add (plugins, info);

  return plugins;
}

/**
 * valent_device_get_state:
 * @device: a #ValentDevice
 *
 * Get the state of the device.
 *
 * Returns: %TRUE if the device is paired.
 */
ValentDeviceState
valent_device_get_state (ValentDevice *device)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), state);

  if (device->connected)
    state |= VALENT_DEVICE_STATE_CONNECTED;

  if (device->paired)
    state |= VALENT_DEVICE_STATE_PAIRED;

  if (device->incoming_pair > 0)
    state |= VALENT_DEVICE_STATE_PAIR_INCOMING;

  if (device->outgoing_pair > 0)
    state |= VALENT_DEVICE_STATE_PAIR_OUTGOING;

  return state;
}

/**
 * valent_device_handle_packet:
 * @device: a #ValentDevice
 * @packet: a #JsonNode packet
 *
 * Take @packet and handle it as a packet from the remote device represented by
 * @device. Identity and pair packets are handled by @device, while all others
 * will be passed to plugins which claim to support the @packet type.
 *
 * Plugin handlers must hold their own reference on @packet if doing anything
 * asynchronous.
 */
void
valent_device_handle_packet (ValentDevice *device,
                             JsonNode     *packet)
{
  ValentDevicePlugin *handler;
  const char *type;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_PACKET (packet));

  VALENT_DEBUG_PKT (packet, device->name);

  type = valent_packet_get_type (packet);

  /* Keep this order */
  if G_UNLIKELY (g_strcmp0 (type, "kdeconnect.identity") == 0)
    valent_device_handle_identity (device, packet);

  else if G_UNLIKELY (g_strcmp0 (type, "kdeconnect.pair") == 0)
    valent_device_handle_pair (device, packet);

  else if G_UNLIKELY (!device->paired)
    valent_device_send_pair (device, FALSE);

  else if ((handler = g_hash_table_lookup (device->handlers, type)))
    valent_device_plugin_handle_packet (handler, type, packet);

  else
    g_debug ("%s: Unsupported packet '%s'", device->name, type);
}

/**
 * valent_device_new_download_file:
 * @device: a #ValentDevice
 * @filename: (type filename): a filename
 * @unique: whether to ensure a unique file
 *
 * Get a new #GFile for @filename in the download directory for @ device. If
 * @unique is %TRUE, the returned file is guaranteed not to be an existing
 * filename by appending `(#)`.
 *
 * As a side-effect, this method will try to ensure the directory exists before
 * returning.
 *
 * Returns: (transfer full) (nullable): a #GFile
 */
GFile *
valent_device_new_download_file (ValentDevice *device,
                                 const char   *filename,
                                 gboolean      unique)
{
  g_autofree char *dirname = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  dirname = g_settings_get_string (device->settings, "download-folder");

  if (strlen (dirname) == 0)
    {
      g_clear_pointer (&dirname, g_free);
      dirname = valent_data_get_directory (G_USER_DIRECTORY_DOWNLOAD);
    }
  else if (g_mkdir_with_parents (dirname, 0700) == -1)
    {
      int error = errno;
      g_warning ("%s: %s", G_STRFUNC, g_strerror (error));
    }

  return valent_data_get_file (dirname, filename, unique);
}

/**
 * valent_device_reload_plugins:
 * @device: a #ValentDevice
 *
 * Check each available plugin and load or unload them if the required
 * capabilities have changed.
 */
static void
valent_device_reload_plugins (ValentDevice *device)
{
  const GList *plugins = NULL;

  g_assert (VALENT_IS_DEVICE (device));

  plugins = peas_engine_get_plugin_list (device->engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (valent_device_supports_plugin (device, iter->data))
        on_load_plugin (device->engine, iter->data, device);
      else
        on_unload_plugin (device->engine, iter->data, device);
    }
}

/**
 * valent_device_update_plugins:
 * @device: a #ValentDevice
 *
 * Call valent_device_plugin_update_state() on each enabled plugin.
 */
static void
valent_device_update_plugins (ValentDevice *device)
{
  GHashTableIter iter;
  gpointer value;

  g_assert (VALENT_IS_DEVICE (device));

  g_hash_table_iter_init (&iter, device->plugins);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      PeasExtension *extension = ((DevicePlugin *)value)->extension;

      if (extension != NULL)
        valent_device_plugin_update_state (VALENT_DEVICE_PLUGIN (extension));
    }
}

/**
 * valent_device_supports_plugin:
 * @device: a #ValentDevice
 * @info: a #PeasPluginInfo
 *
 * Check if @device support the plugin described by @info.
 *
 * Returns: %TRUE if supported
 */
gboolean
valent_device_supports_plugin (ValentDevice   *device,
                               PeasPluginInfo *info)
{
  const char **device_incoming;
  const char **device_outgoing;
  const char *in_str, *out_str;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (info != NULL);

  if (!peas_engine_provides_extension (device->engine,
                                       info,
                                       VALENT_TYPE_DEVICE_PLUGIN))
    return FALSE;

  /* Packet-less plugins aren't dependent on device capabilities */
  in_str = peas_plugin_info_get_external_data (info, "IncomingCapabilities");
  out_str = peas_plugin_info_get_external_data (info, "OutgoingCapabilities");

  if (in_str == NULL && out_str == NULL)
    return TRUE;

  /* Device hasn't supplied an identity packet yet */
  device_incoming = (const char **)device->incoming_capabilities;
  device_outgoing = (const char **)device->outgoing_capabilities;

  if (device_incoming == NULL || device_outgoing == NULL)
    return FALSE;

  /* Check if outgoing from plugin matches incoming from device */
  if (out_str != NULL)
    {
      g_auto (GStrv) plugin_outgoing = NULL;

      plugin_outgoing = g_strsplit(out_str, ";", -1);

      for (int i = 0; plugin_outgoing[i]; i++)
        {
          if (g_strv_contains (device_incoming, plugin_outgoing[i]))
            return TRUE;
        }
    }

  /* Check if incoming from plugin matches outgoing from device */
  if (in_str != NULL)
    {
      g_auto (GStrv) plugin_incoming = NULL;

      plugin_incoming = g_strsplit(in_str, ";", -1);

      for (int i = 0; plugin_incoming[i]; i++)
        {
          if (g_strv_contains (device_outgoing, plugin_incoming[i]))
            return TRUE;
        }
    }

  return FALSE;
}

