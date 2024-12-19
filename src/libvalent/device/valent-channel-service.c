// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-channel-service"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-certificate.h"
#include "valent-channel.h"
#include "valent-channel-service.h"
#include "valent-packet.h"


/**
 * ValentChannelService:
 *
 * An abstract base class for connection backends.
 *
 * `ValentChannelService` is a base class for plugins that implement an interface
 * to negotiate connections with other devices.
 *
 * ## Implementation Notes
 *
 * Implementations may safely invoke [method@Valent.ChannelService.channel] from
 * any thread; it is guaranteed to be emitted in the main thread.
 *
 * ## `.plugin` File
 *
 * Channel services have no special fields in the `.plugin` file.
 *
 * Since: 1.0
 */

typedef struct
{
  GTlsCertificate *certificate;
  const char      *id;
  JsonNode        *identity;
  char            *name;
  GSettings       *settings;
} ValentChannelServicePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentChannelService, valent_channel_service, VALENT_TYPE_EXTENSION);

typedef enum {
  PROP_CERTIFICATE = 1,
  PROP_ID,
  PROP_IDENTITY,
} ValentChannelServiceProperty;

static GParamSpec *properties[PROP_IDENTITY + 1] = { NULL, };

typedef enum {
  CHANNEL,
} ValentChannelServiceSignal;

static guint signals[CHANNEL + 1] = { 0, };


typedef struct
{
  GRecMutex      lock;
  GWeakRef       service;
  ValentChannel *channel;
} ChannelEmission;

static gboolean
valent_channel_service_channel_main (gpointer data)
{
  ChannelEmission *emission = data;
  g_autoptr (ValentChannelService) service = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());

  g_rec_mutex_lock (&emission->lock);
  if ((service = g_weak_ref_get (&emission->service)) != NULL)
    valent_channel_service_channel (service, emission->channel);

  g_weak_ref_clear (&emission->service);
  g_clear_object (&emission->channel);
  g_rec_mutex_unlock (&emission->lock);
  g_rec_mutex_clear (&emission->lock);
  g_clear_pointer (&emission, g_free);

  return G_SOURCE_REMOVE;
}

/*
 * Identity Packet Helpers
 */
static const char *
get_chassis_type (void)
{
  static size_t guard = 0;
  static char *chassis = NULL;

  if (g_once_init_enter (&guard))
    {
      g_autoptr (GDBusConnection) connection = NULL;
      g_autoptr (GVariant) reply = NULL;
      g_autofree char *str = NULL;
      uint64_t type;

      connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

      if (connection == NULL)
        VALENT_GOTO (dmi_fallback);

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
          g_variant_get (value, "s", &chassis);

          /* NOTE: "phone" is the KDE Connect deviceType */
          if (g_str_equal (chassis, "handset"))
            g_set_str (&chassis, "phone");

          VALENT_GOTO (leave);
        }

    /* Fallback to DMI. See the SMBIOS Specification 3.0 section 7.4.1:
     * https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.0.0.pdf
     */
    dmi_fallback:
      if (!g_file_get_contents ("/sys/class/dmi/id/chassis_type", &str, NULL, NULL) ||
          !g_ascii_string_to_unsigned (str, 10, 0, G_MAXUINT64, &type, NULL))
        type = 0x3;

      switch (type)
        {
        case 0x3: /* Desktop */
        case 0x4: /* Low Profile Desktop */
        case 0x6: /* Mini Tower */
        case 0x7: /* Tower */
          g_set_str (&chassis, "desktop");
          break;

        case 0x8: /* Portable */
        case 0x9: /* Laptop */
        case 0xA: /* Notebook */
        case 0xE: /* Sub Notebook */
          g_set_str (&chassis, "laptop");
          break;

        case 0xB: /* Hand Held */
          g_set_str (&chassis, "phone");
          break;

        case 0x1E: /* Tablet */
          g_set_str (&chassis, "tablet");
          break;

        default:
          g_set_str (&chassis, "desktop");
        }

    leave:
      g_once_init_leave (&guard, 1);
    }

  return chassis;
}

/**
 * collect_capabilities:
 * @info: a `PeasPluginInfo`
 * @incoming: a `GHashTable`
 * @outgoing: a `GHashTable`
 *
 * Collect the capabilities from @info and add them to @incoming and @outgoing,
 * using g_hash_table_add() to coalesce duplicates.
 */
static inline void
collect_capabilities (PeasPluginInfo *info,
                      GHashTable     *incoming,
                      GHashTable     *outgoing)
{
  const char *data = NULL;

  if ((data = peas_plugin_info_get_external_data (info, "DevicePluginIncoming")) != NULL)
    {
      g_autofree char **capabilities = NULL;

      capabilities = g_strsplit (data, ";", -1);

      for (unsigned int i = 0; capabilities[i] != NULL; i++)
        g_hash_table_add (incoming, g_steal_pointer (&capabilities[i]));
    }

  if ((data = peas_plugin_info_get_external_data (info, "DevicePluginOutgoing")) != NULL)
    {
      g_autofree char **capabilities = NULL;

      capabilities = g_strsplit (data, ";", -1);

      for (unsigned int i = 0; capabilities[i] != NULL; i++)
        g_hash_table_add (outgoing, g_steal_pointer (&capabilities[i]));
    }
}

static void
on_device_name_changed (GSettings            *settings,
                        const char           *key_name,
                        ValentChannelService *self)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (self);
  g_autofree char *name = NULL;

  g_return_if_fail (VALENT_IS_CHANNEL_SERVICE (self));

  name = g_settings_get_string (settings, "name");
  if (name == NULL || *name == '\0')
    {
      g_settings_set_string (settings, "name", g_get_host_name ());
      return;
    }

  if (g_set_str (&priv->name, name))
    {
      valent_object_lock (VALENT_OBJECT (self));
      if (priv->identity)
        {
          JsonObject *body = valent_packet_get_body (priv->identity);
          json_object_set_string_member (body, "deviceName", priv->name);
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IDENTITY]);
        }
      valent_object_unlock (VALENT_OBJECT (self));
    }
}

/* LCOV_EXCL_START */
static void
valent_channel_service_real_build_identity (ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);
  PeasEngine *engine;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (GHashTable) incoming = NULL;
  g_autoptr (GHashTable) outgoing = NULL;
  GHashTableIter in_iter, out_iter;
  const char *capability = NULL;
  unsigned int n_plugins = 0;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));

  /* Filter the supported plugins and collect their capabilities */
  incoming = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  outgoing = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  engine = valent_get_plugin_engine ();
  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (engine));

  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) info = NULL;

      info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      collect_capabilities (info, incoming, outgoing);
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

  g_hash_table_iter_init (&in_iter, incoming);

  while (g_hash_table_iter_next (&in_iter, (void **)&capability, NULL))
    json_builder_add_string_value (builder, capability);

  json_builder_end_array (builder);

  /* Outgoing Capabilities */
  json_builder_set_member_name (builder, "outgoingCapabilities");
  json_builder_begin_array (builder);

  g_hash_table_iter_init (&out_iter, outgoing);

  while (g_hash_table_iter_next (&out_iter, (void **)&capability, NULL))
    json_builder_add_string_value (builder, capability);

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
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_channel_service_constructed (GObject *object)
{
  ValentChannelService *self = VALENT_CHANNEL_SERVICE (object);
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (self);

  G_OBJECT_CLASS (valent_channel_service_parent_class)->constructed (object);

  if (priv->certificate == NULL)
    {
      ValentResource *source = valent_data_source_get_local_default ();
      g_autoptr (GFile) file = NULL;
      g_autoptr (GError) error = NULL;

      // FIXME
      file = valent_data_source_get_config_file (VALENT_DATA_SOURCE (source), "..");
      priv->certificate = valent_certificate_new_sync (g_file_peek_path (file),
                                                       &error);
      if (priv->certificate == NULL)
        g_critical ("%s(): %s", G_STRFUNC, error->message);
    }

  priv->id = valent_certificate_get_common_name (priv->certificate);
  priv->settings = g_settings_new ("ca.andyholmes.Valent");
  g_signal_connect_object (priv->settings,
                           "changed::name",
                           G_CALLBACK (on_device_name_changed),
                           self,
                           G_CONNECT_DEFAULT);
  on_device_name_changed (priv->settings, NULL, self);

  valent_channel_service_build_identity (self);
}

static void
valent_channel_service_finalize (GObject *object)
{
  ValentChannelService *self = VALENT_CHANNEL_SERVICE (object);
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (self);

  g_clear_object (&priv->certificate);
  g_clear_pointer (&priv->identity, json_node_unref);
  g_clear_pointer (&priv->name, g_free);
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

  switch ((ValentChannelServiceProperty)prop_id)
    {
    case PROP_CERTIFICATE:
      g_value_take_object (value, valent_channel_service_ref_certificate (self));
      break;

    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;

    case PROP_IDENTITY:
      g_value_take_boxed (value, valent_channel_service_ref_identity (self));
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

  switch ((ValentChannelServiceProperty)prop_id)
    {
    case PROP_CERTIFICATE:
      priv->certificate = g_value_dup_object (value);
      break;

    case PROP_ID:
    case PROP_IDENTITY:
      g_assert_not_reached ();

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
  object_class->finalize = valent_channel_service_finalize;
  object_class->get_property = valent_channel_service_get_property;
  object_class->set_property = valent_channel_service_set_property;

  service_class->build_identity = valent_channel_service_real_build_identity;
  service_class->identify = valent_channel_service_real_identify;

  /**
   * ValentChannelService:certificate: (getter ref_certificate)
   *
   * The TLS certificate the service uses to authenticate with other devices.
   */
  properties [PROP_CERTIFICATE] =
    g_param_spec_object ("certificate", NULL, NULL,
                         G_TYPE_TLS_CERTIFICATE,
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
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
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
    g_param_spec_boxed ("identity", NULL, NULL,
                        JSON_TYPE_NODE,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  /**
   * ValentChannelService::channel:
   * @service: a `ValentChannelService`
   * @channel: a `ValentChannel`
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
 * valent_channel_service_ref_certificate: (get-property certificate)
 * @self: a `ValentChannelService`
 *
 * Get the TLS certificate for the service.
 *
 * Returns: (transfer full) (not nullable): the service TLS certificate
 */
GTlsCertificate *
valent_channel_service_ref_certificate (ValentChannelService *service)
{
  ValentChannelServicePrivate *priv = valent_channel_service_get_instance_private (service);
  GTlsCertificate *ret = NULL;

  g_return_val_if_fail (VALENT_IS_CHANNEL_SERVICE (service), NULL);

  valent_object_lock (VALENT_OBJECT (service));
  ret = g_object_ref (priv->certificate);
  valent_object_unlock (VALENT_OBJECT (service));

  return g_steal_pointer (&ret);
}

/**
 * valent_channel_service_dup_id: (get-property id)
 * @service: a `ValentChannelService`
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
 * @service: a `ValentChannelService`
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

  return g_steal_pointer (&ret);
}

/**
 * valent_channel_service_build_identity: (virtual build_identity)
 * @service: a `ValentChannelService`
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

  valent_object_lock (VALENT_OBJECT (service));
  VALENT_CHANNEL_SERVICE_GET_CLASS (service)->build_identity (service);
  valent_object_unlock (VALENT_OBJECT (service));

  VALENT_EXIT;
}

/**
 * valent_channel_service_identify: (virtual identify)
 * @service: a `ValentChannelService`
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
 * valent_channel_service_channel:
 * @service: a `ValentChannelService`
 * @channel: a `ValentChannel`
 *
 * Emit [signal@Valent.ChannelService::channel] on @service.
 *
 * This method should only be called by implementations of
 * [class@Valent.ChannelService].
 *
 * Since: 1.0
 */
void
valent_channel_service_channel (ValentChannelService *service,
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
  g_rec_mutex_init (&emission->lock);
  g_rec_mutex_lock (&emission->lock);
  g_weak_ref_init (&emission->service, service);
  emission->channel = g_object_ref (channel);
  g_rec_mutex_unlock (&emission->lock);

  g_timeout_add (0, valent_channel_service_channel_main, emission);
}

