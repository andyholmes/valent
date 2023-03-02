// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-component.h"
#include "valent-contact-store.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACTS_ADAPTER (valent_contacts_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentContactsAdapter, valent_contacts_adapter, VALENT, CONTACTS_ADAPTER, ValentObject)

struct _ValentContactsAdapterClass
{
  ValentObjectClass   parent_class;

  /* signals */
  void                (*store_added)   (ValentContactsAdapter  *adapter,
                                        ValentContactStore     *store);
  void                (*store_removed) (ValentContactsAdapter  *adapter,
                                        ValentContactStore     *store);

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
void        valent_contacts_adapter_store_added   (ValentContactsAdapter  *adapter,
                                                   ValentContactStore     *store);
VALENT_AVAILABLE_IN_1_0
void        valent_contacts_adapter_store_removed (ValentContactsAdapter  *adapter,
                                                   ValentContactStore     *store);
VALENT_AVAILABLE_IN_1_0
GPtrArray * valent_contacts_adapter_get_stores    (ValentContactsAdapter  *adapter);

G_END_DECLS

