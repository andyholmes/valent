// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CONTACTS_INSIDE) && !defined (VALENT_CONTACTS_COMPILATION)
# error "Only <libvalent-contacts.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-contact-store.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACTS_ADAPTER (valent_contacts_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentContactsAdapter, valent_contacts_adapter, VALENT, CONTACTS_ADAPTER, GObject)

struct _ValentContactsAdapterClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*load_async)    (ValentContactsAdapter  *adapter,
                                   GCancellable           *cancellable,
                                   GAsyncReadyCallback     callback,
                                   gpointer                user_data);
  gboolean       (*load_finish)   (ValentContactsAdapter  *adapter,
                                   GAsyncResult           *result,
                                   GError                **error);

  /* signals */
  void           (*store_added)   (ValentContactsAdapter  *adapter,
                                   ValentContactStore     *store);
  void           (*store_removed) (ValentContactsAdapter  *adapter,
                                   ValentContactStore     *store);
};

VALENT_AVAILABLE_IN_1_0
void        valent_contacts_adapter_emit_store_added   (ValentContactsAdapter  *adapter,
                                                        ValentContactStore     *store);
VALENT_AVAILABLE_IN_1_0
void        valent_contacts_adapter_emit_store_removed (ValentContactsAdapter  *adapter,
                                                        ValentContactStore     *store);
VALENT_AVAILABLE_IN_1_0
GPtrArray * valent_contacts_adapter_get_stores         (ValentContactsAdapter  *adapter);
VALENT_AVAILABLE_IN_1_0
void        valent_contacts_adapter_load_async         (ValentContactsAdapter  *adapter,
                                                        GCancellable           *cancellable,
                                                        GAsyncReadyCallback     callback,
                                                        gpointer                user_data);
VALENT_AVAILABLE_IN_1_0
gboolean    valent_contacts_adapter_load_finish        (ValentContactsAdapter  *adapter,
                                                        GAsyncResult           *result,
                                                        GError                **error);

G_END_DECLS

