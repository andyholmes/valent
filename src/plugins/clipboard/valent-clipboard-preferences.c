// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-clipboard-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-clipboard-preferences.h"


struct _ValentClipboardPreferences
{
  AdwPreferencesPage  parent_instance;

  GSettings          *settings;
  char               *plugin_context;

  /* Template widgets */
  GtkSwitch          *sync_pull;
  GtkSwitch          *sync_push;
};

/* Interfaces */
static void valent_plugin_preferences_iface_init (ValentPluginPreferencesInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentClipboardPreferences, valent_clipboard_preferences, ADW_TYPE_PREFERENCES_PAGE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_PLUGIN_PREFERENCES, valent_plugin_preferences_iface_init))


enum {
  PROP_0,
  PROP_PLUGIN_CONTEXT,
  N_PROPERTIES
};


/*
 * ValentPluginPreferences
 */
static void
valent_plugin_preferences_iface_init (ValentPluginPreferencesInterface *iface)
{
}


/*
 * GObject
 */
static void
valent_clipboard_preferences_constructed (GObject *object)
{
  ValentClipboardPreferences *self = VALENT_CLIPBOARD_PREFERENCES (object);

  /* Setup GSettings */
  self->settings = valent_device_plugin_new_settings (self->plugin_context,
                                                      "clipboard");

  g_settings_bind (self->settings,  "auto-pull",
                   self->sync_pull, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings,  "auto-push",
                   self->sync_push, "active",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (valent_clipboard_preferences_parent_class)->constructed (object);
}

static void
valent_clipboard_preferences_finalize (GObject *object)
{
  ValentClipboardPreferences *self = VALENT_CLIPBOARD_PREFERENCES (object);

  g_clear_pointer (&self->plugin_context, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_clipboard_preferences_parent_class)->finalize (object);
}

static void
valent_clipboard_preferences_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  ValentClipboardPreferences *self = VALENT_CLIPBOARD_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      g_value_set_string (value, self->plugin_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_clipboard_preferences_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  ValentClipboardPreferences *self = VALENT_CLIPBOARD_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      self->plugin_context = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_clipboard_preferences_class_init (ValentClipboardPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_clipboard_preferences_constructed;
  object_class->finalize = valent_clipboard_preferences_finalize;
  object_class->get_property = valent_clipboard_preferences_get_property;
  object_class->set_property = valent_clipboard_preferences_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/clipboard/valent-clipboard-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentClipboardPreferences, sync_pull);
  gtk_widget_class_bind_template_child (widget_class, ValentClipboardPreferences, sync_push);

  g_object_class_override_property (object_class,
                                    PROP_PLUGIN_CONTEXT,
                                    "plugin-context");
}

static void
valent_clipboard_preferences_init (ValentClipboardPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

