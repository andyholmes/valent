// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-gadget"

#include "config.h"

#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-device-gadget.h"


/**
 * ValentDeviceGadget:
 *
 * An abstract base class for device plugin gadgets.
 *
 * #ValentDeviceGadget is an base class for [class@Valent.DevicePlugin]
 * implementations that want to provide a small widget to display or control a
 * simple state (e.g. battery level).
 *
 * Since: 1.0
 */

typedef struct
{
  GtkWidget     parent_instance;

  ValentDevice *device;
} ValentDeviceGadgetPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentDeviceGadget, valent_device_gadget, GTK_TYPE_WIDGET)


enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GObject
 */
static void
valent_device_gadget_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentDeviceGadget *self = VALENT_DEVICE_GADGET (object);
  ValentDeviceGadgetPrivate *priv = valent_device_gadget_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, priv->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_gadget_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentDeviceGadget *self = VALENT_DEVICE_GADGET (object);
  ValentDeviceGadgetPrivate *priv = valent_device_gadget_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEVICE:
      priv->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_gadget_class_init (ValentDeviceGadgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = valent_device_gadget_get_property;
  object_class->set_property = valent_device_gadget_set_property;

  /**
   * ValentDeviceGadget:device:
   *
   * The [class@Valent.Device] this gadget is for.
   *
   * Since: 1.0
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
valent_device_gadget_init (ValentDeviceGadget *self)
{
}

