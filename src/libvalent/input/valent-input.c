// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input"

#include "config.h"

#include <glib-object.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-input.h"
#include "valent-input-adapter.h"


/**
 * ValentInput:
 *
 * A class for controlling pointer and keyboard devices.
 *
 * `ValentInput` is an abstraction of virtual input devices, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.InputAdapter] to provide an interface to
 * control the pointer and keyboard.
 *
 * Since: 1.0
 */

struct _ValentInput
{
  ValentComponent     parent_instance;

  ValentInputAdapter *default_adapter;
  GPtrArray          *adapters; /* complete list of adapters */
  GListModel         *exports;  /* adapters marked for export */
  GPtrArray          *items;    /* adapters exposed by GListModel */
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentInput, valent_input, VALENT_TYPE_COMPONENT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))


static ValentInput *default_input = NULL;


static void
on_items_changed (GListModel   *list,
                  unsigned int  position,
                  unsigned int  removed,
                  unsigned int  added,
                  ValentInput  *self)
{
  unsigned int real_position = 0;

  VALENT_ENTRY;

  g_assert (G_IS_LIST_MODEL (list));
  g_assert (VALENT_IS_INPUT (self));

  /* Translate the adapter position */
  for (unsigned int i = 0; i < self->adapters->len; i++)
    {
      GListModel *adapter = g_ptr_array_index (self->adapters, i);

      if (adapter == list)
        break;

      real_position += g_list_model_get_n_items (adapter);
    }

  real_position += position;

  /* Propagate the changes */
  for (unsigned int i = 0; i < removed; i++)
    {
      g_autoptr (ValentInputAdapter) adapter = NULL;

      adapter = g_ptr_array_steal_index (self->items, real_position);

      VALENT_NOTE ("removed %s", G_OBJECT_TYPE_NAME (adapter));
    }

  for (unsigned int i = 0; i < added; i++)
    {
      ValentInputAdapter *adapter = NULL;

      adapter = g_list_model_get_item (list, position + i);
      g_ptr_array_insert (self->items, real_position + i, adapter);

      VALENT_NOTE ("added %s", G_OBJECT_TYPE_NAME (adapter));
    }

  g_list_model_items_changed (G_LIST_MODEL (self), real_position, removed, added);

  VALENT_EXIT;
}

/*
 * GListModel
 */
static gpointer
valent_input_get_item (GListModel   *list,
                       unsigned int  position)
{
  ValentInput *self = VALENT_INPUT (list);

  g_assert (VALENT_IS_INPUT (self));

  if G_UNLIKELY (position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static GType
valent_input_get_item_type (GListModel *list)
{
  return VALENT_TYPE_INPUT_ADAPTER;
}

static unsigned int
valent_input_get_n_items (GListModel *list)
{
  ValentInput *self = VALENT_INPUT (list);

  g_assert (VALENT_IS_INPUT (self));

  return self->items->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_input_get_item;
  iface->get_item_type = valent_input_get_item_type;
  iface->get_n_items = valent_input_get_n_items;
}

/*
 * ValentComponent
 */
static void
valent_input_bind_preferred (ValentComponent *component,
                             GObject         *extension)
{
  ValentInput *self = VALENT_INPUT (component);
  ValentInputAdapter *adapter = VALENT_INPUT_ADAPTER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_INPUT (self));
  g_assert (adapter == NULL || VALENT_IS_INPUT_ADAPTER (adapter));

  self->default_adapter = adapter;

  VALENT_EXIT;
}

/*
 * ValentObject
 */
static void
valent_input_destroy (ValentObject *object)
{
  ValentInput *self = VALENT_INPUT (object);

  g_list_store_remove_all (G_LIST_STORE (self->exports));

  VALENT_OBJECT_CLASS (valent_input_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_input_finalize (GObject *object)
{
  ValentInput *self = VALENT_INPUT (object);

  g_clear_object (&self->exports);
  g_clear_pointer (&self->adapters, g_ptr_array_unref);
  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_input_parent_class)->finalize (object);
}

static void
valent_input_class_init (ValentInputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->finalize = valent_input_finalize;

  vobject_class->destroy = valent_input_destroy;

  component_class->bind_preferred = valent_input_bind_preferred;
}

static void
valent_input_init (ValentInput *self)
{
  self->adapters = g_ptr_array_new_with_free_func (g_object_unref);
  self->items = g_ptr_array_new_with_free_func (g_object_unref);

  self->exports = G_LIST_MODEL (g_list_store_new (VALENT_TYPE_INPUT_ADAPTER));
  g_signal_connect_object (self->exports,
                           "items-changed",
                           G_CALLBACK (on_items_changed),
                           self, 0);
  g_ptr_array_add (self->adapters, g_object_ref (self->exports));
}

/**
 * valent_input_get_default:
 *
 * Get the default [class@Valent.Input].
 *
 * Returns: (transfer none) (not nullable): a `ValentInput`
 *
 * Since: 1.0
 */
ValentInput *
valent_input_get_default (void)
{
  if (default_input == NULL)
    {
      default_input = g_object_new (VALENT_TYPE_INPUT,
                                    "plugin-domain", "input",
                                    "plugin-type",   VALENT_TYPE_INPUT_ADAPTER,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_input),
                                 (gpointer)&default_input);
    }

  return default_input;
}

/**
 * valent_input_export_adapter:
 * @input: a `ValentInput`
 * @adapter: a `ValentInputAdapter`
 *
 * Export @adapter on all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_input_export_adapter (ValentInput        *input,
                             ValentInputAdapter *adapter)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));
  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  g_list_store_append (G_LIST_STORE (input->exports), adapter);

  VALENT_EXIT;
}

/**
 * valent_input_unexport_adapter:
 * @input: a `ValentInput`
 * @adapter: a `ValentInputAdapter`
 *
 * Unexport @adapter from all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_input_unexport_adapter (ValentInput        *input,
                               ValentInputAdapter *adapter)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));
  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  if (!g_list_store_find (G_LIST_STORE (input->exports), adapter, &position))
    {
      g_critical ("%s(): unknown adapter %s",
                  G_STRFUNC,
                  G_OBJECT_TYPE_NAME (adapter));
      return;
    }

  g_list_store_remove (G_LIST_STORE (input->exports), position);

  VALENT_EXIT;
}

/**
 * valent_input_keyboard_keysym:
 * @input: a `ValentInput`
 * @keysym: a keysym
 * @state: %TRUE to press, or %FALSE to release
 *
 * Press or release @keysym.
 *
 * Since: 1.0
 */
void
valent_input_keyboard_keysym (ValentInput  *input,
                              uint32_t      keysym,
                              gboolean      state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_keyboard_keysym (input->default_adapter, keysym, state);

  VALENT_EXIT;
}

/**
 * valent_input_pointer_axis:
 * @input: a `ValentInput`
 * @dx: movement on x-axis
 * @dy: movement on y-axis
 *
 * Scroll the surface under the pointer (@dx, @dy), relative to its current
 * position.
 *
 * Since: 1.0
 */
void
valent_input_pointer_axis (ValentInput *input,
                           double       dx,
                           double       dy)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_pointer_axis (input->default_adapter, dx, dy);

  VALENT_EXIT;
}

/**
 * valent_input_pointer_button:
 * @input: a `ValentInput`
 * @button: a button
 * @state: %TRUE to press, or %FALSE to release
 *
 * Press or release @button.
 *
 * Since: 1.0
 */
void
valent_input_pointer_button (ValentInput  *input,
                             unsigned int  button,
                             gboolean      state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_pointer_button (input->default_adapter, button, state);

  VALENT_EXIT;
}

/**
 * valent_input_pointer_motion:
 * @input: a `ValentInput`
 * @dx: position on x-axis
 * @dy: position on y-axis
 *
 * Move the pointer (@dx, @dy), relative to its current position.
 *
 * Since: 1.0
 */
void
valent_input_pointer_motion (ValentInput *input,
                             double       dx,
                             double       dy)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_pointer_motion (input->default_adapter, dx, dy);

  VALENT_EXIT;
}

