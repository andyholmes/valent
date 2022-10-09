// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-clipboard-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-device.h>
#include <libvalent-ui.h>

#include "valent-clipboard-preferences.h"


struct _ValentClipboardPreferences
{
  ValentDevicePreferencesPage  parent_instance;

  /* Template widgets */
  GtkSwitch                   *sync_pull;
  GtkSwitch                   *sync_push;
};

G_DEFINE_TYPE (ValentClipboardPreferences, valent_clipboard_preferences, VALENT_TYPE_DEVICE_PREFERENCES_PAGE)


/*
 * GObject
 */
static void
valent_clipboard_preferences_constructed (GObject *object)
{
  ValentClipboardPreferences *self = VALENT_CLIPBOARD_PREFERENCES (object);
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;

  /* Setup GSettings */
  settings = valent_device_preferences_page_get_settings (page);

  g_settings_bind (settings,        "auto-pull",
                   self->sync_pull, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (settings,        "auto-push",
                   self->sync_push, "active",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (valent_clipboard_preferences_parent_class)->constructed (object);
}

static void
valent_clipboard_preferences_class_init (ValentClipboardPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_clipboard_preferences_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/clipboard/valent-clipboard-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentClipboardPreferences, sync_pull);
  gtk_widget_class_bind_template_child (widget_class, ValentClipboardPreferences, sync_push);
}

static void
valent_clipboard_preferences_init (ValentClipboardPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

