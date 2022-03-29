// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-preferences-page"

#include "config.h"

#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-mock-preferences-page.h"


struct _ValentMockPreferencesPage
{
  AdwPreferencesPage  parent_instance;

  PeasPluginInfo     *plugin_info;
  GSettings          *settings;
};

/* Interfaces */
static void valent_preferences_page_iface_init (ValentPreferencesPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMockPreferencesPage, valent_mock_preferences_page, ADW_TYPE_PREFERENCES_PAGE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_PREFERENCES_PAGE, valent_preferences_page_iface_init))


enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};


/*
 * ValentPreferencesPage
 */
static void
valent_preferences_page_iface_init (ValentPreferencesPageInterface *iface)
{
}


/*
 * GObject
 */
static void
valent_mock_preferences_page_finalize (GObject *object)
{
  ValentMockPreferencesPage *self = VALENT_MOCK_PREFERENCES_PAGE (object);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_mock_preferences_page_parent_class)->finalize (object);
}

static void
valent_mock_preferences_page_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  ValentMockPreferencesPage *self = VALENT_MOCK_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, self->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mock_preferences_page_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  ValentMockPreferencesPage *self = VALENT_MOCK_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      self->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mock_preferences_page_class_init (ValentMockPreferencesPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_mock_preferences_page_finalize;
  object_class->get_property = valent_mock_preferences_page_get_property;
  object_class->set_property = valent_mock_preferences_page_set_property;

  g_object_class_override_property (object_class, PROP_PLUGIN_INFO, "plugin-info");
}

static void
valent_mock_preferences_page_init (ValentMockPreferencesPage *self)
{
}

