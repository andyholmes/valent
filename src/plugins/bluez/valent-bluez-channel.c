// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-bluez-channel"

#include "config.h"

#include <valent.h>

#include "valent-bluez-channel.h"
#include "valent-mux-connection.h"


struct _ValentBluezChannel
{
  ValentChannel        parent_instance;

  ValentMuxConnection *muxer;
};

G_DEFINE_FINAL_TYPE (ValentBluezChannel, valent_bluez_channel, VALENT_TYPE_CHANNEL)

enum {
  PROP_0,
  PROP_MUXER,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * ValentChannel
 */
static GIOStream *
valent_bluez_channel_download (ValentChannel  *channel,
                               JsonNode       *packet,
                               GCancellable   *cancellable,
                               GError        **error)
{
  ValentBluezChannel *self = VALENT_BLUEZ_CHANNEL (channel);
  JsonObject *info;
  const char *uuid;
  goffset size;

  g_assert (VALENT_IS_BLUEZ_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Payload Info */
  if ((info = valent_packet_get_payload_full (packet, &size, error)) == NULL)
    return NULL;

  if ((uuid = json_object_get_string_member (info, "uuid")) == NULL ||
      *uuid == '\0')
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_INVALID_FIELD,
                           "Invalid \"uuid\" field");
      return NULL;
    }

  /* Accept the new channel */
  return valent_mux_connection_accept_channel (self->muxer,
                                               uuid,
                                               cancellable,
                                               error);
}

static GIOStream *
valent_bluez_channel_upload (ValentChannel  *channel,
                             JsonNode       *packet,
                             GCancellable   *cancellable,
                             GError        **error)
{
  ValentBluezChannel *self = VALENT_BLUEZ_CHANNEL (channel);
  JsonObject *info;
  g_autoptr (GIOStream) stream = NULL;
  g_autofree char *uuid = NULL;

  g_assert (VALENT_IS_BLUEZ_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Choose a unique UUID? */
  uuid = g_uuid_string_random ();

  /* Payload Info */
  info = json_object_new();
  json_object_set_string_member (info, "uuid", uuid);
  valent_packet_set_payload_info (packet, info);

  /* Open a new channel */
  stream = valent_mux_connection_open_channel (self->muxer,
                                               uuid,
                                               cancellable,
                                               error);

  /* Notify the device we're ready */
  valent_channel_write_packet (channel, packet, cancellable, NULL, NULL);

  return g_steal_pointer (&stream);
}

/*
 * GObject
 */
static void
valent_bluez_channel_finalize (GObject *object)
{
  ValentBluezChannel *self = VALENT_BLUEZ_CHANNEL (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_object (&self->muxer);
  valent_object_unlock (VALENT_OBJECT (self));

  G_OBJECT_CLASS (valent_bluez_channel_parent_class)->finalize (object);
}

static void
valent_bluez_channel_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentBluezChannel *self = VALENT_BLUEZ_CHANNEL (object);

  switch (prop_id)
    {
    case PROP_MUXER:
      valent_object_lock (VALENT_OBJECT (self));
      g_value_set_object (value, self->muxer);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_bluez_channel_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentBluezChannel *self = VALENT_BLUEZ_CHANNEL (object);

  switch (prop_id)
    {
    case PROP_MUXER:
      valent_object_lock (VALENT_OBJECT (self));
      self->muxer = g_value_dup_object (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_bluez_channel_class_init (ValentBluezChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentChannelClass *channel_class = VALENT_CHANNEL_CLASS (klass);

  object_class->finalize = valent_bluez_channel_finalize;
  object_class->get_property = valent_bluez_channel_get_property;
  object_class->set_property = valent_bluez_channel_set_property;

  channel_class->download = valent_bluez_channel_download;
  channel_class->upload = valent_bluez_channel_upload;

  /**
   * ValentBluezChannel:muxer:
   *
   * The `ValentMuxConnection` responsible for muxing and demuxing data.
   */
  properties [PROP_MUXER] =
    g_param_spec_object ("muxer", NULL, NULL,
                         VALENT_TYPE_MUX_CONNECTION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_bluez_channel_init (ValentBluezChannel *self)
{
}

