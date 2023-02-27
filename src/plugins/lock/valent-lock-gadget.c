// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lock-gadget"

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <valent.h>

#include "valent-lock-gadget.h"


struct _ValentLockGadget
{
  ValentDeviceGadget  parent_instance;

  /* widgets */
  GtkWidget          *button;
};

G_DEFINE_FINAL_TYPE (ValentLockGadget, valent_lock_gadget, VALENT_TYPE_DEVICE_GADGET)


/*
 * GObject
 */
static void
valent_lock_gadget_dispose (GObject *object)
{
  ValentLockGadget *self = VALENT_LOCK_GADGET (object);

  g_clear_pointer (&self->button, gtk_widget_unparent);

  G_OBJECT_CLASS (valent_lock_gadget_parent_class)->dispose (object);
}

static void
valent_lock_gadget_class_init (ValentLockGadgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_lock_gadget_dispose;
}

static void
valent_lock_gadget_init (ValentLockGadget *self)
{
  self->button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                               "action-name",  "device.lock.state",
                               "icon-name",    "channel-secure-symbolic",
                               "has-frame",    FALSE,
                               "tooltip-text", _("Lock"),
                               NULL);
  g_object_bind_property (self->button, "sensitive",
                          self->button, "visible",
                          G_BINDING_SYNC_CREATE);

  gtk_widget_set_parent (self->button, GTK_WIDGET (self));
}

