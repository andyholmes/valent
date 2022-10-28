// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-device-gadget"

#include "config.h"

#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-mock-device-gadget.h"


struct _ValentMockDeviceGadget
{
  GtkWidget     parent_instance;

  ValentDevice *device;
};

static void valent_device_gadget_iface_init (ValentDeviceGadgetInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMockDeviceGadget, valent_mock_device_gadget, GTK_TYPE_WIDGET,
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
valent_mock_device_gadget_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ValentMockDeviceGadget *self = VALENT_MOCK_DEVICE_GADGET (object);

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
valent_mock_device_gadget_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ValentMockDeviceGadget *self = VALENT_MOCK_DEVICE_GADGET (object);

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
valent_mock_device_gadget_class_init (ValentMockDeviceGadgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_mock_device_gadget_get_property;
  object_class->set_property = valent_mock_device_gadget_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_mock_device_gadget_init (ValentMockDeviceGadget *self)
{
}

