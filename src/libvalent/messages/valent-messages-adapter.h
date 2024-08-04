// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <gio/gio.h>

#include "../core/valent-extension.h"
#include "valent-message.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MESSAGES_ADAPTER (valent_messages_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentMessagesAdapter, valent_messages_adapter, VALENT, MESSAGES_ADAPTER, ValentExtension)

/**
 * ValentMessagesAdapterClass:
 * @export_adapter: the virtual function pointer for valent_messages_adapter_export_adapter()
 * @unexport_adapter: the virtual function pointer for valent_messages_adapter_unexport_adapter()
 *
 * The virtual function table for `ValentMessagesAdapter`.
 *
 * Since: 1.0
 */
struct _ValentMessagesAdapterClass
{
  ValentExtensionClass  parent_class;

  void                  (*send_message)        (ValentMessagesAdapter  *adapter,
                                                ValentMessage          *message,
                                                GCancellable           *cancellable,
                                                GAsyncReadyCallback     callback,
                                                gpointer                user_data);
  gboolean              (*send_message_finish) (ValentMessagesAdapter  *adapter,
                                                GAsyncResult           *result,
                                                GError                **error);

  /* virtual functions */
  void                  (*export_adapter)      (ValentMessagesAdapter  *adapter,
                                                ValentMessagesAdapter  *object);
  void                  (*unexport_adapter)    (ValentMessagesAdapter  *adapter,
                                                ValentMessagesAdapter  *object);

  /*< private >*/
  gpointer              padding[8];
};

VALENT_AVAILABLE_IN_1_0
void       valent_messages_adapter_send_message        (ValentMessagesAdapter  *adapter,
                                                        ValentMessage          *message,
                                                        GCancellable           *cancellable,
                                                        GAsyncReadyCallback     callback,
                                                        gpointer                user_data);
VALENT_AVAILABLE_IN_1_0
gboolean   valent_messages_adapter_send_message_finish (ValentMessagesAdapter  *adapter,
                                                        GAsyncResult           *result,
                                                        GError                **error);
VALENT_AVAILABLE_IN_1_0
void       valent_messages_adapter_export_adapter      (ValentMessagesAdapter  *adapter,
                                                        ValentMessagesAdapter  *object);
VALENT_AVAILABLE_IN_1_0
void       valent_messages_adapter_unexport_adapter    (ValentMessagesAdapter  *adapter,
                                                        ValentMessagesAdapter  *object);

G_END_DECLS

