// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-channel-service"

#include "config.h"

#include <gio/gunixinputstream.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-mock-channel-service.h"
#include "valent-test-utils.h"


#define DEFAULT_PORT      1717
#define TRANSFER_PORT_MIN 1739
#define TRANSFER_PORT_MAX 1764


struct _ValentMockChannelService
{
  ValentChannelService  parent_instance;

  GCancellable         *cancellable;

  char                 *broadcast_address;
  guint                 port;

  ValentChannel        *channel;
  ValentChannel        *endpoint;
};

G_DEFINE_TYPE (ValentMockChannelService, valent_mock_channel_service, VALENT_TYPE_CHANNEL_SERVICE)

enum {
  PROP_0,
  PROP_BROADCAST_ADDRESS,
  PROP_PORT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static ValentChannelService *test_instance = NULL;

static const char identity_json[] =
"{                                   "
"  \"id\": 0,                        "
"  \"type\": \"kdeconnect.identity\","
"  \"body\": {                       "
"    \"deviceId\": \"test-device\",  "
"    \"deviceName\": \"Mock Device\","
"    \"protocolVersion\": 7,         "
"    \"deviceType\": \"phone\",      "
"    \"incomingCapabilities\": [     "
"      \"kdeconnect.mock.echo\",     "
"      \"kdeconnect.mock.transfer\"  "
"    ],                              "
"    \"outgoingCapabilities\": [     "
"      \"kdeconnect.mock.echo\",     "
"      \"kdeconnect.mock.transfer\"  "
"    ],                              "
"    \"tcpPort\": 1716               "
"  }                                 "
"}                                   ";


/*
 * ValentChannelService
 */
static void
valent_mock_channel_service_identify (ValentChannelService *service,
                                      const char           *target)
{
  ValentMockChannelService *self = VALENT_MOCK_CHANNEL_SERVICE (service);
  g_autofree ValentChannel **channels = NULL;
  JsonNode *identity, *peer_identity;

  g_assert (VALENT_IS_MOCK_CHANNEL_SERVICE (self));

  identity = valent_channel_service_get_identity (service);
  peer_identity = json_from_string (identity_json, NULL);
  channels = valent_test_channels (identity, peer_identity);

  self->channel = g_steal_pointer (&channels[0]);
  self->endpoint = g_steal_pointer (&channels[1]);

  valent_channel_service_emit_channel (service, self->channel);
  json_node_unref (peer_identity);
}

static void
valent_mock_channel_service_start (ValentChannelService *service,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  ValentMockChannelService *self = VALENT_MOCK_CHANNEL_SERVICE (service);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_MOCK_CHANNEL_SERVICE (service));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (service, self->cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_channel_service_start);
  g_task_return_boolean (task, TRUE);
}

static void
valent_mock_channel_service_stop (ValentChannelService *service)
{
  ValentMockChannelService *self = VALENT_MOCK_CHANNEL_SERVICE (service);

  g_assert (VALENT_IS_MOCK_CHANNEL_SERVICE (self));

  if (self->endpoint)
    {
      valent_channel_close (self->endpoint, NULL, NULL);
      g_clear_object (&self->endpoint);
    }

  if (self->channel)
    {
      valent_channel_close_async (self->channel, NULL, NULL, NULL);
      v_assert_finalize_object (self->channel);
      self->channel = NULL;
    }

  g_cancellable_cancel (self->cancellable);
}


/*
 * GObject
 */
static void
valent_mock_channel_service_constructed (GObject *object)
{
  ValentMockChannelService *self = (ValentMockChannelService *)object;

  if (self->broadcast_address == NULL)
    self->broadcast_address = g_strdup ("127.0.0.255");

  G_OBJECT_CLASS (valent_mock_channel_service_parent_class)->constructed (object);
}

static void
valent_mock_channel_service_finalize (GObject *object)
{
  ValentMockChannelService *self = (ValentMockChannelService *)object;

  valent_mock_channel_service_stop (VALENT_CHANNEL_SERVICE (object));
  g_clear_pointer (&self->broadcast_address, g_free);

  G_OBJECT_CLASS (valent_mock_channel_service_parent_class)->finalize (object);
}

static void
valent_mock_channel_service_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  ValentMockChannelService *self = (ValentMockChannelService *)object;

  switch (prop_id)
    {
    case PROP_BROADCAST_ADDRESS:
      g_value_set_string (value, self->broadcast_address);
      break;

    case PROP_PORT:
      g_value_set_uint (value, self->port);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mock_channel_service_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  ValentMockChannelService *self = (ValentMockChannelService *)object;

  switch (prop_id)
    {
    case PROP_BROADCAST_ADDRESS:
      self->broadcast_address = g_value_dup_string (value);
      break;

    case PROP_PORT:
      self->port = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mock_channel_service_class_init (ValentMockChannelServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentChannelServiceClass *service_class = VALENT_CHANNEL_SERVICE_CLASS (klass);

  object_class->constructed = valent_mock_channel_service_constructed;
  object_class->finalize = valent_mock_channel_service_finalize;
  object_class->get_property = valent_mock_channel_service_get_property;
  object_class->set_property = valent_mock_channel_service_set_property;

  service_class->identify = valent_mock_channel_service_identify;
  service_class->start = valent_mock_channel_service_start;
  service_class->stop = valent_mock_channel_service_stop;

  /**
   * ValentMockChannelService:broadcast-address:
   *
   * The UDP broadcast address for the backend.
   *
   * This available as a construct property primarily for use in unit tests.
   */
  properties [PROP_BROADCAST_ADDRESS] =
    g_param_spec_string ("broadcast-address",
                         "Broadcast Address",
                         "The UDP broadcast address for outgoing identity packets",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMockChannelService:port:
   *
   * The TCP/IP port for the backend. The current KDE Connect protocol (v7)
   * defines port 1716 as the default. We use 1717 in the loopback service.
   */
  properties [PROP_PORT] =
    g_param_spec_uint ("port",
                       "Port",
                       "TCP/IP port",
                       0, G_MAXUINT16,
                       DEFAULT_PORT,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mock_channel_service_init (ValentMockChannelService *self)
{
  self->cancellable = NULL;
  self->broadcast_address = NULL;
  self->port = DEFAULT_PORT;

  if (test_instance == NULL)
    {
      test_instance = VALENT_CHANNEL_SERVICE (self);
      g_object_add_weak_pointer (G_OBJECT (test_instance),
                                 (gpointer)&test_instance);
    }
}

/**
 * valent_mock_channel_service_get_instance:
 *
 * Get the #ValentMockChannelService instance.
 *
 * Returns: (transfer none) (nullable): a #ValentChannelService
 */
ValentChannelService *
valent_mock_channel_service_get_instance (void)
{
  return test_instance;
}

/**
 * valent_mock_channel_service_get_channel:
 *
 * Get the local #ValentChannel.
 *
 * Returns: (transfer none) (nullable): a #ValentChannel
 */
ValentChannel *
valent_mock_channel_service_get_channel (void)
{
  if (test_instance != NULL)
    return VALENT_MOCK_CHANNEL_SERVICE (test_instance)->channel;

  return NULL;
}

/**
 * valent_mock_channel_service_get_endpoint:
 *
 * Get the endpoint #ValentChannel.
 *
 * Returns: (transfer none) (nullable): a #ValentChannel
 */
ValentChannel *
valent_mock_channel_service_get_endpoint (void)
{
  if (test_instance != NULL)
    return VALENT_MOCK_CHANNEL_SERVICE (test_instance)->endpoint;

  return NULL;
}

