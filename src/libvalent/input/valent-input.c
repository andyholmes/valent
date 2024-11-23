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
  GPtrArray          *items;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentInput, valent_input, VALENT_TYPE_COMPONENT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

static void   valent_input_unbind_extension (ValentComponent *component,
                                             GObject         *extension);

static ValentInput *default_input = NULL;


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

static void
valent_input_bind_extension (ValentComponent *component,
                             GObject         *extension)
{
  ValentInput *self = VALENT_INPUT (component);
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_INPUT (self));
  g_assert (VALENT_IS_INPUT_ADAPTER (extension));

  if (g_ptr_array_find (self->items, extension, &position))
    {
      g_warning ("Adapter \"%s\" already exported in \"%s\"",
                 G_OBJECT_TYPE_NAME (extension),
                 G_OBJECT_TYPE_NAME (component));
      return;
    }

  g_signal_connect_object (extension,
                           "destroy",
                           G_CALLBACK (valent_input_unbind_extension),
                           self,
                           G_CONNECT_SWAPPED);

  position = self->items->len;
  g_ptr_array_add (self->items, g_object_ref (extension));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  VALENT_EXIT;
}

static void
valent_input_unbind_extension (ValentComponent *component,
                               GObject         *extension)
{
  ValentInput *self = VALENT_INPUT (component);
  g_autoptr (ValentExtension) item = NULL;
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_INPUT (self));
  g_assert (VALENT_IS_INPUT_ADAPTER (extension));

  if (!g_ptr_array_find (self->items, extension, &position))
    {
      g_warning ("Adapter \"%s\" not found in \"%s\"",
                 G_OBJECT_TYPE_NAME (extension),
                 G_OBJECT_TYPE_NAME (component));
      return;
    }

  g_signal_handlers_disconnect_by_func (extension, valent_input_unbind_extension, self);
  item = g_ptr_array_steal_index (self->items, position);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_input_finalize (GObject *object)
{
  ValentInput *self = VALENT_INPUT (object);

  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_input_parent_class)->finalize (object);
}

static void
valent_input_class_init (ValentInputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->finalize = valent_input_finalize;

  component_class->bind_preferred = valent_input_bind_preferred;
  component_class->bind_extension = valent_input_bind_extension;
  component_class->unbind_extension = valent_input_unbind_extension;
}

static void
valent_input_init (ValentInput *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
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

