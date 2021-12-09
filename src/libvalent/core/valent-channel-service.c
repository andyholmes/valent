// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-channel-service"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>

#include "valent-channel.h"
#include "valent-channel-service.h"
#include "valent-data.h"
#include "valent-debug.h"
#include "valent-macros.h"
#include "valent-object-utils.h"
#include "valent-packet.h"
#include "valent-utils.h"


/**
 * SECTION:valentchannelservice
 * @short_description: Base class for channel services
 * @title: ValentChannelService
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * #ValentChannelService is base class for services that provide #ValentChannel
 * objects, analagous to to #GSocketService and #GSocketConnection. In the
 * context of the protocol, #ValentChannelService serves as the counterpart to
 * #ValentDevice by representing the local device.
 *
 * The interface is intentionally abstract, to allow channel services for very
 * different protocols. All that is required is for the service to emit
 * #ValentChannelService::channel when a #ValentChannel is ready to be used by a
 * #ValentDevice.
 *
 * # `.plugin` File
 *
 * #ValentChannelService implementations should define the `X-ChannelProtocol`
 * key to indicate the underlying protocol. This is used when constructing the
 * identity packet to filter device plugins with inherent requirements, such as
 * SFTP.
 *
 * Currently recognized values are `tcp` and `bluetooth`. If the plugin is
 * missing this key, it will be assumed supported and its incoming and outgoing
 * capabilities included in the identity packet.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;

  GSettings      *settings;
  GCancellable   *cancellable;

  ValentData     *data;
  char           *id;
  JsonNode       *identity;
} ValentChannelServicePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentChannelService, valent_channel_service, G_TYPE_OBJECT);

/**
 * ValentChannelServiceClass:
 * @build_identity: the virtual function pointer for valent_channel_service_build_identity()
 * @identify: the virtual function pointer for valent_channel_service_identify()
 * @start: the virtual function pointer for valent_channel_service_start()
 * @stop: the virtual function pointer for valent_channel_service_stop()
 * @channel: the class closure for #ValentChannelService::channel
 *
 * The virtual function table for #ValentChannelService.
 */

enum {
  PROP_0,
  PROP_DATA,
  PROP_ID,
  PROP_IDENTITY,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  CHANNEL,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/*
 * Identity Packet Helpers
 */
static char *chassis = NULL;

static void
init_chassis_type (void)
{
  static gsize guard;
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GVariant) reply = NULL;
  g_autofree char *str = NULL;
  guint64 type;

  if (!g_once_init_enter (&guard))
    return;

  /* Prefer org.freedesktop.hostname1 */
  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

  if (connection != NULL)
    reply = g_dbus_connection_call_sync (connection,
                                         "org.freedesktop.hostname1",
                                         "/org/freedesktop/hostname1",
                                         "org.freedesktop.DBus.Properties",
                                         "Get",
                                         g_variant_new ("(ss)",
                                                        "org.freedesktop.hostname1",
                                                        "Chassis"),
                                         G_VARIANT_TYPE ("(v)"),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         NULL);

  if (reply != NULL)
    {
      g_autoptr (GVariant) value = NULL;

      g_variant_get (reply, "(v)", &value);
      g_variant_get (value, "s", &str);

      if (g_str_equal (str, "handset"))
        chassis = "phone";
      else
        chassis = g_steal_pointer (&str);

      VALENT_GOTO (out);
    }

  /* Fallback to DMI. See the SMBIOS Specification 3.0 section 7.4.1:
   * https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.0.0.pdf
   */
  if (!g_file_get_contents ("/sys/class/dmi/id/chassis_type", &str, NULL, NULL) ||
      !g_ascii_string_to_unsigned (str, 10, 0, G_MAXUINT64, &type, NULL))
    type = 0x3;

  switch (type)
    {
    case 0x3: /* Desktop */
    case 0x4: /* Low Profile Desktop */
    case 0x6: /* Mini Tower */
    case 0x7: /* Tower */
      chassis = "desktop";
      break;

    case 0x8: /* Portable */
    case 0x9: /* Laptop */
    case 0xA: /* Notebook */
    case 0xE: /* Sub Notebook */
      chassis = "laptop";
      break;

    case 0xB: /* Hand Held */
      chassis = "phone";
      break;

    case 0x1E: /* Tablet */
      chassis = "tablet";
      break;

    default:
      chassis = "desktop";
    }

  out:
    g_once_init_leave (&guard, 1);
}

static const char *
get_chassis_type (void)
{
  if G_UNLIKELY (chassis == NULL)
    init_chassis_type ();

  return chassis;
}

static void
on_name_changed (GSettings            *settings,
                 const char           *key,
                 ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);
  g_autofree char *name = NULL;
  JsonObject *body;

  name = g_settings_get_string (settings, key);
  body = valent_packet_get_body (priv->identity);
  json_object_set_string_member (body, "deviceName", name);

  valent_object_notify_by_pspec (service, properties [PROP_IDENTITY]);
}

/**
 * collect_capabilities:
 * @info: a #GList
 * @field: the field name
 *
 *
 */
static inline char **
collect_capabilities (const GList *infos,
                      const char  *field)
{
  g_autofree char *tmp = NULL;
  const char *data;
  GString *packets = NULL;

  packets = g_string_new("");

  for (const GList *iter = infos; iter; iter = iter->next)
    {
      data = peas_plugin_info_get_external_data (iter->data, field);

      if (data == NULL)
        continue;

      if G_UNLIKELY (packets->len == 0)
        packets = g_string_append (packets, data);
      else
        g_string_append_printf (packets, ";%s", data);
    }

  tmp = g_string_free (packets, FALSE);

  return g_strsplit (tmp, ";", -1);
}

/*
 * ValentChannelService::channel Helper
 */
typedef struct
{
  ValentChannelService *service;
  ValentChannel        *channel;
} ChannelEmission;

static gboolean
valent_channel_service_emit_channel_main (gpointer data)
{
  ChannelEmission *emission = data;

  valent_channel_service_emit_channel (emission->service, emission->channel);

  g_clear_object (&emission->service);
  g_clear_object (&emission->channel);
  g_free (emission);

  return G_SOURCE_REMOVE;
}

/* LCOV_EXCL_START */
static void
valent_channel_service_real_build_identity (ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);
  PeasEngine *engine;
  const GList *plugins = NULL;
  g_autoptr (GList) supported = NULL;
  g_autofree char *name = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_auto (GStrv) incoming = NULL;
  g_auto (GStrv) outgoing = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));

  engine = valent_get_engine ();
  name = g_settings_get_string (priv->settings, "name");

  /* Filter plugins */
  plugins = peas_engine_get_plugin_list (engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if G_LIKELY (valent_channel_service_supports_plugin (service, iter->data))
        supported = g_list_prepend (supported, iter->data);
    }

  /* Build the identity packet */
  builder = json_builder_new ();
  json_builder_begin_object (builder);

  /* Packet */
  json_builder_set_member_name (builder, "id");
  json_builder_add_int_value (builder, 0);
  json_builder_set_member_name (builder, "type");
  json_builder_add_string_value (builder, "kdeconnect.identity");

  /* Body */
  json_builder_set_member_name (builder, "body");
  json_builder_begin_object (builder);

    /* Metadata */
    json_builder_set_member_name (builder, "deviceId");
    json_builder_add_string_value (builder, priv->id);
    json_builder_set_member_name (builder, "deviceName");
    json_builder_add_string_value (builder, name);
    json_builder_set_member_name (builder, "deviceType");
    json_builder_add_string_value (builder, get_chassis_type());
    json_builder_set_member_name (builder, "protocolVersion");
    json_builder_add_int_value (builder, 7);

    /* Incoming Capabilities */
    json_builder_set_member_name (builder, "incomingCapabilities");
    json_builder_begin_array (builder);

    incoming = collect_capabilities (supported, "X-IncomingCapabilities");

    for (unsigned int i = 0; incoming[i]; i++)
      json_builder_add_string_value (builder, incoming[i]);

    json_builder_end_array (builder);

    /* Outgoing Capabilities */
    json_builder_set_member_name (builder, "outgoingCapabilities");
    json_builder_begin_array (builder);

    outgoing = collect_capabilities (supported, "X-OutgoingCapabilities");

    for (unsigned int i = 0; outgoing[i]; i++)
      json_builder_add_string_value (builder, outgoing[i]);

    json_builder_end_array (builder);

  /* End Body, Packet */
  json_builder_end_object (builder);
  json_builder_end_object (builder);


  /* Store the identity */
  g_clear_pointer (&priv->identity, json_node_unref);
  priv->identity = json_builder_get_root (builder);
}

static void
valent_channel_service_real_identify (ValentChannelService *service,
                                      const char           *target)
{
  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
}

static void
valent_channel_service_real_start (ValentChannelService *service,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  g_task_report_new_error (service, callback, user_data,
                           valent_channel_service_real_start,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement start",
                           G_OBJECT_TYPE_NAME (service));
}

static void
valent_channel_service_real_stop (ValentChannelService *service)
{
  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_channel_service_constructed (GObject *object)
{
  ValentChannelService *self = VALENT_CHANNEL_SERVICE (object);
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (self);

  /* Ensure we have a data manager */
  if (priv->data == NULL)
    priv->data = valent_data_new (NULL, NULL);

  valent_channel_service_build_identity (self);

  g_signal_connect (priv->settings,
                    "changed::name",
                    G_CALLBACK (on_name_changed),
                    self);

  G_OBJECT_CLASS (valent_channel_service_parent_class)->constructed (object);
}

static void
valent_channel_service_dispose (GObject *object)
{
  ValentChannelService *self = VALENT_CHANNEL_SERVICE (object);

  valent_channel_service_stop (self);

  G_OBJECT_CLASS (valent_channel_service_parent_class)->dispose (object);
}

static void
valent_channel_service_finalize (GObject *object)
{
  ValentChannelService *self = VALENT_CHANNEL_SERVICE (object);
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (self);

  g_clear_object (&priv->data);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->identity, json_node_unref);
  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (valent_channel_service_parent_class)->finalize (object);
}

static void
valent_channel_service_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ValentChannelService *self = VALENT_CHANNEL_SERVICE (object);
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DATA:
      g_value_set_object (value, priv->data);
      break;

    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;

    case PROP_IDENTITY:
      g_value_set_boxed (value, priv->identity);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_channel_service_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ValentChannelService *self = VALENT_CHANNEL_SERVICE (object);
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_DATA:
      priv->data = g_value_dup_object (value);
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_channel_service_class_init (ValentChannelServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentChannelServiceClass *service_class = VALENT_CHANNEL_SERVICE_CLASS (klass);

  object_class->constructed = valent_channel_service_constructed;
  object_class->dispose = valent_channel_service_dispose;
  object_class->finalize = valent_channel_service_finalize;
  object_class->get_property = valent_channel_service_get_property;
  object_class->set_property = valent_channel_service_set_property;

  service_class->build_identity = valent_channel_service_real_build_identity;
  service_class->identify = valent_channel_service_real_identify;
  service_class->start = valent_channel_service_real_start;
  service_class->stop = valent_channel_service_real_stop;

  /**
   * ValentChannelService:data:
   *
   * The data manager for the service.
   */
  properties [PROP_DATA] =
    g_param_spec_object ("data",
                         "Data",
                         "The data manager for the service",
                         VALENT_TYPE_DATA,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannelService:id:
   *
   * The local ID string used to uniquely identify the service to the remote
   * device.
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "ID",
                         "The service ID string",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannelService:identity:
   *
   * The local identity packet used to identify the service to the remote device.
   */
  properties [PROP_IDENTITY] =
    g_param_spec_boxed ("identity",
                        "Identity",
                        "Identity",
                        JSON_TYPE_NODE,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannelService:plugin-info:
   *
   * The #PeasPluginInfo describing this channel service.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "Plugin Info",
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentChannelService::channel:
   * @service: a #ValentChannelService
   * @channel: a #ValentChannel
   *
   * Implementations should emit #ValentChannelService::channel when a new
   * connection is successfully made.
   *
   * In practice, when #ValentChannelService is emitted a #ValentManager will
   * have a #ValentDevice to take ownership of the #ValentChannel.
   * Implementations may drop their reference after the signal handlers return
   * unless they have some other reason to track the channel state.
   */
  signals [CHANNEL] =
    g_signal_new ("channel",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentChannelServiceClass, channel),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_CHANNEL);
  g_signal_set_va_marshaller (signals [CHANNEL],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_channel_service_init (ValentChannelService *self)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (self);

  priv->data = NULL;
  priv->id = NULL;
  priv->identity = NULL;
  priv->settings = g_settings_new ("ca.andyholmes.Valent");
}

/**
 * valent_channel_service_get_id:
 * @service: a #ValentChannelService
 *
 * Get the service ID.
 *
 * Returns: (transfer none): the service ID
 */
const char *
valent_channel_service_get_id (ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);

  g_return_val_if_fail (VALENT_IS_CHANNEL_SERVICE (service), NULL);

  return priv->id;
}

/**
 * valent_channel_service_get_identity:
 * @service: a #ValentChannelService
 *
 * Get the identity packet @service will use to identify itself to remote
 * devices.
 *
 * Returns: (transfer none): a #JsonNode
 */
JsonNode *
valent_channel_service_get_identity (ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);

  g_return_val_if_fail (VALENT_IS_CHANNEL_SERVICE (service), NULL);

  return priv->identity;
}

/**
 * valent_channel_service_build_identity: (virtual build_identity)
 * @service: a #ValentChannelService
 *
 * Rebuild the identity packet used to identify the service to other devices.
 * Implementations that plan to modify the default should chain-up, then call
 * valent_channel_service_get_identity() and modify that.
 */
void
valent_channel_service_build_identity (ValentChannelService *service)
{
  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));

  VALENT_CHANNEL_SERVICE_GET_CLASS (service)->build_identity (service);
}

/**
 * valent_channel_service_identify: (virtual identify)
 * @service: a #ValentChannelService
 * @target: (type utf8) (nullable): a target string
 *
 * Identify the local device either to @target if given, or possibly it's
 * respective network as a whole if %NULL.
 *
 * Implementations may ignore @target or perform some transformation on it. For
 * example, #ValentLanChannelService will construct a #GSocketAddress from it or
 * broadcast to the LAN if %NULL.
 */
void
valent_channel_service_identify (ValentChannelService *service,
                                 const char           *target)
{
  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));

  VALENT_CHANNEL_SERVICE_GET_CLASS (service)->identify (service, target);
}

/**
 * valent_channel_service_start: (virtual start)
 * @service: a #ValentChannelService
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Start @service and begin accepting connections.
 *
 * Before this operation completes valent_channel_service_stop() may be called,
 * so implementations may want to chain to @cancellable.
 */
void
valent_channel_service_start (ValentChannelService *service,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);

  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (priv->cancellable != NULL)
    return valent_channel_service_real_start (service,
                                              cancellable,
                                              callback,
                                              user_data);

  priv->cancellable = g_cancellable_new ();

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (g_cancellable_cancel),
                             priv->cancellable,
                             G_CONNECT_SWAPPED);

  VALENT_CHANNEL_SERVICE_GET_CLASS (service)->start (service,
                                                     priv->cancellable,
                                                     callback,
                                                     user_data);
}

/**
 * valent_channel_service_start_finish:
 * @service: a #ValentChannelService
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_channel_service_start().
 */
gboolean
valent_channel_service_start_finish (ValentChannelService  *service,
                                     GAsyncResult          *result,
                                     GError               **error)
{
  g_return_val_if_fail (VALENT_IS_CHANNEL_SERVICE (service), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, service), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * valent_channel_service_stop: (virtual stop)
 * @service: a #ValentChannelService
 *
 * Stop processing incoming identity packets and prevent the broadcast of the
 * local identity which results in the same behaviour from other devices.
 */
void
valent_channel_service_stop (ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);

  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));

  if (priv->cancellable == NULL)
    return;

  g_cancellable_cancel (priv->cancellable);
  VALENT_CHANNEL_SERVICE_GET_CLASS (service)->stop (service);
  g_clear_object (&priv->cancellable);
}

/**
 * valent_channel_service_channel:
 * @service: a #ValentChannelService
 * @channel: a #ValentChannel
 *
 * Emits the #ValentChannelService::channel signal on @service, in the main
 * thread.
 *
 * An implementation of #ValentChannelService should call this when a
 * #ValentChannel has completed the connection process and is ready to be
 * attached to a #ValentDevice.
 */
void
valent_channel_service_emit_channel (ValentChannelService *service,
                                     ValentChannel        *channel)
{
  ChannelEmission *emission;

  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));
  g_return_if_fail (VALENT_IS_CHANNEL (channel));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_signal_emit (G_OBJECT (service), signals [CHANNEL], 0, channel);
      return;
    }

  emission = g_new0 (ChannelEmission, 1);
  emission->service = g_object_ref (service);
  emission->channel = g_object_ref (channel);

  g_timeout_add (0, valent_channel_service_emit_channel_main, emission);
}

/**
 * valent_channel_service_supports_plugin:
 * @service: a #ValentChannelService
 * @info: a #PeasPluginInfo
 *
 * Check if the #ValentDevicePlugin described by @info requires a specific
 * transport protocol (indicated by a `X-ChannelProtocol` field) and if so that
 * @service utilizes that transport protocol.
 *
 * This is useful for plugins that wrap protocols such as SFTP, which requires
 * TCP.
 *
 * Returns: %TRUE if supported
 */
gboolean
valent_channel_service_supports_plugin (ValentChannelService *service,
                                        PeasPluginInfo       *info)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);
  const char *requires;
  const char *provides;

  g_return_val_if_fail (VALENT_IS_CHANNEL_SERVICE (service), FALSE);

  requires = peas_plugin_info_get_external_data (info, "X-ChannelProtocol");

  if (requires == NULL)
    return TRUE;

  provides = peas_plugin_info_get_external_data (priv->plugin_info, "X-ChannelProtocol");

  if (provides == NULL)
    return TRUE;

  return g_strcmp0 (requires, provides) == 0;
}

