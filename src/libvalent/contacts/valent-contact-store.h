// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CONTACTS_INSIDE) && !defined (VALENT_CONTACTS_COMPILATION)
# error "Only <libvalent-contacts.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-eds.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_STORE (valent_contact_store_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentContactStore, valent_contact_store, VALENT, CONTACT_STORE, ValentObject)

struct _ValentContactStoreClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*add_contacts)    (ValentContactStore   *store,
                                          GSList               *contacts,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
  void                (*remove_contacts) (ValentContactStore   *store,
                                          GSList               *uids,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
  void                (*query)           (ValentContactStore   *store,
                                          const char           *query,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
  void                (*get_contact)     (ValentContactStore   *store,
                                          const char           *uid,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);

  /* signals */
  void                (*contact_added)   (ValentContactStore   *store,
                                          EContact             *contact);
  void                (*contact_removed) (ValentContactStore   *store,
                                          const char           *uid);

  /*< private >*/
  gpointer            padding[8];
};


VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_contact_added          (ValentContactStore   *store,
                                                          EContact             *contact);
VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_contact_removed        (ValentContactStore   *store,
                                                          const char           *uid);
VALENT_AVAILABLE_IN_1_0
const char * valent_contact_store_get_name               (ValentContactStore   *store);
VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_set_name               (ValentContactStore   *store,
                                                          const char           *name);
VALENT_AVAILABLE_IN_1_0
ESource    * valent_contact_store_get_source             (ValentContactStore   *store);
VALENT_AVAILABLE_IN_1_0
const char * valent_contact_store_get_uid                (ValentContactStore   *store);

VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_add_contact            (ValentContactStore   *store,
                                                          EContact             *contact,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_add_contacts           (ValentContactStore   *store,
                                                          GSList               *contacts,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
gboolean     valent_contact_store_add_contacts_finish    (ValentContactStore   *store,
                                                          GAsyncResult         *result,
                                                          GError              **error);
VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_remove_contact         (ValentContactStore   *store,
                                                          const char           *uid,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_remove_contacts        (ValentContactStore   *store,
                                                          GSList               *uids,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
gboolean     valent_contact_store_remove_contacts_finish (ValentContactStore   *store,
                                                          GAsyncResult         *result,
                                                          GError              **error);
VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_get_contact            (ValentContactStore   *store,
                                                          const char           *uid,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
EContact   * valent_contact_store_get_contact_finish     (ValentContactStore   *store,
                                                          GAsyncResult         *result,
                                                          GError              **error);
VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_get_contacts           (ValentContactStore   *store,
                                                          char                **uids,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
void         valent_contact_store_query                  (ValentContactStore   *store,
                                                          const char           *query,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
GSList     * valent_contact_store_query_finish           (ValentContactStore   *store,
                                                          GAsyncResult         *result,
                                                          GError              **error);

G_END_DECLS
