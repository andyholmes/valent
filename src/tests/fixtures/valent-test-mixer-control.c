// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <math.h>
#include <libvalent-core.h>
#include <libvalent-mixer.h>

#include "valent-test-mixer-control.h"


struct _ValentTestMixerControl
{
  ValentMixerControl  parent_instance;

  ValentMixerStream  *default_input;
  ValentMixerStream  *default_output;
};

G_DEFINE_TYPE (ValentTestMixerControl, valent_test_mixer_control, VALENT_TYPE_MIXER_CONTROL)


static void
on_stream_changed (ValentMixerStream  *stream,
                   GParamSpec         *pspec,
                   ValentMixerControl *control)
{
  g_assert (VALENT_IS_TEST_MIXER_CONTROL (control));

  valent_mixer_control_emit_stream_changed (control, stream);
}

/*
 * ValentMixerControl
 */
static ValentMixerStream *
valent_test_mixer_control_get_default_input (ValentMixerControl *control)
{
  ValentTestMixerControl *self = VALENT_TEST_MIXER_CONTROL (control);

  return self->default_input;
}

static ValentMixerStream *
valent_test_mixer_control_get_default_output (ValentMixerControl *control)
{
  ValentTestMixerControl *self = VALENT_TEST_MIXER_CONTROL (control);

  return self->default_output;
}

static void
valent_test_mixer_control_stream_added (ValentMixerControl *control,
                                        ValentMixerStream  *stream)
{
  ValentTestMixerControl *self = VALENT_TEST_MIXER_CONTROL (control);

  if (self->default_input == NULL &&
      (valent_mixer_stream_get_flags (stream) & VALENT_MIXER_STREAM_SOURCE) != 0)
    self->default_input = g_object_ref (stream);

  if (self->default_output == NULL &&
      (valent_mixer_stream_get_flags (stream) & VALENT_MIXER_STREAM_SINK) != 0)
    self->default_output = g_object_ref (stream);

  g_signal_connect (stream,
                    "notify",
                    G_CALLBACK (on_stream_changed),
                    control);

  VALENT_MIXER_CONTROL_CLASS (valent_test_mixer_control_parent_class)->stream_added (control,
                                                                                     stream);
}

static void
valent_test_mixer_control_stream_removed (ValentMixerControl *control,
                                          ValentMixerStream  *stream)
{
  ValentTestMixerControl *self = VALENT_TEST_MIXER_CONTROL (control);

  if (self->default_input == stream)
    g_clear_object (&self->default_input);

  if (self->default_output == stream)
    g_clear_object (&self->default_output);

  g_signal_handlers_disconnect_by_func (stream, on_stream_changed, control);

  VALENT_MIXER_CONTROL_CLASS (valent_test_mixer_control_parent_class)->stream_removed (control,
                                                                                       stream);
}


/*
 * GObject
 */
static void
valent_test_mixer_control_dispose (GObject *object)
{
  ValentTestMixerControl *self = VALENT_TEST_MIXER_CONTROL (object);

  g_clear_object (&self->default_input);
  g_clear_object (&self->default_output);

  G_OBJECT_CLASS (valent_test_mixer_control_parent_class)->dispose (object);
}

static void
valent_test_mixer_control_class_init (ValentTestMixerControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMixerControlClass *control_class = VALENT_MIXER_CONTROL_CLASS (klass);

  object_class->dispose = valent_test_mixer_control_dispose;

  control_class->get_default_input = valent_test_mixer_control_get_default_input;
  control_class->get_default_output = valent_test_mixer_control_get_default_output;
  control_class->stream_added = valent_test_mixer_control_stream_added;
  control_class->stream_removed = valent_test_mixer_control_stream_removed;
}

static void
valent_test_mixer_control_init (ValentTestMixerControl *self)
{
}

