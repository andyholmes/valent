// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-bluez-channel"

#include "config.h"

#include <valent.h>

#include "valent-bluez-channel.h"
#include "valent-bluez-muxer.h"


struct _ValentBluezChannel
{
  ValentChannel     parent_instance;

  ValentBluezMuxer *muxer;
};

G_DEFINE_FINAL_TYPE (ValentBluezChannel, valent_bluez_channel, VALENT_TYPE_CHANNEL)

typedef enum {
  PROP_MUXER = 1,
} ValentBluezChannelProperty;

static GParamSpec *properties[PROP_MUXER + 1] = { NULL, };


/*
 * ValentChannel
 */
static void
valent_bluez_channel_download_task (GTask        *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  ValentBluezChannel *self = VALENT_BLUEZ_CHANNEL (source_object);
  JsonNode *packet = (JsonNode *)task_data;
  JsonObject *info;
  const char *uuid;
  g_autoptr (GIOStream) stream = NULL;
  goffset size;
  GError *error = NULL;

  /* Payload Info
   */
  info = valent_packet_get_payload_full (packet, &size, &error);
  if (info == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if ((uuid = json_object_get_string_member (info, "uuid")) == NULL ||
      *uuid == '\0')
    {
      g_task_return_new_error_literal (task,
                                       VALENT_PACKET_ERROR,
                                       VALENT_PACKET_ERROR_INVALID_FIELD,
                                       "Invalid \"uuid\" field");
      return;
    }

  /* Open a new channel
   */
  valent_object_lock (VALENT_OBJECT (self));
  stream = valent_bluez_muxer_accept_channel (self->muxer,
                                              uuid,
                                              cancellable,
                                              &error);
  valent_object_unlock (VALENT_OBJECT (self));
  if (stream == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task, g_object_ref (stream), g_object_unref);
}

static void
valent_bluez_channel_download (ValentChannel       *channel,
                               JsonNode            *packet,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_bluez_channel_download);
  g_task_set_task_data (task,
                        json_node_ref (packet),
                        (GDestroyNotify)json_node_unref);
  g_task_run_in_thread (task, valent_bluez_channel_download_task);
}

static void
valent_bluez_channel_upload_task (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  ValentBluezChannel *self = VALENT_BLUEZ_CHANNEL (source_object);
  JsonNode *packet = (JsonNode *)task_data;
  JsonObject *info;
  g_autoptr (GIOStream) stream = NULL;
  g_autofree char *uuid = NULL;
  GError *error = NULL;

  /* Payload Info
   */
  uuid = g_uuid_string_random ();
  info = json_object_new();
  json_object_set_string_member (info, "uuid", uuid);
  valent_packet_set_payload_info (packet, info);

  /* Open a new channel
   */
  valent_object_lock (VALENT_OBJECT (self));
  stream = valent_bluez_muxer_open_channel (self->muxer,
                                            uuid,
                                            cancellable,
                                            &error);
  valent_object_unlock (VALENT_OBJECT (self));
  if (stream == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Notify the device we're ready
   */
  valent_channel_write_packet (VALENT_CHANNEL (self),
                               packet,
                               cancellable,
                               NULL,
                               NULL);
  g_task_return_pointer (task, g_object_ref (stream), g_object_unref);
}

static void
valent_bluez_channel_upload (ValentChannel       *channel,
                             JsonNode            *packet,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_bluez_channel_upload);
  g_task_set_task_data (task,
                        json_node_ref (packet),
                        (GDestroyNotify)json_node_unref);
  g_task_run_in_thread (task, valent_bluez_channel_upload_task);
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

  switch ((ValentBluezChannelProperty)prop_id)
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

  switch ((ValentBluezChannelProperty)prop_id)
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
   * The [class@Valent.BluezMuxer] responsible for muxing and demuxing data.
   */
  properties [PROP_MUXER] =
    g_param_spec_object ("muxer", NULL, NULL,
                         VALENT_TYPE_BLUEZ_MUXER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_bluez_channel_init (ValentBluezChannel *self)
{
}

