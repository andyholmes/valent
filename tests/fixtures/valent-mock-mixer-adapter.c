// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <valent.h>

#include "valent-mock-mixer-adapter.h"


struct _ValentMockMixerAdapter
{
  ValentMixerAdapter  parent_instance;

  ValentMixerStream  *default_input;
  ValentMixerStream  *default_output;
};

G_DEFINE_FINAL_TYPE (ValentMockMixerAdapter, valent_mock_mixer_adapter, VALENT_TYPE_MIXER_ADAPTER)

static void
valent_mock_mixer_adapter_items_changed (GListModel   *list,
                                         unsigned int  position,
                                         unsigned int  removed,
                                         unsigned int  added)
{
  ValentMockMixerAdapter *self = VALENT_MOCK_MIXER_ADAPTER (list);

  if (added > 0)
    {
      g_autoptr (ValentMixerStream) stream = g_list_model_get_item (list, position);

      if (self->default_input == NULL &&
          valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
        valent_mixer_adapter_set_default_input (VALENT_MIXER_ADAPTER (self),
                                                stream);
      else if (self->default_output == NULL &&
          valent_mixer_stream_get_direction (stream) == VALENT_MIXER_OUTPUT)
        valent_mixer_adapter_set_default_output (VALENT_MIXER_ADAPTER (self),
                                                 stream);
    }

  g_signal_chain_from_overridden_handler (list, position, removed, added);
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

  g_signal_override_class_handler ("items-changed",
                                   VALENT_TYPE_MOCK_MIXER_ADAPTER,
                                   G_CALLBACK (valent_mock_mixer_adapter_items_changed));
}

static void
valent_mock_mixer_adapter_init (ValentMockMixerAdapter *self)
{
}

