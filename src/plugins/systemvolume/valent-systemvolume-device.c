// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-systemvolume-device"

#include "config.h"

#include <inttypes.h>
#include <math.h>

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <valent.h>

#include "valent-systemvolume-device.h"

static inline void
valent_device_send_packet_cb (ValentDevice *device,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!valent_device_send_packet_finish (device, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED))
        g_critical ("%s(): %s", G_STRFUNC, error->message);
      else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("%s(): %s", G_STRFUNC, error->message);
    }
}

#define VALENT_TYPE_SYSTEMVOLUME_STREAM (valent_systemvolume_stream_get_type ())
G_DECLARE_FINAL_TYPE (ValentSystemvolumeStream, valent_systemvolume_stream, VALENT, SYSTEMVOLUME_STREAM, ValentMixerStream)

struct _ValentSystemvolumeStream
{
  ValentMixerStream  parent_instance;

  ValentDevice      *device;
  GCancellable      *cancellable;

  int64_t            max_volume;
  int64_t            volume;
  unsigned int       muted : 1;
};

G_DEFINE_FINAL_TYPE (ValentSystemvolumeStream, valent_systemvolume_stream, VALENT_TYPE_MIXER_STREAM)

/*
 * ValentMixerStream
 */
static unsigned int
valent_systemvolume_stream_get_level (ValentMixerStream *stream)
{
  ValentSystemvolumeStream *self = VALENT_SYSTEMVOLUME_STREAM (stream);
  double percent;

  g_assert (VALENT_IS_SYSTEMVOLUME_STREAM (self));

  percent = (double)self->volume / (double)self->max_volume;

  return (unsigned int)round (percent * 100.0);
}

static void
valent_systemvolume_stream_set_level (ValentMixerStream *stream,
                                      unsigned int       level)
{
  ValentSystemvolumeStream *self = VALENT_SYSTEMVOLUME_STREAM (stream);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  double percent;
  int64_t volume;

  g_assert (VALENT_IS_SYSTEMVOLUME_STREAM (self));

  percent = (double)level / 100.0;
  volume = (int64_t)round (percent * self->max_volume);

  valent_packet_init (&builder,  "kdeconnect.systemvolume.request");
  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, valent_mixer_stream_get_name (stream));
  json_builder_set_member_name (builder, "volume");
  json_builder_add_int_value (builder, volume);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device,
                             packet,
                             self->cancellable,
                             (GAsyncReadyCallback) valent_device_send_packet_cb,
                             NULL);
}

static gboolean
valent_systemvolume_stream_get_muted (ValentMixerStream *stream)
{
  ValentSystemvolumeStream *self = VALENT_SYSTEMVOLUME_STREAM (stream);

  g_assert (VALENT_IS_SYSTEMVOLUME_STREAM (self));

  return self->muted;
}

static void
valent_systemvolume_stream_set_muted (ValentMixerStream *stream,
                                      gboolean           state)
{
  ValentSystemvolumeStream *self = VALENT_SYSTEMVOLUME_STREAM (stream);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_SYSTEMVOLUME_STREAM (self));

  valent_packet_init (&builder, "kdeconnect.systemvolume.request");
  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, valent_mixer_stream_get_name (stream));
  json_builder_set_member_name (builder, "muted");
  json_builder_add_boolean_value (builder, state);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device,
                             packet,
                             self->cancellable,
                             (GAsyncReadyCallback) valent_device_send_packet_cb,
                             NULL);
}

static void
valent_systemvolume_stream_update (ValentMixerStream *stream,
                                   JsonObject        *state)
{
  ValentSystemvolumeStream *self = VALENT_SYSTEMVOLUME_STREAM (stream);
  JsonNode *node;

  node = json_object_get_member (state, "maxVolume");
  if (node != NULL)
    {
      if (json_node_get_value_type (node) == G_TYPE_INT64)
        {
          self->max_volume = json_node_get_int (node);
        }
      else
        {
          g_warning ("%s(): expected \"maxVolume\" field holding an integer",
                     G_STRFUNC);
        }
    }

  node = json_object_get_member (state, "muted");
  if (node != NULL)
    {
      if (json_node_get_value_type (node) == G_TYPE_BOOLEAN)
        {
          gboolean muted = json_node_get_boolean (node);
          if (self->muted != muted)
            {
              self->muted = muted;
              g_object_notify (G_OBJECT (self), "muted");
            }
        }
      else
        {
          g_warning ("%s(): expected \"muted\" field holding a boolean",
                     G_STRFUNC);
        }
    }

  node = json_object_get_member (state, "volume");
  if (node != NULL)
    {
      if (json_node_get_value_type (node) == G_TYPE_INT64)
        {
          int64_t volume = json_node_get_int (node);
          if (self->volume != volume)
            {
              self->volume = volume;
              g_object_notify (G_OBJECT (self), "level");
            }
        }
      else
        {
          g_warning ("%s(): expected \"volume\" field holding an integer",
                     G_STRFUNC);
        }
    }
}

static void
valent_systemvolume_stream_class_init (ValentSystemvolumeStreamClass *klass)
{
  ValentMixerStreamClass *stream_class = VALENT_MIXER_STREAM_CLASS (klass);

  stream_class->get_level = valent_systemvolume_stream_get_level;
  stream_class->set_level = valent_systemvolume_stream_set_level;
  stream_class->get_muted = valent_systemvolume_stream_get_muted;
  stream_class->set_muted = valent_systemvolume_stream_set_muted;
}

static void
valent_systemvolume_stream_init (ValentSystemvolumeStream *self)
{
}


/* <private>
 * ValentSystemvolumeDevice:
 *
 * A `ValentMixerAdapter` for KDE Connect devices.
 */
struct _ValentSystemvolumeDevice
{
  ValentMixerAdapter  parent_instance;

  ValentDevice       *device;
  GCancellable       *cancellable;
  ValentMixerStream  *default_output;
  GHashTable         *outputs;
};

G_DEFINE_FINAL_TYPE (ValentSystemvolumeDevice, valent_systemvolume_device, VALENT_TYPE_MIXER_ADAPTER)

#if 0
static void
valent_systemvolume_device_request_sinks (ValentSystemvolumeDevice *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.systemvolume.request");
  json_builder_set_member_name (builder, "requestSinks");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device,
                             packet,
                             self->cancellable,
                             (GAsyncReadyCallback) valent_device_send_packet_cb,
                             NULL);
}
#endif

/*
 * ValentMixerAdapter
 */
static void
on_device_state_changed (ValentDevice             *device,
                         GParamSpec               *pspec,
                         ValentSystemvolumeDevice *self)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  gboolean available;

  g_assert (VALENT_IS_SYSTEMVOLUME_DEVICE (self));

  state = valent_device_get_state (device);
  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  if (available && self->cancellable == NULL)
    {
      self->cancellable = g_cancellable_new ();
    }
  else if (!available && self->cancellable != NULL)
    {
      GHashTableIter iter;
      ValentMixerStream *output;

      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);

      g_hash_table_iter_init (&iter, self->outputs);
      while (g_hash_table_iter_next (&iter, NULL, (void **)&output))
        {
          valent_mixer_adapter_stream_removed (VALENT_MIXER_ADAPTER (self),
                                               output);
          g_hash_table_iter_remove (&iter);
        }
    }
}

static ValentMixerStream  *
valent_systemvolume_device_get_default_output (ValentMixerAdapter *adapter)
{
  ValentSystemvolumeDevice *self = VALENT_SYSTEMVOLUME_DEVICE (adapter);

  g_assert (VALENT_IS_SYSTEMVOLUME_DEVICE (self));

  return self->default_output;
}

static void
valent_systemvolume_device_set_default_output (ValentMixerAdapter *adapter,
                                               ValentMixerStream  *stream)
{
  ValentSystemvolumeDevice *self = VALENT_SYSTEMVOLUME_DEVICE (adapter);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_SYSTEMVOLUME_DEVICE (self));

  if (self->default_output == stream)
    return;

  valent_packet_init (&builder, "kdeconnect.systemvolume.request");
  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, valent_mixer_stream_get_name (stream));
  json_builder_set_member_name (builder, "enabled");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device,
                             packet,
                             self->cancellable,
                             (GAsyncReadyCallback) valent_device_send_packet_cb,
                             NULL);
}

/*
 * GObject
 */
static void
valent_systemvolume_device_constructed (GObject *object)
{
  ValentSystemvolumeDevice *self = VALENT_SYSTEMVOLUME_DEVICE (object);

  G_OBJECT_CLASS (valent_systemvolume_device_parent_class)->constructed (object);

  self->device = valent_resource_get_source (VALENT_RESOURCE (self));
  g_signal_connect_object (self->device,
                           "notify::state",
                           G_CALLBACK (on_device_state_changed),
                           self,
                           G_CONNECT_DEFAULT);
  on_device_state_changed (self->device, NULL, self);
}

static void
valent_systemvolume_device_finalize (GObject *object)
{
  ValentSystemvolumeDevice *self = VALENT_SYSTEMVOLUME_DEVICE (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->default_output);
  g_clear_pointer (&self->outputs, g_hash_table_unref);

  G_OBJECT_CLASS (valent_systemvolume_device_parent_class)->finalize (object);
}

static void
valent_systemvolume_device_class_init (ValentSystemvolumeDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMixerAdapterClass *adapter_class = VALENT_MIXER_ADAPTER_CLASS (klass);

  object_class->constructed = valent_systemvolume_device_constructed;
  object_class->finalize = valent_systemvolume_device_finalize;

  adapter_class->get_default_output = valent_systemvolume_device_get_default_output;
  adapter_class->set_default_output = valent_systemvolume_device_set_default_output;
}

static void
valent_systemvolume_device_init (ValentSystemvolumeDevice *self)
{
  self->outputs = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}

/**
 * valent_systemvolume_device_new:
 * @device: a `ValentDevice`
 *
 * Create a new `ValentSystemvolumeDevice`.
 *
 * Returns: (transfer full): a new message store
 */
ValentMixerAdapter *
valent_systemvolume_device_new (ValentDevice *device)
{
  g_autoptr (ValentContext) context = NULL;
  g_autofree char *iri = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  context = valent_context_new (valent_device_get_context (device),
                                "plugin",
                                "systemvolume");
  iri = tracker_sparql_escape_uri_printf ("urn:valent:mixer:%s",
                                          valent_device_get_id (device));
  return g_object_new (VALENT_TYPE_SYSTEMVOLUME_DEVICE,
                       "iri",     iri,
                       "context", context,
                       "source",  device,
                       "title",   valent_device_get_name (device),
                       NULL);
}

static const char *
valent_systemvolume_device_handle_stream (ValentSystemvolumeDevice *self,
                                          JsonObject               *state)
{
  ValentMixerStream *stream = NULL;
  gboolean new_stream = FALSE;
  JsonNode *node;
  const char *name = NULL;

  node = json_object_get_member (state, "name");
  if (node == NULL || json_node_get_value_type (node) != G_TYPE_STRING)
    {
      g_warning ("%s(): expected \"name\" field holding a string", G_STRFUNC);
      return NULL;
    }
  name = json_node_get_string (node);

  stream = g_hash_table_lookup (self->outputs, name);
  if (stream == NULL)
    {
      const char *description = NULL;

      node = json_object_get_member (state, "description");
      if (node != NULL)
        {
          if (json_node_get_value_type (node) == G_TYPE_STRING)
            {
              description = json_node_get_string (node);
            }
          else
            {
              g_warning ("%s(): expected \"description\" field holding a string",
                         G_STRFUNC);
            }
        }

      stream = g_object_new (VALENT_TYPE_SYSTEMVOLUME_STREAM,
                             "name",        name,
                             "description", description,
                             "direction",   VALENT_MIXER_OUTPUT,
                             NULL);
      ((ValentSystemvolumeStream *)stream)->device = self->device;
      ((ValentSystemvolumeStream *)stream)->cancellable = self->cancellable;
      g_hash_table_replace (self->outputs, g_strdup (name), stream /* owned */);
      new_stream = TRUE;
    }

  valent_systemvolume_stream_update (stream, state);

  node = json_object_get_member (state, "enabled");
  if (node != NULL)
    {
      if (json_node_get_value_type (node) == G_TYPE_BOOLEAN)
        {
          if (json_node_get_boolean (node) &&
              g_set_object (&self->default_output, stream))
            {
              g_object_notify (G_OBJECT (self), "default-output");
            }
        }
      else
        {
          g_warning ("%s(): expected \"enabled\" field holding a boolean",
                     G_STRFUNC);
        }
    }

  if (new_stream)
    valent_mixer_adapter_stream_added (VALENT_MIXER_ADAPTER (self), stream);

  return name;
}

/**
 * valent_systemvolume_device_handle_packet:
 * @self: a `ValentSystemvolumeDevice`
 * @packet: a `kdeconnect.systemvolume.*` packet
 *
 * Handle a packet of mixer.
 */
void
valent_systemvolume_device_handle_packet (ValentSystemvolumeDevice *self,
                                          JsonNode                 *packet)
{
  JsonArray *sinks;

  VALENT_ENTRY;

  g_assert (VALENT_IS_SYSTEMVOLUME_DEVICE (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (valent_packet_get_array (packet, "sinkList", &sinks))
    {
      g_autoptr (GStrvBuilder) names = NULL;
      g_auto (GStrv) namev = NULL;
      unsigned int n_sinks = 0;
      GHashTableIter iter;
      const char *name = NULL;
      ValentMixerStream *output = NULL;

      names = g_strv_builder_new ();
      n_sinks = json_array_get_length (sinks);
      for (unsigned int i = 0; i < n_sinks; i++)
        {
          JsonObject *sink = json_array_get_object_element (sinks, i);

          name = valent_systemvolume_device_handle_stream (self, sink);
          if (name != NULL)
            g_strv_builder_add (names, name);
        }
      namev = g_strv_builder_end (names);

      g_hash_table_iter_init (&iter, self->outputs);
      while (g_hash_table_iter_next (&iter, (void **)&name, (void **)&output))
        {
          if (!g_strv_contains ((const char * const *)namev, name))
            {
              valent_mixer_adapter_stream_removed (VALENT_MIXER_ADAPTER (self),
                                                   output);
              g_hash_table_iter_remove (&iter);
            }
        }
    }
  else
    {
      JsonObject *body;

      body = valent_packet_get_body (packet);
      valent_systemvolume_device_handle_stream (self, body);
    }

  VALENT_EXIT;
}

