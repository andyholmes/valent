// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014-2019 Christian Hergert <chergert@redhat.com>
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_OBJECT (valent_object_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentObject, valent_object, VALENT, OBJECT, GObject)

struct _ValentObjectClass
{
  GObjectClass   parent_class;

  void           (*destroy)     (ValentObject *object);
};

void           valent_object_lock              (ValentObject  *object);
void           valent_object_unlock            (ValentObject  *object);
GCancellable * valent_object_ref_cancellable   (ValentObject  *object);
gboolean       valent_object_in_destruction    (ValentObject  *object);
void           valent_object_destroy           (ValentObject  *object);

/* Utilities */
void           valent_object_notify            (gpointer       object,
                                                const char    *property_name);
void           valent_object_notify_by_pspec   (gpointer       object,
                                                GParamSpec    *pspec);

void           valent_object_list_free         (gpointer       list);
void           valent_object_slist_free        (gpointer       slist);

G_END_DECLS

