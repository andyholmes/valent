// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libvalent-core.h>
#include <libvalent-mixer.h>

#include "valent-mock-mixer-adapter.h"


struct _ValentMockMixerAdapter
{
  ValentMixerAdapter  parent_instance;

  ValentMixerStream  *default_input;
  ValentMixerStream  *default_output;
};

G_DEFINE_TYPE (ValentMockMixerAdapter, valent_mock_mixer_adapter, VALENT_TYPE_MIXER_ADAPTER)


static ValentMixerAdapter *test_instance = NULL;

static void
on_stream_changed (ValentMixerStream  *stream,
                   GParamSpec         *pspec,
                   ValentMixerAdapter *adapter)
{
  g_assert (VALENT_IS_MOCK_MIXER_ADAPTER (adapter));

  valent_mixer_adapter_emit_stream_changed (adapter, stream);
}

/*
 * ValentMixerAdapter
 */
static ValentMixerStream *
valent_mock_mixer_adapter_get_default_input (ValentMixerAdapter *adapter)
{
  ValentMockMixerAdapter *self = VALENT_MOCK_MIXER_ADAPTER (adapter);

  return self->default_input;
}

static void
valent_mock_mixer_adapter_set_default_input (ValentMixerAdapter *adapter,
                                             ValentMixerStream  *stream)
{
  ValentMockMixerAdapter *self = VALENT_MOCK_MIXER_ADAPTER (adapter);

  if (g_set_object (&self->default_input, stream))
    g_object_notify (G_OBJECT (adapter), "default-input");
}

static ValentMixerStream *
valent_mock_mixer_adapter_get_default_output (ValentMixerAdapter *adapter)
{
  ValentMockMixerAdapter *self = VALENT_MOCK_MIXER_ADAPTER (adapter);

  return self->default_output;
}

static void
valent_mock_mixer_adapter_set_default_output (ValentMixerAdapter *adapter,
                                              ValentMixerStream  *stream)
{
  ValentMockMixerAdapter *self = VALENT_MOCK_MIXER_ADAPTER (adapter);

  if (g_set_object (&self->default_output, stream))
    g_object_notify (G_OBJECT (adapter), "default-output");
}

static void
valent_mock_mixer_adapter_stream_added (ValentMixerAdapter *adapter,
                                        ValentMixerStream  *stream)
{
  ValentMockMixerAdapter *self = VALENT_MOCK_MIXER_ADAPTER (adapter);
  ValentMixerDirection direction;

  /* Set a default input/output automatically, for convenience in tests */
  direction = valent_mixer_stream_get_direction (stream);

  if (self->default_input == NULL && direction == VALENT_MIXER_INPUT)
    valent_mixer_adapter_set_default_input (adapter, stream);

  if (self->default_output == NULL && direction == VALENT_MIXER_OUTPUT)
    valent_mixer_adapter_set_default_output (adapter, stream);

  g_signal_connect (stream,
                    "notify",
                    G_CALLBACK (on_stream_changed),
                    adapter);

  VALENT_MIXER_ADAPTER_CLASS (valent_mock_mixer_adapter_parent_class)->stream_added (adapter,
                                                                                     stream);
}

static void
valent_mock_mixer_adapter_stream_removed (ValentMixerAdapter *adapter,
                                          ValentMixerStream  *stream)
{
  ValentMockMixerAdapter *self = VALENT_MOCK_MIXER_ADAPTER (adapter);

  if (self->default_input == stream)
    {
      g_clear_object (&self->default_input);
      g_object_notify (G_OBJECT (adapter), "default-input");
    }

  if (self->default_output == stream)
    {
      g_clear_object (&self->default_output);
      g_object_notify (G_OBJECT (adapter), "default-output");
    }

  g_signal_handlers_disconnect_by_func (stream, on_stream_changed, adapter);

  VALENT_MIXER_ADAPTER_CLASS (valent_mock_mixer_adapter_parent_class)->stream_removed (adapter,
                                                                                       stream);
}

/*
 * GObject
 */
static void
valent_mock_mixer_adapter_dispose (GObject *object)
{
  ValentMockMixerAdapter *self = VALENT_MOCK_MIXER_ADAPTER (object);

  g_clear_object (&self->default_input);
  g_clear_object (&self->default_output);

  G_OBJECT_CLASS (valent_mock_mixer_adapter_parent_class)->dispose (object);
}

static void
valent_mock_mixer_adapter_class_init (ValentMockMixerAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMixerAdapterClass *adapter_class = VALENT_MIXER_ADAPTER_CLASS (klass);

  object_class->dispose = valent_mock_mixer_adapter_dispose;

  adapter_class->get_default_input = valent_mock_mixer_adapter_get_default_input;
  adapter_class->set_default_input = valent_mock_mixer_adapter_set_default_input;
  adapter_class->get_default_output = valent_mock_mixer_adapter_get_default_output;
  adapter_class->set_default_output = valent_mock_mixer_adapter_set_default_output;
  adapter_class->stream_added = valent_mock_mixer_adapter_stream_added;
  adapter_class->stream_removed = valent_mock_mixer_adapter_stream_removed;
}

static void
valent_mock_mixer_adapter_init (ValentMockMixerAdapter *self)
{
  if (test_instance == NULL)
    {
      test_instance = VALENT_MIXER_ADAPTER (self);
      g_object_add_weak_pointer (G_OBJECT (test_instance),
                                 (gpointer)&test_instance);
    }
}

/**
 * valent_mock_mixer_adapter_get_instance:
 *
 * Get the #ValentMockMixerAdapter instance.
 *
 * Returns: (transfer none) (nullable): a #ValentMixerAdapter
 */
ValentMixerAdapter *
valent_mock_mixer_adapter_get_instance (void)
{
  return test_instance;
}

