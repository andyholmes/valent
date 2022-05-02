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
#include "valent-object.h"
#include "valent-packet.h"
#include "valent-utils.h"


/**
 * ValentChannelService:
 *
 * An abstract base class for connection backends.
 *
 * #ValentChannelService is a base class for plugins that implement an interface
 * to negotiate connections with other devices.
 *
 * ## Implementation Notes
 *
 * Implementations should override [vfunc@Valent.ChannelService.start] to
 * provide failable initialization and [vfunc@Valent.ChannelService.stop] as an
 * idempotent cleanup function. Implementations may safely emit
 * [signal@Valent.ChannelService::channel] from any thread.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-ChannelProtocol`
 *
 *     A string indicating the transport protocol. This is used to filter device
 *     plugins when constructing the identity packet (eg. SFTP).
 *
 *     Currently recognized values are `tcp` and `bluetooth`. If the plugin is
 *     missing this key, it will be assumed supported.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;

  ValentData     *data;
  char           *id;
  JsonNode       *identity;
  char           *name;
} ValentChannelServicePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentChannelService, valent_channel_service, VALENT_TYPE_OBJECT);

/**
 * ValentChannelServiceClass:
 * @build_identity: the virtual function pointer for valent_channel_service_build_identity()
 * @identify: the virtual function pointer for valent_channel_service_identify()
 * @start: the virtual function pointer for valent_channel_service_start()
 * @start_finish: the virtual function pointer for valent_channel_service_start_finish()
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
  PROP_NAME,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  CHANNEL,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };
static GRecMutex channel_lock;


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

  packets = g_string_new ("");

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
  GWeakRef       service;
  ValentChannel *channel;
} ChannelEmission;

static gboolean
valent_channel_service_emit_channel_main (gpointer data)
{
  ChannelEmission *emission = data;
  g_autoptr (ValentChannelService) service = NULL;

  g_rec_mutex_lock (&channel_lock);

  if ((service = g_weak_ref_get (&emission->service)) != NULL)
    valent_channel_service_emit_channel (service, emission->channel);

  g_weak_ref_clear (&emission->service);
  g_clear_object (&emission->channel);
  g_clear_pointer (&emission, g_free);

  g_rec_mutex_unlock (&channel_lock);

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
  g_autoptr (JsonBuilder) builder = NULL;
  g_auto (GStrv) incoming = NULL;
  g_auto (GStrv) outgoing = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));

  /* Filter plugins */
  engine = valent_get_engine ();
  plugins = peas_engine_get_plugin_list (engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (valent_channel_service_supports_plugin (service, iter->data))
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
    json_builder_add_string_value (builder, priv->name);
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
  valent_object_lock (VALENT_OBJECT (service));
  g_clear_pointer (&priv->identity, json_node_unref);
  priv->identity = json_builder_get_root (builder);
  valent_object_unlock (VALENT_OBJECT (service));
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

static gboolean
valent_channel_service_real_start_finish (ValentChannelService  *service,
                                          GAsyncResult          *result,
                                          GError               **error)
{
  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (g_task_is_valid (result, service));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
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
  g_clear_pointer (&priv->name, g_free);

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
      g_value_take_boxed (value, valent_channel_service_ref_identity (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, priv->name);
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

    case PROP_NAME:
      valent_channel_service_set_name (self, g_value_get_string (value));
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
  service_class->start_finish = valent_channel_service_real_start_finish;
  service_class->stop = valent_channel_service_real_stop;

  /**
   * ValentChannelService:data:
   *
   * The data context.
   *
   * Since: 1.0
   */
  properties [PROP_DATA] =
    g_param_spec_object ("data",
                         "Data",
                         "The data context",
                         VALENT_TYPE_DATA,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannelService:id: (getter dup_id)
   *
   * The local ID.
   *
   * This is the ID used to identify the local device, which should be unique
   * among devices in a given network.
   *
   * This property is thread-safe. Emissions of [signal@GObject.Object::notify]
   * are guaranteed to happen in the main thread.
   *
   * Since: 1.0
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "ID",
                         "The local ID",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannelService:identity: (getter ref_identity)
   *
   * The local identity packet.
   *
   * This is the identity packet sent by the [class@Valent.ChannelService]
   * implementation to describe the local device.
   *
   * This property is thread-safe. Emissions of [signal@GObject.Object::notify]
   * are guaranteed to happen in the main thread.
   *
   * Since: 1.0
   */
  properties [PROP_IDENTITY] =
    g_param_spec_boxed ("identity",
                        "Identity",
                        "The local identity packet",
                        JSON_TYPE_NODE,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannelService:name: (getter get_name) (setter set_name)
   *
   * The local display name.
   *
   * This is the user-visible label packet used to identify the local device in
   * user interfaces.
   *
   * This property is thread-safe. Emissions of [signal@GObject.Object::notify]
   * are guaranteed to happen in the main thread.
   *
   * Since: 1.0
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The display name of the local device",
                         "Valent",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentChannelService:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this channel service.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "The plugin info describing this channel service",
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
   * Emitted when a new channel has been negotiated.
   *
   * In practice, when this is emitted a [class@Valent.DeviceManager] will
   * ensure a [class@Valent.Device] exists to take ownership of @channel.
   *
   * Since: 1.0
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
}

/**
 * valent_channel_service_dup_id: (get-property id)
 * @service: a #ValentChannelService
 *
 * Get the local ID.
 *
 * Returns: (transfer full) (not nullable): the service ID
 *
 * Since: 1.0
 */
char *
valent_channel_service_dup_id (ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);
  char *ret;

  g_return_val_if_fail (VALENT_IS_CHANNEL_SERVICE (service), NULL);

  valent_object_lock (VALENT_OBJECT (service));
  ret = g_strdup (priv->id);
  valent_object_unlock (VALENT_OBJECT (service));

  return g_steal_pointer (&ret);
}

/**
 * valent_channel_service_ref_identity: (get-property identity)
 * @service: a #ValentChannelService
 *
 * Get the local identity packet.
 *
 * Returns: (transfer full): a KDE Connect packet
 *
 * Since: 1.0
 */
JsonNode *
valent_channel_service_ref_identity (ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);
  JsonNode *ret;

  g_return_val_if_fail (VALENT_IS_CHANNEL_SERVICE (service), NULL);

  valent_object_lock (VALENT_OBJECT (service));
  ret = json_node_ref (priv->identity);
  valent_object_unlock (VALENT_OBJECT (service));

  return ret;
}

/**
 * valent_channel_service_get_name: (get-property name)
 * @service: a #ValentChannelService
 *
 * Get the local display name.
 *
 * Returns: (transfer none): the local display name
 *
 * Since: 1.0
 */
const char *
valent_channel_service_get_name (ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);

  g_return_val_if_fail (VALENT_IS_CHANNEL_SERVICE (service), NULL);

  return priv->name;
}

/**
 * valent_channel_service_set_name: (set-property name)
 * @service: a #ValentChannelService
 * @name: (not nullable): a display name
 *
 * Set the local display name.
 *
 * Since: 1.0
 */
void
valent_channel_service_set_name (ValentChannelService *service,
                                 const char           *name)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);
  JsonObject *body;

  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));
  g_return_if_fail (name != NULL && *name != '\0');

  if (g_strcmp0 (priv->name, name) == 0)
    return;

  g_clear_pointer (&priv->name, g_free);
  priv->name = g_strdup (name);
  g_object_notify_by_pspec (G_OBJECT (service), properties [PROP_NAME]);

  valent_object_lock (VALENT_OBJECT (service));
  if (priv->identity)
    {
      body = valent_packet_get_body (priv->identity);
      json_object_set_string_member (body, "deviceName", priv->name);
      g_object_notify_by_pspec (G_OBJECT (service), properties [PROP_IDENTITY]);
    }
  valent_object_unlock (VALENT_OBJECT (service));
}

/**
 * valent_channel_service_build_identity: (virtual build_identity)
 * @service: a #ValentChannelService
 *
 * Rebuild the local KDE Connect identity packet.
 *
 * This method is called to rebuild the identity packet used to identify the
 * host device to remote devices.
 *
 * Implementations that override [vfunc@Valent.ChannelService.build_identity]
 * should chain-up first, then call [method@Valent.ChannelService.ref_identity]
 * and modify that.
 *
 * Since: 1.0
 */
void
valent_channel_service_build_identity (ValentChannelService *service)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));

  VALENT_CHANNEL_SERVICE_GET_CLASS (service)->build_identity (service);

  VALENT_EXIT;
}

/**
 * valent_channel_service_identify: (virtual identify)
 * @service: a #ValentChannelService
 * @target: (nullable): a target string
 *
 * Identify the host device to the network.
 *
 * This method is called to announce the availability of the host device to
 * other devices.
 *
 * Implementations that override [vfunc@Valent.ChannelService.identify] may
 * ignore @target or use it to address a particular device.
 *
 * Since: 1.0
 */
void
valent_channel_service_identify (ValentChannelService *service,
                                 const char           *target)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));

  VALENT_CHANNEL_SERVICE_GET_CLASS (service)->identify (service, target);

  VALENT_EXIT;
}

/**
 * valent_channel_service_start: (virtual start)
 * @service: a #ValentChannelService
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Start the service.
 *
 * This method is called by the #ValentManager singleton when a
 * #ValentChannelService implementation is enabled. It is therefore a programmer
 * error for an API user to call this method.
 *
 * Before this operation completes valent_channel_service_stop() may be called,
 * so implementations may want to chain to @cancellable.
 *
 * Since: 1.0
 */
void
valent_channel_service_start (ValentChannelService *service,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_CHANNEL_SERVICE_GET_CLASS (service)->start (service,
                                                     cancellable,
                                                     callback,
                                                     user_data);

  VALENT_EXIT;
}

/**
 * valent_channel_service_start_finish: (virtual start_finish)
 * @service: a #ValentChannelService
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.ChannelService.start].
 *
 * Since: 1.0
 */
gboolean
valent_channel_service_start_finish (ValentChannelService  *service,
                                     GAsyncResult          *result,
                                     GError               **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CHANNEL_SERVICE (service), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, service), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_CHANNEL_SERVICE_GET_CLASS (service)->start_finish (service,
                                                                  result,
                                                                  error);

  VALENT_RETURN (ret);
}

/**
 * valent_channel_service_stop: (virtual stop)
 * @service: a #ValentChannelService
 *
 * Stop the service.
 *
 * Stop processing incoming identity packets and prevent the broadcast of the
 * local identity which results in the same behaviour from other devices.
 *
 * Implementations of this method must chain-up and be idempotent; it is called
 * automatically when the last reference to @service is dropped.
 *
 * Since: 1.0
 */
void
valent_channel_service_stop (ValentChannelService *service)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (service));

  VALENT_CHANNEL_SERVICE_GET_CLASS (service)->stop (service);

  VALENT_EXIT;
}

/**
 * valent_channel_service_emit_channel:
 * @service: a #ValentChannelService
 * @channel: a #ValentChannel
 *
 * Emit [signal@Valent.ChannelService::channel] on @service.
 *
 * This method should only be called by implementations of
 * [class@Valent.ChannelService].
 *
 * Since: 1.0
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

  g_rec_mutex_lock (&channel_lock);
  emission = g_new0 (ChannelEmission, 1);
  g_weak_ref_init (&emission->service, service);
  emission->channel = g_object_ref (channel);
  g_rec_mutex_unlock (&channel_lock);

  g_timeout_add (0, valent_channel_service_emit_channel_main, emission);
}

/**
 * valent_channel_service_supports_plugin:
 * @service: a #ValentChannelService
 * @info: a #PeasPluginInfo
 *
 * Check if @service supports @info.
 *
 * This is a convenience for comparing the `X-ChannelProtocol` field between
 * @info and [property@Valent.ChannelService:plugin-info].
 *
 * Returns %TRUE if either @info or [property@Valent.ChannelService:plugin-info]
 * are missing the `X-ChannelProtocol` field or if the fields match. Returns
 * %FALSE if @info specifies a different protocol.
 *
 * Returns: %TRUE if supported, or %FALSE if not
 *
 * Since: 1.0
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

