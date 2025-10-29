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
};

G_DEFINE_FINAL_TYPE (ValentMockChannelService, valent_mock_channel_service, VALENT_TYPE_CHANNEL_SERVICE)

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
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  int sv[2] = { 0, };
  g_autoptr (GSocket) socket = NULL;
  g_autoptr (GSocket) peer_socket = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (GSocketConnection) peer_connection = NULL;
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (ValentChannel) endpoint = NULL;
  g_autofree char *peer_identity_json = NULL;

  g_assert (VALENT_IS_MOCK_CHANNEL_SERVICE (self));

  /* Generate certificates and update the identity packets
   */
  identity = valent_channel_service_ref_identity (service);
  peer_identity_json = json_to_string (identity, FALSE);
  peer_identity = json_from_string (peer_identity_json, NULL);
  peer_certificate = valent_certificate_new_sync (NULL, NULL);
  json_object_set_string_member (valent_packet_get_body (peer_identity),
                                 "deviceId",
                                 valent_certificate_get_common_name (peer_certificate));
  json_object_set_string_member (valent_packet_get_body (peer_identity),
                                 "deviceName",
                                 "Peer Device");

  /* Open a socket pair to back the connections
   */
  g_assert_no_errno (socketpair (AF_UNIX, SOCK_STREAM, 0, sv));

  /* This is the channel associated with the ValentDevice object
   */
  certificate = valent_channel_service_ref_certificate (service);
  socket = g_socket_new_from_fd (sv[0], NULL);
  connection = g_object_new (G_TYPE_SOCKET_CONNECTION,
                             "socket", socket,
                             NULL);
  channel = g_object_new (VALENT_TYPE_MOCK_CHANNEL,
                          "base-stream",      connection,
                          "certificate",      certificate,
                          "identity",         identity,
                          "peer-certificate", peer_certificate,
                          "peer-identity",    peer_identity,
                          NULL);

  /* This is the channel associated with the mock endpoint
   */
  peer_socket = g_socket_new_from_fd (sv[1], NULL);
  peer_connection = g_object_new (G_TYPE_SOCKET_CONNECTION,
                                  "socket", peer_socket,
                                  NULL);
  endpoint = g_object_new (VALENT_TYPE_MOCK_CHANNEL,
                           "base-stream",      peer_connection,
                           "certificate",      peer_certificate,
                           "identity",         peer_identity,
                           "peer-certificate", certificate,
                           "peer-identity",    identity,
                           NULL);
  g_object_set_data_full (G_OBJECT (self),
                          "valent-peer-channel",
                          g_object_ref (endpoint),
                          g_object_unref);

  valent_channel_service_channel (service, channel);
}

/*
 * GObject
 */
static void
valent_mock_channel_service_class_init (ValentMockChannelServiceClass *klass)
{
  ValentChannelServiceClass *service_class = VALENT_CHANNEL_SERVICE_CLASS (klass);

  service_class->identify = valent_mock_channel_service_identify;
}

static void
valent_mock_channel_service_init (ValentMockChannelService *self)
{
}

