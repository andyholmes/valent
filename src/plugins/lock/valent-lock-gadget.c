// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lock-gadget"

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-device.h>
#include <libvalent-ui.h>

#include "valent-lock-gadget.h"


struct _ValentLockGadget
{
  GtkWidget     parent_instance;

  ValentDevice *device;

  /* widgets */
  GtkWidget    *button;
};

static void valent_device_gadget_iface_init (ValentDeviceGadgetInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentLockGadget, valent_lock_gadget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_GADGET, valent_device_gadget_iface_init))


enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


/*
 * ValentDeviceGadget
 */
static void
valent_device_gadget_iface_init (ValentDeviceGadgetInterface *iface)
{
}

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
valent_lock_gadget_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentLockGadget *self = VALENT_LOCK_GADGET (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lock_gadget_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentLockGadget *self = VALENT_LOCK_GADGET (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lock_gadget_class_init (ValentLockGadgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_lock_gadget_dispose;
  object_class->get_property = valent_lock_gadget_get_property;
  object_class->set_property = valent_lock_gadget_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
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

