// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-channel-service"

#include "config.h"

#include <sys/socket.h>

#include <libpeas.h>
#include <valent.h>

#include "valent-mock-channel.h"
#include "valent-mock-channel-service.h"


struct _ValentMockChannelService
{
  ValentChannelService  parent_instance;

  ValentChannel        *channel;
  ValentChannel        *endpoint;
};

G_DEFINE_FINAL_TYPE (ValentMockChannelService, valent_mock_channel_service, VALENT_TYPE_CHANNEL_SERVICE)

static ValentChannelService *test_instance = NULL;

static const char peer_identity_json[] =
"{                                   "
"  \"id\": 0,                        "
"  \"type\": \"kdeconnect.identity\","
"  \"body\": {                       "
"    \"deviceId\": \"mock-device\",  "
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
"    ]                               "
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
  g_autoptr (JsonNode) identity = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  int fds[2] = { 0, };
  g_autoptr (GSocket) channel_socket = NULL;
  g_autoptr (GSocket) endpoint_socket = NULL;
  g_autoptr (GSocketConnection) channel_connection = NULL;
  g_autoptr (GSocketConnection) endpoint_connection = NULL;

  g_assert (VALENT_IS_MOCK_CHANNEL_SERVICE (self));

  identity = valent_channel_service_ref_identity (service);
  peer_identity = json_from_string (peer_identity_json, NULL);

  g_assert_no_errno (socketpair (AF_UNIX, SOCK_STREAM, 0, fds));

  /* This is the "local" representation of the mock "remote" device */
  channel_socket = g_socket_new_from_fd (fds[0], NULL);
  channel_connection = g_object_new (G_TYPE_SOCKET_CONNECTION,
                                     "socket", channel_socket,
                                     NULL);
  self->channel = g_object_new (VALENT_TYPE_MOCK_CHANNEL,
                                "base-stream",   channel_connection,
                                "identity",      identity,
                                "peer-identity", peer_identity,
                                NULL);
  g_object_add_weak_pointer (G_OBJECT (self->channel),
                             (gpointer)&self->channel);

  /* This is the "remote" representation of our mock "local" service */
  endpoint_socket = g_socket_new_from_fd (fds[1], NULL);
  endpoint_connection = g_object_new (G_TYPE_SOCKET_CONNECTION,
                                      "socket", endpoint_socket,
                                      NULL);
  self->endpoint = g_object_new (VALENT_TYPE_MOCK_CHANNEL,
                                 "base-stream",   endpoint_connection,
                                 "identity",      peer_identity,
                                 "peer-identity", identity,
                                 NULL);
  g_object_add_weak_pointer (G_OBJECT (self->endpoint),
                             (gpointer)&self->endpoint);

  valent_channel_service_channel (service, self->channel);
}


/*
 * ValentObject
 */
static void
valent_mock_channel_service_destroy (ValentObject *object)
{
  ValentMockChannelService *self = VALENT_MOCK_CHANNEL_SERVICE (object);

  g_assert (VALENT_IS_MOCK_CHANNEL_SERVICE (self));

  g_clear_object (&self->endpoint);
  g_clear_object (&self->channel);

  VALENT_OBJECT_CLASS (valent_mock_channel_service_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_mock_channel_service_class_init (ValentMockChannelServiceClass *klass)
{
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentChannelServiceClass *service_class = VALENT_CHANNEL_SERVICE_CLASS (klass);

  vobject_class->destroy = valent_mock_channel_service_destroy;

  service_class->identify = valent_mock_channel_service_identify;
}

static void
valent_mock_channel_service_init (ValentMockChannelService *self)
{
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
 * Get the `ValentMockChannelService` instance.
 *
 * Returns: (transfer none) (nullable): a `ValentChannelService`
 */
ValentChannelService *
valent_mock_channel_service_get_instance (void)
{
  return test_instance;
}

/**
 * valent_mock_channel_service_get_endpoint:
 *
 * Get the endpoint `ValentChannel`.
 *
 * Returns: (transfer none) (nullable): a `ValentChannel`
 */
ValentChannel *
valent_mock_channel_service_get_endpoint (void)
{
  if (test_instance != NULL)
    return VALENT_MOCK_CHANNEL_SERVICE (test_instance)->endpoint;

  return NULL;
}

