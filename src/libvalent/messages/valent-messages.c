// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-messages"

#include "config.h"

#include <gio/gio.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-messages-adapter.h"

#include "valent-messages.h"


/**
 * ValentMessages:
 *
 * A class for managing address books.
 *
 * `ValentMessages` is an address book manager, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.MessagesAdapter] to provide an interface
 * to manage instances of [class@Valent.MessagesAdapter].
 *
 * Since: 1.0
 */

struct _ValentMessages
{
  ValentComponent  parent_instance;

  GPtrArray       *adapters;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMessages, valent_messages, VALENT_TYPE_COMPONENT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

static ValentMessages *default_messages = NULL;

/*
 * GListModel
 */
static gpointer
valent_messages_get_item (GListModel   *list,
                          unsigned int  position)
{
  ValentMessages *self = VALENT_MESSAGES (list);

  g_assert (VALENT_IS_MESSAGES (self));

  if G_UNLIKELY (position >= self->adapters->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->adapters, position));
}

static GType
valent_messages_get_item_type (GListModel *list)
{
  return VALENT_TYPE_MESSAGES_ADAPTER;
}

static unsigned int
valent_messages_get_n_items (GListModel *list)
{
  ValentMessages *self = VALENT_MESSAGES (list);

  g_assert (VALENT_IS_MESSAGES (self));

  return self->adapters->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_messages_get_item;
  iface->get_item_type = valent_messages_get_item_type;
  iface->get_n_items = valent_messages_get_n_items;
}

/*
 * ValentComponent
 */
static void
valent_messages_bind_extension (ValentComponent *component,
                                GObject         *extension)
{
  ValentMessages *self = VALENT_MESSAGES (component);
  GListModel *list = G_LIST_MODEL (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_MESSAGES (self));
  g_assert (VALENT_IS_MESSAGES_ADAPTER (extension));

  g_ptr_array_add (self->adapters, g_object_ref (extension));
  g_list_model_items_changed (list, self->adapters->len, 0, 1);

  VALENT_EXIT;
}

static void
valent_messages_unbind_extension (ValentComponent *component,
                                  GObject         *extension)
{
  ValentMessages *self = VALENT_MESSAGES (component);
  GListModel *list = G_LIST_MODEL (extension);
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MESSAGES (self));
  g_assert (VALENT_IS_MESSAGES_ADAPTER (extension));

  if (!g_ptr_array_find (self->adapters, extension, &position))
    {
      g_warning ("No such adapter \"%s\" found in \"%s\"",
                 G_OBJECT_TYPE_NAME (extension),
                 G_OBJECT_TYPE_NAME (component));
      return;
    }

  g_ptr_array_remove (self->adapters, extension);
  g_list_model_items_changed (list, position, 1, 0);

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_messages_finalize (GObject *object)
{
  ValentMessages *self = VALENT_MESSAGES (object);

  g_clear_pointer (&self->adapters, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_messages_parent_class)->finalize (object);
}

static void
valent_messages_class_init (ValentMessagesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->finalize = valent_messages_finalize;

  component_class->bind_extension = valent_messages_bind_extension;
  component_class->unbind_extension = valent_messages_unbind_extension;
}

static void
valent_messages_init (ValentMessages *self)
{
  self->adapters = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_messages_get_default:
 *
 * Get the default [class@Valent.Messages].
 *
 * Returns: (transfer none) (not nullable): a `ValentMessages`
 *
 * Since: 1.0
 */
ValentMessages *
valent_messages_get_default (void)
{
  if (default_messages == NULL)
    {
      default_messages = g_object_new (VALENT_TYPE_MESSAGES,
                                       "plugin-domain", "messages",
                                       "plugin-type",   VALENT_TYPE_MESSAGES_ADAPTER,
                                       NULL);
      g_object_add_weak_pointer (G_OBJECT (default_messages),
                                 (gpointer)&default_messages);
    }

  return default_messages;
}

/**
 * valent_messages_export_adapter:
 * @messages: a `ValentMessages`
 * @object: a `ValentMessagesAdapter`
 *
 * Export @object on all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_messages_export_adapter (ValentMessages        *messages,
                                ValentMessagesAdapter *object)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES (messages));
  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (object));

  if (g_ptr_array_find (messages->adapters, object, NULL))
    {
      g_critical ("%s(): known export %s",
                  G_STRFUNC,
                  G_OBJECT_TYPE_NAME (object));
      VALENT_EXIT;
    }

  // Starting at index `1` skips the exports GListModel
  for (unsigned int i = 1; i < messages->adapters->len; i++)
    {
      ValentMessagesAdapter *adapter = NULL;

      adapter = g_ptr_array_index (messages->adapters, i);
      valent_messages_adapter_export_adapter (adapter, object);
    }

  position = messages->adapters->len;
  g_ptr_array_add (messages->adapters, g_object_ref (object));
  g_list_model_items_changed (G_LIST_MODEL (messages), position, 0, 1);

  VALENT_EXIT;
}

/**
 * valent_messages_unexport_adapter:
 * @messages: a `ValentMessages`
 * @object: a `ValentMessagesAdapter`
 *
 * Unexport @object from all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_messages_unexport_adapter (ValentMessages        *messages,
                                  ValentMessagesAdapter *object)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES (messages));
  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (object));

  if (!g_ptr_array_find (messages->adapters, object, &position))
    {
      g_critical ("%s(): unknown export %s",
                  G_STRFUNC,
                  G_OBJECT_TYPE_NAME (object));
      VALENT_EXIT;
    }

  for (unsigned int i = 0; i < messages->adapters->len; i++)
    {
      ValentMessagesAdapter *adapter = NULL;

      adapter = g_ptr_array_index (messages->adapters, i);
      if (adapter != object)
        valent_messages_adapter_unexport_adapter (adapter, object);
    }

  g_ptr_array_remove (messages->adapters, g_object_ref (object));
  g_list_model_items_changed (G_LIST_MODEL (messages), position, 1, 0);

  VALENT_EXIT;
}

