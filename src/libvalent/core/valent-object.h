// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014-2019 Christian Hergert <chergert@redhat.com>
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-version.h"

G_BEGIN_DECLS

#define VALENT_TYPE_OBJECT (valent_object_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentObject, valent_object, VALENT, OBJECT, GObject)

struct _ValentObjectClass
{
  GObjectClass   parent_class;

  /* signals */
  void           (*destroy)     (ValentObject *object);

  /*< private >*/
  gpointer       padding[8];
};

VALENT_AVAILABLE_IN_1_0
void           valent_object_lock              (ValentObject *object);
VALENT_AVAILABLE_IN_1_0
void           valent_object_unlock            (ValentObject *object);
VALENT_AVAILABLE_IN_1_0
GCancellable * valent_object_ref_cancellable   (ValentObject *object);
VALENT_AVAILABLE_IN_1_0
void           valent_object_destroy           (ValentObject *object);
VALENT_AVAILABLE_IN_1_0
gboolean       valent_object_in_destruction    (ValentObject *object);
VALENT_AVAILABLE_IN_1_0
void           valent_object_notify            (ValentObject *object,
                                                const char   *property_name);
VALENT_AVAILABLE_IN_1_0
void           valent_object_notify_by_pspec   (ValentObject *object,
                                                GParamSpec   *pspec);

G_END_DECLS

