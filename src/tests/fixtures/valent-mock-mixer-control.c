// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <math.h>
#include <libvalent-core.h>
#include <libvalent-mixer.h>

#include "valent-mock-mixer-control.h"


struct _ValentMockMixerControl
{
  ValentMixerControl  parent_instance;

  ValentMixerStream  *default_input;
  ValentMixerStream  *default_output;
};

G_DEFINE_TYPE (ValentMockMixerControl, valent_mock_mixer_control, VALENT_TYPE_MIXER_CONTROL)


static ValentMixerControl *test_instance = NULL;

static void
on_stream_changed (ValentMixerStream  *stream,
                   GParamSpec         *pspec,
                   ValentMixerControl *control)
{
  g_assert (VALENT_IS_MOCK_MIXER_CONTROL (control));

  valent_mixer_control_emit_stream_changed (control, stream);
}

/*
 * ValentMixerControl
 */
static ValentMixerStream *
valent_mock_mixer_control_get_default_input (ValentMixerControl *control)
{
  ValentMockMixerControl *self = VALENT_MOCK_MIXER_CONTROL (control);

  return self->default_input;
}

static ValentMixerStream *
valent_mock_mixer_control_get_default_output (ValentMixerControl *control)
{
  ValentMockMixerControl *self = VALENT_MOCK_MIXER_CONTROL (control);

  return self->default_output;
}

static void
valent_mock_mixer_control_stream_added (ValentMixerControl *control,
                                        ValentMixerStream  *stream)
{
  ValentMockMixerControl *self = VALENT_MOCK_MIXER_CONTROL (control);

  if ((valent_mixer_stream_get_flags (stream) & VALENT_MIXER_STREAM_SOURCE) != 0)
    g_set_object (&self->default_input, stream);

  if ((valent_mixer_stream_get_flags (stream) & VALENT_MIXER_STREAM_SINK) != 0)
    g_set_object (&self->default_output, stream);

  g_signal_connect (stream,
                    "notify",
                    G_CALLBACK (on_stream_changed),
                    control);

  VALENT_MIXER_CONTROL_CLASS (valent_mock_mixer_control_parent_class)->stream_added (control,
                                                                                     stream);
}

static void
valent_mock_mixer_control_stream_removed (ValentMixerControl *control,
                                          ValentMixerStream  *stream)
{
  ValentMockMixerControl *self = VALENT_MOCK_MIXER_CONTROL (control);

  if (self->default_input == stream)
    g_clear_object (&self->default_input);

  if (self->default_output == stream)
    g_clear_object (&self->default_output);

  g_signal_handlers_disconnect_by_func (stream, on_stream_changed, control);

  VALENT_MIXER_CONTROL_CLASS (valent_mock_mixer_control_parent_class)->stream_removed (control,
                                                                                       stream);
}


/*
 * GObject
 */
static void
valent_mock_mixer_control_dispose (GObject *object)
{
  ValentMockMixerControl *self = VALENT_MOCK_MIXER_CONTROL (object);

  g_clear_object (&self->default_input);
  g_clear_object (&self->default_output);

  G_OBJECT_CLASS (valent_mock_mixer_control_parent_class)->dispose (object);
}

static void
valent_mock_mixer_control_class_init (ValentMockMixerControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMixerControlClass *control_class = VALENT_MIXER_CONTROL_CLASS (klass);

  object_class->dispose = valent_mock_mixer_control_dispose;

  control_class->get_default_input = valent_mock_mixer_control_get_default_input;
  control_class->get_default_output = valent_mock_mixer_control_get_default_output;
  control_class->stream_added = valent_mock_mixer_control_stream_added;
  control_class->stream_removed = valent_mock_mixer_control_stream_removed;
}

static void
valent_mock_mixer_control_init (ValentMockMixerControl *self)
{
  if (test_instance == NULL)
    {
      test_instance = VALENT_MIXER_CONTROL (self);
      g_object_add_weak_pointer (G_OBJECT (test_instance),
                                 (gpointer)&test_instance);
    }
}

/**
 * valent_mock_mixer_control_get_instance:
 *
 * Get the #ValentMockMixerControl instance.
 *
 * Returns: (transfer none) (nullable): a #ValentMixerControl
 */
ValentMixerControl *
valent_mock_mixer_control_get_instance (void)
{
  return test_instance;
}

