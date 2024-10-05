// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <libebook-contacts/libebook-contacts.h>
#include <valent.h>

#include "valent-message-store.h"

G_BEGIN_DECLS


static void
valent_test_message_store_new_cb (ValentMessagesAdapter *store,
                                  GAsyncResult       *result,
                                  gboolean           *done)
{
  g_autoptr (GError) error = NULL;

  valent_messages_adapter_add_messages_finish (store, result, &error);
  g_assert_no_error (error);

  if (done != NULL)
    *done = TRUE;
}

/**
 * valent_test_contact1:
 *
 * Get test contact #1.
 *
 * Returns: (transfer none): a `EContact`
 */
static inline EContact *
valent_test_contact1 (void)
{
  static EContact *contact = NULL;

  if G_UNLIKELY (contact == NULL)
    {
      g_autoptr (GBytes) bytes = NULL;

      bytes = g_resources_lookup_data ("/tests/contact.vcf", 0, NULL);
      contact = e_contact_new_from_vcard_with_uid (g_bytes_get_data (bytes, NULL),
                                                   "4077i252298cf8ded4bfe");
    }

  return contact;
}

/**
 * valent_test_contact2:
 *
 * Get test contact #2.
 *
 * Returns: (transfer none): a `EContact`
 */
static inline EContact *
valent_test_contact2 (void)
{
  static EContact *contact = NULL;

  if G_UNLIKELY (contact == NULL)
    {
      g_autoptr (GBytes) bytes = NULL;

      bytes = g_resources_lookup_data ("/tests/contact2.vcf", 0, NULL);
      contact = e_contact_new_from_vcard_with_uid (g_bytes_get_data (bytes, NULL),
                                                   "4077i252298cf8ded4bff");
    }

  return contact;
}

/**
 * valent_test_contact3:
 *
 * Get test contact #3.
 *
 * Returns: (transfer none): a `EContact`
 */
static inline EContact *
valent_test_contact3 (void)
{
  static EContact *contact = NULL;

  if G_UNLIKELY (contact == NULL)
    {
      g_autoptr (GBytes) bytes = NULL;

      bytes = g_resources_lookup_data ("/tests/contact3.vcf", 0, NULL);
      contact = e_contact_new_from_vcard_with_uid (g_bytes_get_data (bytes, NULL),
                                                   "4077i252298cf8ded4bfg");
    }

  return contact;
}

/**
 * valent_test_contact_store_new:
 *
 * Create a new `ValentContactsAdapter` for testing.
 *
 * Returns: (transfer full): a `ValentContactsAdapter`
 */
static inline ValentContactsAdapter *
valent_test_contact_store_new (void)
{
  return NULL;
}

static inline GPtrArray *
valent_test_sms_get_messages (void)
{
  static GPtrArray *messages = NULL;

  if (messages == NULL)
    {
      ValentMessage *message;
      GVariant *metadata;

      messages = g_ptr_array_new_with_free_func (g_object_unref);

      metadata = g_variant_new_parsed ("{'addresses': <[{'address': <'+1-234-567-8912'>},{'address': <'+1-234-567-8910'>}]>}");
      message = g_object_new (VALENT_TYPE_MESSAGE,
                              "box",       VALENT_MESSAGE_BOX_INBOX,
                              "date",      1,
                              "id",        1,
                              "metadata",  metadata,
                              "read",      FALSE,
                              "sender",    "+1-234-567-8912",
                              "text",      "Thread 1, Message 1",
                              "thread-id", 1,
                              NULL);
      g_ptr_array_add (messages, message);

      metadata = g_variant_new_parsed ("{'addresses': <[{'address': <'+1-234-567-8912'>}]>}");
      message = g_object_new (VALENT_TYPE_MESSAGE,
                               "box",       VALENT_MESSAGE_BOX_SENT,
                               "date",      2,
                               "id",        2,
                               "metadata",  metadata,
                               "read",      FALSE,
                               "sender",    NULL,
                               "text",      "Thread 1, Message 2",
                               "thread-id", 1,
                               NULL);
      g_ptr_array_add (messages, message);

      metadata = g_variant_new_parsed ("{'addresses': <[{'address': <'+1-234-567-8914'>}]>}");
      message = g_object_new (VALENT_TYPE_MESSAGE,
                              "box",       VALENT_MESSAGE_BOX_SENT,
                              "date",      3,
                              "id",        3,
                              "metadata",  metadata,
                              "read",      FALSE,
                              "sender",    NULL,
                              "text",      "Thread 2, Message 1",
                              "thread-id", 2,
                              NULL);
      g_ptr_array_add (messages, message);
    }

  return g_ptr_array_ref (messages);
}

/**
 * valent_test_message_store_new:
 *
 * Create a new `ValentMessagesAdapter` for testing.
 *
 * Returns: (transfer full): a `ValentMessagesAdapter`
 */
static inline ValentMessagesAdapter *
valent_test_message_store_new (void)
{
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (ValentMessagesAdapter) store = NULL;
  g_autoptr (GPtrArray) messages = NULL;
  gboolean done = FALSE;

  /* Prepare Store */
  context = g_object_new (VALENT_TYPE_CONTEXT,
                          "domain", "device",
                          "id",     "test-device",
                          NULL);
  store = g_object_new (VALENT_TYPE_MESSAGES_ADAPTER,
                        "parent", context,
                        NULL);
  messages = valent_test_sms_get_messages ();

  /* Add Messages */
  valent_messages_adapter_add_messages (store,
                                     messages,
                                     NULL,
                                     (GAsyncReadyCallback)valent_test_message_store_new_cb,
                                     &done);
  valent_test_await_boolean (&done);

  return g_steal_pointer (&store);
}

G_END_DECLS
