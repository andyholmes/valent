// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CONTACTS_INSIDE) && !defined (VALENT_CONTACTS_COMPILATION)
# error "Only <libvalent-contacts.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-eds.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_STORE (valent_contact_store_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentContactStore, valent_contact_store, VALENT, CONTACT_STORE, GObject)

struct _ValentContactStoreClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  gboolean       (*add)               (ValentContactStore   *store,
                                       EContact             *contact,
                                       GCancellable         *cancellable,
                                       GError              **error);
  void           (*add_async)         (ValentContactStore   *store,
                                       EContact             *contact,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
  gboolean       (*remove)            (ValentContactStore   *store,
                                       const char           *uid,
                                       GCancellable         *cancellable,
                                       GError              **error);
  void           (*remove_async)      (ValentContactStore   *store,
                                       const char           *uid,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
  GSList       * (*query)             (ValentContactStore   *store,
                                       const char           *query,
                                       GCancellable         *cancellable,
                                       GError              **error);
  void           (*query_async)       (ValentContactStore   *store,
                                       const char           *query,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
  EContact     * (*get_contact)       (ValentContactStore   *store,
                                       const char           *uid,
                                       GCancellable         *cancellable,
                                       GError              **error);
  void           (*get_contact_async) (ValentContactStore   *store,
                                       const char           *uid,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
  void           (*prepare_backend)   (ValentContactStore   *store);

  /* signals */
  void           (*contact_added)     (ValentContactStore *store,
                                       const char         *uid,
                                       EContact           *contact);
  void           (*contact_removed)   (ValentContactStore *store,
                                       const char         *uid,
                                       EContact           *contact);
};


void         valent_contact_store_emit_contact_added   (ValentContactStore   *store,
                                                        const char           *uid,
                                                        EContact             *contact);
void         valent_contact_store_emit_contact_removed (ValentContactStore   *store,
                                                        const char           *uid,
                                                        EContact             *contact);
const char * valent_contact_store_get_name             (ValentContactStore   *store);
void         valent_contact_store_set_name             (ValentContactStore   *store,
                                                        const char           *name);
ESource    * valent_contact_store_get_source           (ValentContactStore   *store);
const char * valent_contact_store_get_uid              (ValentContactStore   *store);

gboolean     valent_contact_store_add                  (ValentContactStore   *store,
                                                        EContact             *contact,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
void         valent_contact_store_add_async            (ValentContactStore   *store,
                                                        EContact             *contact,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
gboolean     valent_contact_store_add_finish           (ValentContactStore   *store,
                                                        GAsyncResult         *result,
                                                        GError              **error);
gboolean     valent_contact_store_remove               (ValentContactStore   *store,
                                                        const char           *uid,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
void         valent_contact_store_remove_async         (ValentContactStore   *store,
                                                        const char           *uid,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
gboolean     valent_contact_store_remove_finish        (ValentContactStore   *store,
                                                        GAsyncResult         *result,
                                                        GError              **error);
EContact *   valent_contact_store_get_contact          (ValentContactStore   *store,
                                                        const char           *uid,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
void         valent_contact_store_get_contact_async    (ValentContactStore   *store,
                                                        const char           *uid,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
EContact   * valent_contact_store_get_contact_finish   (ValentContactStore   *store,
                                                        GAsyncResult         *result,
                                                        GError              **error);
void         valent_contact_store_get_contacts_async   (ValentContactStore   *store,
                                                        char                **uids,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
GSList     * valent_contact_store_query                (ValentContactStore   *store,
                                                        const char           *query,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
void         valent_contact_store_query_async          (ValentContactStore   *store,
                                                        const char           *query,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
GSList     * valent_contact_store_query_finish         (ValentContactStore   *store,
                                                        GAsyncResult         *result,
                                                        GError              **error);

EContact   * valent_contact_store_dup_for_phone        (ValentContactStore   *store,
                                                        const char           *number);
void         valent_contact_store_dup_for_phone_async  (ValentContactStore   *store,
                                                        const char           *number,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
EContact   * valent_contact_store_dup_for_phone_finish (ValentContactStore   *store,
                                                        GAsyncResult         *result,
                                                        GError              **error);

G_END_DECLS
