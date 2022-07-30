// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-channel"

#include "config.h"

#include <gio/gio.h>
#include <gio/gnetworking.h>
#include <json-glib/json-glib.h>
#include <libvalent-core.h>

#include "valent-lan-channel.h"
#include "valent-lan-utils.h"

#define VALENT_LAN_TCP_PORT 1716
#define VALENT_LAN_UDP_PORT 1716
#define VALENT_LAN_AUX_MIN  1739
#define VALENT_LAN_AUX_MAX  1764


struct _ValentLanChannel
{
  ValentChannel    parent_instance;

  GTlsCertificate *certificate;
  char            *description;
  char            *host;
  guint16          port;
};

G_DEFINE_TYPE (ValentLanChannel, valent_lan_channel, VALENT_TYPE_CHANNEL)

enum {
  PROP_0,
  PROP_CERTIFICATE,
  PROP_HOST,
  PROP_PEER_CERTIFICATE,
  PROP_PORT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * ValentChannel
 */
static const char *
valent_lan_channel_get_verification_key (ValentChannel *channel)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (channel);
  g_autoptr (GChecksum) checksum = NULL;
  g_autoptr (GTlsCertificate) cert = NULL;
  g_autoptr (GTlsCertificate) peer_cert = NULL;
  GByteArray *pubkey;
  GByteArray *peer_pubkey;
  size_t cmplen;

  if (self->description != NULL)
    return self->description;

  if ((cert = valent_lan_channel_ref_certificate (self)) == NULL ||
      (peer_cert = valent_lan_channel_ref_peer_certificate (self)) == NULL)
    g_return_val_if_reached (NULL);

  if ((pubkey = valent_certificate_get_public_key (cert)) == NULL ||
      (peer_pubkey = valent_certificate_get_public_key (peer_cert)) == NULL)
    g_return_val_if_reached (NULL);

  checksum = g_checksum_new (G_CHECKSUM_SHA256);
  cmplen = pubkey->len < peer_pubkey->len ? pubkey->len : peer_pubkey->len;

  if (memcmp (pubkey->data, peer_pubkey->data, cmplen) > 0)
    {
      g_checksum_update (checksum, pubkey->data, pubkey->len);
      g_checksum_update (checksum, peer_pubkey->data, peer_pubkey->len);
    }
  else
    {
      g_checksum_update (checksum, peer_pubkey->data, peer_pubkey->len);
      g_checksum_update (checksum, pubkey->data, pubkey->len);
    }

  self->description = g_strdup (g_checksum_get_string (checksum));

  return self->description;
}

static GIOStream *
valent_lan_channel_download (ValentChannel  *channel,
                             JsonNode       *packet,
                             GCancellable   *cancellable,
                             GError        **error)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (channel);
  JsonObject *info;
  gint64 port;
  goffset size;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  g_autofree char *host = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Get the payload information */
  if ((info = valent_packet_get_payload_full (packet, &size, error)) == NULL)
    return NULL;

  if ((port = json_object_get_int_member (info, "port")) == 0 ||
      (port < 0 || port > G_MAXUINT16))
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_INVALID_FIELD,
                           "expected \"port\" field holding a uint16");
      return NULL;
    }

  /* Open the outgoing connection */
  host = valent_lan_channel_dup_host (self);
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  connection = g_socket_client_connect_to_host (client,
                                                host,
                                                (guint16)port,
                                                cancellable,
                                                error);

  if (connection == NULL)
    return NULL;

  /* We're the TLS client when downloading */
  certificate = valent_lan_channel_ref_certificate (self);
  peer_certificate = valent_lan_channel_ref_peer_certificate (self);
  tls_stream = valent_lan_encrypt_client (connection,
                                          certificate,
                                          peer_certificate,
                                          cancellable,
                                          error);

  if (tls_stream == NULL)
    {
      g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

static GIOStream *
valent_lan_channel_upload (ValentChannel  *channel,
                           JsonNode       *packet,
                           GCancellable   *cancellable,
                           GError        **error)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (channel);
  JsonObject *info;
  g_autoptr (GSocketListener) listener = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  guint16 port = VALENT_LAN_AUX_MIN;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Find an open port */
  listener = g_socket_listener_new ();

  while (!g_socket_listener_add_inet_port (listener, port, NULL, error))
    {
      if (port >= VALENT_LAN_AUX_MAX)
        return NULL;

      port++;
      g_clear_error (error);
    }

  /* Set the payload information */
  info = json_object_new();
  json_object_set_int_member (info, "port", (gint64)port);
  valent_packet_set_payload_info (packet, info);

  /* Notify the device and accept the incoming connection */
  valent_channel_write_packet (channel, packet, cancellable, NULL, NULL);

  connection = g_socket_listener_accept (listener, NULL, cancellable, error);
  g_socket_listener_close (listener);

  if (connection == NULL)
    return NULL;

  /* We're the TLS server when uploading */
  certificate = valent_lan_channel_ref_certificate (self);
  peer_certificate = valent_lan_channel_ref_peer_certificate (self);
  tls_stream = valent_lan_encrypt_server (connection,
                                          certificate,
                                          peer_certificate,
                                          cancellable,
                                          error);

  if (tls_stream == NULL)
    {
      g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

static void
valent_lan_channel_store_data (ValentChannel *channel,
                               ValentData    *data)
{
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autofree char *certificate_pem = NULL;
  g_autofree char *certificate_path = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_DATA (data));

  /* Chain-up first */
  VALENT_CHANNEL_CLASS (valent_lan_channel_parent_class)->store_data (channel,
                                                                      data);

  /* Save the peer certificate */
  g_object_get (channel, "peer-certificate", &certificate, NULL);
  g_object_get (certificate, "certificate-pem", &certificate_pem, NULL);

  certificate_path = g_build_filename (valent_data_get_config_path (data),
                                       "certificate.pem",
                                       NULL);
  g_file_set_contents_full (certificate_path,
                            certificate_pem,
                            strlen (certificate_pem),
                            G_FILE_SET_CONTENTS_DURABLE,
                            0600,
                            &error);

  if (error != NULL)
    g_warning ("%s(): failed to write \"%s\": %s",
               G_STRFUNC,
               certificate_path,
               error->message);
}

/*
 * GObject
 */
static void
valent_lan_channel_finalize (GObject *object)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (object);

  g_clear_object (&self->certificate);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->host, g_free);

  G_OBJECT_CLASS (valent_lan_channel_parent_class)->finalize (object);
}

static void
valent_lan_channel_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (object);

  switch (prop_id)
    {
    case PROP_CERTIFICATE:
      g_value_take_object (value, valent_lan_channel_ref_certificate (self));
      break;

    case PROP_HOST:
      g_value_take_string (value, valent_lan_channel_dup_host (self));
      break;

    case PROP_PEER_CERTIFICATE:
      g_value_take_object (value, valent_lan_channel_ref_peer_certificate (self));
      break;

    case PROP_PORT:
      g_value_set_uint (value, valent_lan_channel_get_port (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lan_channel_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (object);

  switch (prop_id)
    {
    case PROP_CERTIFICATE:
      valent_object_lock (VALENT_OBJECT (self));
      self->certificate = g_value_dup_object (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_HOST:
      valent_object_lock (VALENT_OBJECT (self));
      self->host = g_value_dup_string (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_PORT:
      valent_object_lock (VALENT_OBJECT (self));
      self->port = g_value_get_uint (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lan_channel_class_init (ValentLanChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentChannelClass *channel_class = VALENT_CHANNEL_CLASS (klass);

  object_class->finalize = valent_lan_channel_finalize;
  object_class->get_property = valent_lan_channel_get_property;
  object_class->set_property = valent_lan_channel_set_property;

  channel_class->get_verification_key = valent_lan_channel_get_verification_key;
  channel_class->download = valent_lan_channel_download;
  channel_class->upload = valent_lan_channel_upload;
  channel_class->store_data = valent_lan_channel_store_data;

  /**
   * ValentLanChannel:certificate:
   *
   * The TLS certificate.
   */
  properties [PROP_CERTIFICATE] =
    g_param_spec_object ("certificate", NULL, NULL,
                         G_TYPE_TLS_CERTIFICATE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentLanChannel:host:
   *
   * The remote TCP/IP address for the channel.
   */
  properties [PROP_HOST] =
    g_param_spec_string ("host", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentLanChannel:peer-certificate:
   *
   * The peer TLS certificate.
   */
  properties [PROP_PEER_CERTIFICATE] =
    g_param_spec_object ("peer-certificate", NULL, NULL,
                         G_TYPE_TLS_CERTIFICATE,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentLanChannel:port:
   *
   * The remote TCP/IP port for the channel.
   */
  properties [PROP_PORT] =
    g_param_spec_uint ("port", NULL, NULL,
                       0, G_MAXUINT16,
                       VALENT_LAN_TCP_PORT,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_lan_channel_init (ValentLanChannel *self)
{
}

/**
 * valent_lan_channel_ref_certificate:
 * @self: a #ValentLanChannel
 *
 * Get the TLS certificate.
 *
 * Returns: (transfer full) (nullable): a #GTlsCertificate
 */
GTlsCertificate *
valent_lan_channel_ref_certificate (ValentLanChannel *self)
{
  GTlsCertificate *ret = NULL;

  g_return_val_if_fail (VALENT_IS_LAN_CHANNEL (self), NULL);

  valent_object_lock (VALENT_OBJECT (self));
  if (self->certificate != NULL)
    ret = g_object_ref (self->certificate);
  valent_object_unlock (VALENT_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * valent_lan_channel_ref_peer_certificate:
 * @self: a #ValentLanChannel
 *
 * Get the peer TLS certificate.
 *
 * Returns: (transfer full) (nullable): a #GTlsCertificate
 */
GTlsCertificate *
valent_lan_channel_ref_peer_certificate (ValentLanChannel *self)
{
  g_autoptr (GIOStream) base_stream = NULL;
  GTlsCertificate *ret = NULL;

  g_return_val_if_fail (VALENT_IS_LAN_CHANNEL (self), NULL);

  base_stream = valent_channel_ref_base_stream (VALENT_CHANNEL (self));

  if (base_stream != NULL)
    g_object_get (base_stream, "peer-certificate", &ret, NULL);

  return g_steal_pointer (&ret);
}

/**
 * valent_lan_channel_dup_host:
 * @self: a #ValentLanChannel
 *
 * Get the host address.
 *
 * Returns: (transfer full) (nullable): the remote host address
 */
char *
valent_lan_channel_dup_host (ValentLanChannel *self)
{
  char *ret = NULL;

  g_return_val_if_fail (VALENT_LAN_CHANNEL (self), NULL);

  valent_object_lock (VALENT_OBJECT (self));
  ret = g_strdup (self->host);
  valent_object_unlock (VALENT_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * valent_lan_channel_get_port:
 * @self: a #ValentLanChannel
 *
 * Get the host port.
 *
 * Returns: the remote host port
 */
guint16
valent_lan_channel_get_port (ValentLanChannel *self)
{
  guint16 ret;

  g_return_val_if_fail (VALENT_IS_LAN_CHANNEL (self), 1716);

  valent_object_lock (VALENT_OBJECT (self));
  ret = self->port;
  valent_object_unlock (VALENT_OBJECT (self));

  return ret;
}

