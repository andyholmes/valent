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
};

G_DEFINE_FINAL_TYPE (ValentMessages, valent_messages, VALENT_TYPE_COMPONENT)

/*
 * GObject
 */
static void
valent_messages_class_init (ValentMessagesClass *klass)
{
}

static void
valent_messages_init (ValentMessages *self)
{
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
  static ValentMessages *default_instance = NULL;

  if (default_instance == NULL)
    {
      default_instance = g_object_new (VALENT_TYPE_MESSAGES,
                                       "plugin-domain", "messages",
                                       "plugin-type",   VALENT_TYPE_MESSAGES_ADAPTER,
                                       NULL);
      g_object_add_weak_pointer (G_OBJECT (default_instance),
                                 (gpointer)&default_instance);
    }

  return default_instance;
}

