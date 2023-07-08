// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-pa-mixer"

#include "config.h"

#include <gvc-mixer-control.h>
#include <gvc-mixer-sink.h>
#include <gvc-mixer-source.h>
#include <gvc-mixer-stream.h>
#include <valent.h>

#include "valent-pa-mixer.h"
#include "valent-pa-stream.h"


struct _ValentPaMixer
{
  ValentMixerAdapter  parent_instance;

  GvcMixerControl    *control;

  GHashTable         *streams;
  unsigned int        input;
  unsigned int        output;
  unsigned int        vol_max;
};

G_DEFINE_FINAL_TYPE (ValentPaMixer, valent_pa_mixer, VALENT_TYPE_MIXER_ADAPTER)


/*
 * Gvc Callbacks
 */
static void
on_default_sink_changed (GvcMixerControl *control,
                         unsigned int     stream_id,
                         ValentPaMixer   *self)
{
  g_assert (GVC_IS_MIXER_CONTROL (control));
  g_assert (VALENT_IS_PA_MIXER (self));

  if (self->output == stream_id)
    return;

  self->output = stream_id;
  g_object_notify (G_OBJECT (self), "default-output");
}

static void
on_default_source_changed (GvcMixerControl *control,
                           unsigned int     stream_id,
                           ValentPaMixer   *self)
{
  g_assert (GVC_IS_MIXER_CONTROL (control));
  g_assert (VALENT_IS_PA_MIXER (self));

  if (self->input == stream_id)
    return;

  self->input = stream_id;
  g_object_notify (G_OBJECT (self), "default-input");
}

static void
on_stream_added (GvcMixerControl *control,
                 unsigned int     stream_id,
                 ValentPaMixer   *self)
{
  GvcMixerStream *base_stream = NULL;
  ValentMixerStream *stream = NULL;
  ValentMixerDirection direction;

  base_stream = gvc_mixer_control_lookup_stream_id (control, stream_id);

  /* Ignore source outputs and sink inputs */
  if (GVC_IS_MIXER_SINK (base_stream))
    direction = VALENT_MIXER_OUTPUT;
  else if (GVC_IS_MIXER_SOURCE (base_stream))
    direction = VALENT_MIXER_INPUT;
  else
    return;

  stream = g_object_new (VALENT_TYPE_PA_STREAM,
                         "base-stream", base_stream,
                         "direction",   direction,
                         "vol-max",     self->vol_max,
                         NULL);

  if (!g_hash_table_replace (self->streams, GUINT_TO_POINTER (stream_id), stream))
    {
      g_warning ("%s: Duplicate Stream: %s",
                 G_OBJECT_TYPE_NAME (self),
                 valent_mixer_stream_get_name (stream));
    }

  valent_mixer_adapter_stream_added (VALENT_MIXER_ADAPTER (self), stream);
}

static void
on_stream_removed (GvcMixerControl *control,
                   unsigned int     stream_id,
                   ValentPaMixer   *self)
{
  ValentMixerStream *stream = NULL;

  g_assert (GVC_IS_MIXER_CONTROL (control));
  g_assert (VALENT_IS_PA_MIXER (self));

  stream = g_hash_table_lookup (self->streams, GUINT_TO_POINTER (stream_id));

  if (stream == NULL)
    return;

  /* FIXME: If the stream being removed is the default, the change notification
   *        will come after the removal notification. As a side effect, if the
   *        kdeconnect-android activity is open it will automatically select a
   *        remaining stream and override any automatic selection the system
   *        wants to perform.
   */

  valent_mixer_adapter_stream_removed (VALENT_MIXER_ADAPTER (self), stream);
  g_hash_table_remove (self->streams, GUINT_TO_POINTER (stream_id));
}

static void
on_stream_changed (GvcMixerControl *control,
                   unsigned int     stream_id,
                   ValentPaMixer   *self)
{
  ValentMixerStream *stream = NULL;

  g_assert (GVC_IS_MIXER_CONTROL (control));
  g_assert (VALENT_IS_PA_MIXER (self));

  stream = g_hash_table_lookup (self->streams, GUINT_TO_POINTER (stream_id));

  if (stream == NULL)
    return;

  g_object_notify (G_OBJECT (stream), "level");
}

static void
valent_pa_mixer_load (ValentPaMixer *self)
{
  g_autoptr (GSList) sources = NULL;
  g_autoptr (GSList) sinks = NULL;
  GvcMixerStream *stream = NULL;

  g_assert (VALENT_IS_PA_MIXER (self));

  /* Get current defaults */
  self->vol_max = gvc_mixer_control_get_vol_max_norm (self->control);

  /* Query the default streams before emitting signals */
  if ((stream = gvc_mixer_control_get_default_sink (self->control)) != NULL)
    self->output = gvc_mixer_stream_get_id (stream);

  if ((stream = gvc_mixer_control_get_default_source (self->control)) != NULL)
    self->input = gvc_mixer_stream_get_id (stream);

  /* Get current streams */
  sinks = gvc_mixer_control_get_sinks (self->control);

  for (const GSList *iter = sinks; iter; iter = iter->next)
    on_stream_added (self->control, gvc_mixer_stream_get_id (iter->data), self);

  sources = gvc_mixer_control_get_sources (self->control);

  for (const GSList *iter = sources; iter; iter = iter->next)
    on_stream_added (self->control, gvc_mixer_stream_get_id (iter->data), self);

  /* Watch stream changes */
  g_object_connect (self->control,
                    "signal::default-sink-changed",   on_default_sink_changed,   self,
                    "signal::default-source-changed", on_default_source_changed, self,
                    "signal::stream-added",           on_stream_added,           self,
                    "signal::stream-removed",         on_stream_removed,         self,
                    "signal::stream-changed",         on_stream_changed,         self,
                    NULL);
}

static void
valent_pa_mixer_unload (ValentPaMixer *self)
{
  ValentMixerAdapter *adapter = VALENT_MIXER_ADAPTER (self);
  GHashTableIter iter;
  ValentMixerStream *stream;

  g_assert (VALENT_IS_PA_MIXER (self));

  /* Clear the current defaults */
  self->input = 0;
  g_object_notify (G_OBJECT (self), "default-input");
  self->output = 0;
  g_object_notify (G_OBJECT (self), "default-output");

  g_hash_table_iter_init (&iter, self->streams);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&stream))
    {
      valent_mixer_adapter_stream_removed (adapter, stream);
      g_hash_table_iter_remove (&iter);
    }

  g_signal_handlers_disconnect_by_func (self->control, on_default_sink_changed, self);
  g_signal_handlers_disconnect_by_func (self->control, on_default_source_changed, self);
  g_signal_handlers_disconnect_by_func (self->control, on_stream_added, self);
  g_signal_handlers_disconnect_by_func (self->control, on_stream_removed, self);
  g_signal_handlers_disconnect_by_func (self->control, on_stream_changed, self);
}

static void
on_state_changed (GvcMixerControl      *control,
                  GvcMixerControlState  state,
                  ValentPaMixer        *self)
{
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_PA_MIXER (self));

  switch (state)
    {
    case GVC_STATE_CLOSED:
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_INACTIVE,
                                             error);
      valent_pa_mixer_unload (self);
      break;

    case GVC_STATE_READY:
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ACTIVE,
                                             error);
      valent_pa_mixer_load (self);
      break;

    case GVC_STATE_CONNECTING:
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_INACTIVE,
                                             error);
      break;

    case GVC_STATE_FAILED:
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "failed to connect to PulseAudio server");
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      valent_pa_mixer_unload (self);
      break;
    }
}

/*
 * ValentMixerAdapter
 */
static ValentMixerStream *
valent_pa_mixer_get_default_input (ValentMixerAdapter *adapter)
{
  ValentPaMixer *self = VALENT_PA_MIXER (adapter);

  return g_hash_table_lookup (self->streams, GUINT_TO_POINTER (self->input));
}

static void
valent_pa_mixer_set_default_input (ValentMixerAdapter *adapter,
                                   ValentMixerStream  *stream)
{
  ValentPaMixer *self = VALENT_PA_MIXER (adapter);
  GvcMixerStream *base_stream = NULL;

  g_object_get (stream, "base-stream", &base_stream, NULL);
  gvc_mixer_control_set_default_source (self->control, base_stream);
  g_clear_object (&base_stream);
}

static ValentMixerStream *
valent_pa_mixer_get_default_output (ValentMixerAdapter *adapter)
{
  ValentPaMixer *self = VALENT_PA_MIXER (adapter);

  return g_hash_table_lookup (self->streams, GUINT_TO_POINTER (self->output));
}

static void
valent_pa_mixer_set_default_output (ValentMixerAdapter *adapter,
                                    ValentMixerStream  *stream)
{
  ValentPaMixer *self = VALENT_PA_MIXER (adapter);
  GvcMixerStream *base_stream = NULL;

  g_object_get (stream, "base-stream", &base_stream, NULL);
  gvc_mixer_control_set_default_sink (self->control, base_stream);
  g_clear_object (&base_stream);
}

/*
 * ValentObject
 */
static void
valent_pa_mixer_destroy (ValentObject *object)
{
  ValentPaMixer *self = VALENT_PA_MIXER (object);

  g_signal_handlers_disconnect_by_data (self->control, self);
  gvc_mixer_control_close (self->control);
  g_hash_table_remove_all (self->streams);

  VALENT_OBJECT_CLASS (valent_pa_mixer_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_pa_mixer_constructed (GObject *object)
{
  ValentPaMixer *self = VALENT_PA_MIXER (object);

  self->vol_max = gvc_mixer_control_get_vol_max_norm (self->control);

  g_signal_connect_object (self->control,
                           "state-changed",
                           G_CALLBACK (on_state_changed),
                           self, 0);
  gvc_mixer_control_open (self->control);

  G_OBJECT_CLASS (valent_pa_mixer_parent_class)->constructed (object);
}

static void
valent_pa_mixer_finalize (GObject *object)
{
  ValentPaMixer *self = VALENT_PA_MIXER (object);

  g_clear_pointer (&self->streams, g_hash_table_unref);
  g_clear_object (&self->control);

  G_OBJECT_CLASS (valent_pa_mixer_parent_class)->finalize (object);
}

static void
valent_pa_mixer_class_init (ValentPaMixerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentMixerAdapterClass *adapter_class = VALENT_MIXER_ADAPTER_CLASS (klass);

  object_class->constructed = valent_pa_mixer_constructed;
  object_class->finalize = valent_pa_mixer_finalize;

  vobject_class->destroy = valent_pa_mixer_destroy;

  adapter_class->get_default_input = valent_pa_mixer_get_default_input;
  adapter_class->set_default_input = valent_pa_mixer_set_default_input;
  adapter_class->get_default_output = valent_pa_mixer_get_default_output;
  adapter_class->set_default_output = valent_pa_mixer_set_default_output;
}

static void
valent_pa_mixer_init (ValentPaMixer *self)
{
  self->control = g_object_new (GVC_TYPE_MIXER_CONTROL,
                                "name", "Valent",
                                NULL);
  self->streams = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
}

