// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-systemvolume-plugin"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-systemvolume-plugin.h"


struct _ValentSystemvolumePlugin
{
  ValentDevicePlugin  parent_instance;

  ValentMixer        *mixer;
  unsigned int        mixer_watch : 1;
  GListModel         *sinks;
  GPtrArray          *states;
};

static void valent_systemvolume_plugin_handle_request     (ValentSystemvolumePlugin *self,
                                                           JsonNode                 *packet);
static void valent_systemvolume_plugin_handle_sink_change (ValentSystemvolumePlugin *self,
                                                           JsonNode                 *packet);
static void valent_systemvolume_plugin_send_sinklist      (ValentSystemvolumePlugin *self);

G_DEFINE_FINAL_TYPE (ValentSystemvolumePlugin, valent_systemvolume_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/*
 * Local Mixer
 */
typedef struct
{
  ValentMixerStream *stream;
  unsigned long      notify_id;
  char              *name;
  char              *description;
  unsigned int       volume;
  unsigned int       muted : 1;
  unsigned int       enabled : 1;
} StreamState;

static StreamState *
stream_state_find (ValentSystemvolumePlugin *self,
                   const char               *name)
{
  for (unsigned int i = 0; i < self->states->len; i++)
    {
      StreamState *state = g_ptr_array_index (self->states, i);

      if (g_strcmp0 (state->name, name) == 0)
        return state;
    }

  return NULL;
}

static void
on_stream_changed (ValentMixerStream        *stream,
                   GParamSpec               *pspec,
                   ValentSystemvolumePlugin *self)
{
  StreamState *state;
  const char *name;
  const char *description;
  gboolean enabled;
  gboolean muted;
  unsigned int volume;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  /* If this is an unknown stream, send a new sink list */
  name = valent_mixer_stream_get_name (stream);

  if ((state = stream_state_find (self, name)) == NULL)
    {
      valent_systemvolume_plugin_send_sinklist (self);
      return;
    }

  /* If the description changed it's probably because the port changed, so
   * update the state and send the whole list */
  description = valent_mixer_stream_get_description (stream);

  if (valent_set_string (&state->description, description))
    {
      valent_systemvolume_plugin_send_sinklist (self);
      return;
    }

  /* If none of the other properties changed, there's nothing to update */
  enabled = valent_mixer_get_default_output (self->mixer) == stream;
  muted = valent_mixer_stream_get_muted (stream);
  volume = valent_mixer_stream_get_level (stream);

  if (state->enabled == enabled &&
      state->muted == muted &&
      state->volume == volume)
    return;

  /* Sink update */
  valent_packet_init (&builder, "kdeconnect.systemvolume");
  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, state->name);

  if (state->muted != muted)
    {
      state->muted = muted;
      json_builder_set_member_name (builder, "muted");
      json_builder_add_boolean_value (builder, state->muted);
    }

  if (state->volume != volume)
    {
      state->volume = volume;
      json_builder_set_member_name (builder, "volume");
      json_builder_add_int_value (builder, state->volume);
    }

  if (state->enabled != enabled)
    {
      state->enabled = enabled;
      json_builder_set_member_name (builder, "enabled");
      json_builder_add_boolean_value (builder, state->enabled);
    }

  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static StreamState *
stream_state_new (ValentSystemvolumePlugin *self,
                  ValentMixerStream        *stream)
{
  StreamState *state;

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));
  g_assert (VALENT_IS_MIXER_STREAM (stream));

  state = g_new0 (StreamState, 1);
  state->stream = g_object_ref (stream);
  g_signal_connect (state->stream,
                    "notify",
                    G_CALLBACK (on_stream_changed),
                    self);

  state->name = g_strdup (valent_mixer_stream_get_name (stream));
  state->description = g_strdup (valent_mixer_stream_get_description (stream));
  state->volume = valent_mixer_stream_get_level (stream);
  state->muted = valent_mixer_stream_get_muted (stream);
  state->enabled = valent_mixer_get_default_output (self->mixer) == stream;

  return state;
}

static void
stream_state_free (gpointer data)
{
  StreamState *state = data;

  g_clear_signal_handler (&state->notify_id, state->stream);
  g_clear_object (&state->stream);
  g_clear_pointer (&state->name, g_free);
  g_clear_pointer (&state->description, g_free);
  g_clear_pointer (&state, g_free);
}

static void
on_default_output_changed (ValentMixer              *mixer,
                           GParamSpec               *pspec,
                           ValentSystemvolumePlugin *self)
{
  ValentMixerStream *default_output = NULL;

  g_assert (VALENT_IS_MIXER (mixer));
  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  default_output = valent_mixer_get_default_output (mixer);

  for (unsigned int i = 0; i < self->states->len; i++)
    {
      StreamState *state = g_ptr_array_index (self->states, i);

      state->enabled = state->stream == default_output;
    }

  /* It's unclear whether the `enabled` field with a value of `false` is
   * relevant in the protocol, we resend the whole list */
  valent_systemvolume_plugin_send_sinklist (self);
}

static void
on_items_changed (GListModel               *list,
                  unsigned int              position,
                  unsigned int              removed,
                  unsigned int              added,
                  ValentSystemvolumePlugin *self)
{
  g_assert (G_IS_LIST_MODEL (list));
  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  g_ptr_array_remove_range (self->states, position, removed);

  for (unsigned int i = 0; i < added; i++)
    {
      g_autoptr (ValentMixerStream) stream = NULL;
      StreamState *state = NULL;

      stream = g_list_model_get_item (list, position + i);
      state = stream_state_new (self, stream);

      g_ptr_array_insert (self->states, position + i, state);
    }

  valent_systemvolume_plugin_send_sinklist (self);
}

static void
valent_systemvolume_plugin_watch_mixer (ValentSystemvolumePlugin *self,
                                        gboolean                  watch)
{
  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  if (self->mixer_watch == watch)
    return;

  if (self->mixer == NULL)
    self->mixer = valent_mixer_get_default ();

  if (watch)
    {
      if (self->sinks == NULL)
        {
          unsigned int n_sinks = 0;

          self->sinks = valent_mixer_get_outputs (self->mixer);
          n_sinks = g_list_model_get_n_items (self->sinks);

          for (unsigned int i = 0; i < n_sinks; i++)
            {
              g_autoptr (ValentMixerStream) stream = NULL;
              StreamState *state = NULL;

              stream = g_list_model_get_item (self->sinks, i);
              state = stream_state_new (self, stream);
              g_ptr_array_add (self->states, state);
            }
        }

      g_signal_connect (self->mixer,
                        "notify::default-output",
                        G_CALLBACK (on_default_output_changed),
                        self);
      g_signal_connect (self->sinks,
                        "items-changed",
                        G_CALLBACK (on_items_changed),
                        self);
      self->mixer_watch = TRUE;
    }
  else
    {
      g_signal_handlers_disconnect_by_data (self->mixer, self);
      g_signal_handlers_disconnect_by_data (self->sinks, self);
      g_clear_object (&self->sinks);
      self->mixer_watch = FALSE;
    }
}

/*
 * Packet Providers
 */
static void
valent_systemvolume_plugin_send_sinklist (ValentSystemvolumePlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  unsigned int max_volume = 100;

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  /* Sink List */
  valent_packet_init (&builder, "kdeconnect.systemvolume");
  json_builder_set_member_name (builder, "sinkList");
  json_builder_begin_array (builder);

  for (unsigned int i = 0; i < self->states->len; i++)
    {
      StreamState *state;

      state = g_ptr_array_index (self->states, i);

      json_builder_begin_object (builder);
      json_builder_set_member_name (builder, "name");
      json_builder_add_string_value (builder, state->name);
      json_builder_set_member_name (builder, "description");
      json_builder_add_string_value (builder, state->description);
      json_builder_set_member_name (builder, "muted");
      json_builder_add_boolean_value (builder, state->muted);
      json_builder_set_member_name (builder, "volume");
      json_builder_add_int_value (builder, state->volume);
      json_builder_set_member_name (builder, "maxVolume");
      json_builder_add_int_value (builder, max_volume);
      json_builder_set_member_name (builder, "enabled");
      json_builder_add_boolean_value (builder, state->enabled);
      json_builder_end_object (builder);
    }

  json_builder_end_array (builder);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_systemvolume_plugin_handle_sink_change (ValentSystemvolumePlugin *self,
                                               JsonNode                 *packet)
{
  StreamState *state;
  const char *name;
  gint64 volume;
  gboolean muted;
  gboolean enabled;

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_string (packet, "name", &name))
    {
      g_warning ("%s(): expected \"name\" field holding a string", G_STRFUNC);
      return;
    }

  if ((state = stream_state_find (self, name)) == NULL)
    {
      valent_systemvolume_plugin_send_sinklist (self);
      return;
    }

  if (valent_packet_get_int (packet, "volume", &volume) && volume >= 0)
    valent_mixer_stream_set_level (state->stream, volume);

  if (valent_packet_get_boolean (packet, "muted", &muted))
    valent_mixer_stream_set_muted (state->stream, muted);

  if (valent_packet_get_boolean (packet, "enabled", &enabled) && enabled)
    valent_mixer_set_default_output (self->mixer, state->stream);
}

static void
valent_systemvolume_plugin_handle_request (ValentSystemvolumePlugin *self,
                                           JsonNode                 *packet)
{
  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  /* A request for a list of audio outputs */
  if (valent_packet_check_field (packet, "requestSinks"))
    valent_systemvolume_plugin_send_sinklist (self);

  /* A request to change an audio output */
  else if (valent_packet_check_field (packet, "name"))
    valent_systemvolume_plugin_handle_sink_change (self, packet);

  else
    g_warn_if_reached ();
}

/*
 * ValentDevicePlugin
 */
static void
valent_systemvolume_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (plugin);

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  self->states = g_ptr_array_new_with_free_func (stream_state_free);
}

static void
valent_systemvolume_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (plugin);

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  valent_systemvolume_plugin_watch_mixer (self, FALSE);
  g_clear_pointer (&self->states, g_ptr_array_unref);
}

static void
valent_systemvolume_plugin_update_state (ValentDevicePlugin *plugin,
                                         ValentDeviceState   state)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  /* Watch stream changes */
  if (available)
    {
      valent_systemvolume_plugin_watch_mixer (self, TRUE);
      valent_systemvolume_plugin_send_sinklist (self);
    }
  else
    {
      valent_systemvolume_plugin_watch_mixer (self, FALSE);
      g_ptr_array_remove_range (self->states, 0, self->states->len);
    }
}

static void
valent_systemvolume_plugin_handle_packet (ValentDevicePlugin *plugin,
                                          const char         *type,
                                          JsonNode           *packet)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (plugin);

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (strcmp (type, "kdeconnect.systemvolume.request") == 0)
    valent_systemvolume_plugin_handle_request (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_systemvolume_plugin_class_init (ValentSystemvolumePluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_systemvolume_plugin_enable;
  plugin_class->disable = valent_systemvolume_plugin_disable;
  plugin_class->handle_packet = valent_systemvolume_plugin_handle_packet;
  plugin_class->update_state = valent_systemvolume_plugin_update_state;
}

static void
valent_systemvolume_plugin_init (ValentSystemvolumePlugin *self)
{
}

