// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-device-preferences-page"

#include "config.h"

#include <libvalent-ui.h>

#include "valent-mock-device-preferences-page.h"


struct _ValentMockDevicePreferencesPage
{
  AdwPreferencesPage  parent_instance;

  char               *device_id;
  PeasPluginInfo     *plugin_info;
  GSettings          *settings;
};

/* Interfaces */
static void valent_device_preferences_page_iface_init (ValentDevicePreferencesPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMockDevicePreferencesPage, valent_mock_device_preferences_page, ADW_TYPE_PREFERENCES_PAGE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PREFERENCES_PAGE, valent_device_preferences_page_iface_init))


enum {
  PROP_0,
  PROP_DEVICE_ID,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};


/*
 * ValentDevicePreferencesPage
 */
static void
valent_device_preferences_page_iface_init (ValentDevicePreferencesPageInterface *iface)
{
}


/*
 * GObject
 */
static void
valent_mock_device_preferences_page_finalize (GObject *object)
{
  ValentMockDevicePreferencesPage *self = VALENT_MOCK_DEVICE_PREFERENCES_PAGE (object);

  g_clear_pointer (&self->device_id, g_free);

  G_OBJECT_CLASS (valent_mock_device_preferences_page_parent_class)->finalize (object);
}

static void
valent_mock_device_preferences_page_get_property (GObject    *object,
                                                  guint       prop_id,
                                                  GValue     *value,
                                                  GParamSpec *pspec)
{
  ValentMockDevicePreferencesPage *self = VALENT_MOCK_DEVICE_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_value_set_string (value, self->device_id);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, self->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mock_device_preferences_page_set_property (GObject      *object,
                                                  guint         prop_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec)
{
  ValentMockDevicePreferencesPage *self = VALENT_MOCK_DEVICE_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      self->device_id = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_INFO:
      self->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mock_device_preferences_page_class_init (ValentMockDevicePreferencesPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_mock_device_preferences_page_finalize;
  object_class->get_property = valent_mock_device_preferences_page_get_property;
  object_class->set_property = valent_mock_device_preferences_page_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE_ID, "device-id");
  g_object_class_override_property (object_class, PROP_PLUGIN_INFO, "plugin-info");
}

static void
valent_mock_device_preferences_page_init (ValentMockDevicePreferencesPage *self)
{
}

