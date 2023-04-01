// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-clipboard-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-clipboard-preferences.h"


struct _ValentClipboardPreferences
{
  ValentDevicePreferencesGroup  parent_instance;

  /* template */
  GtkSwitch                   *sync_pull;
  GtkSwitch                   *sync_push;
};

G_DEFINE_FINAL_TYPE (ValentClipboardPreferences, valent_clipboard_preferences, VALENT_TYPE_DEVICE_PREFERENCES_GROUP)


/*
 * GObject
 */
static void
valent_clipboard_preferences_constructed (GObject *object)
{
  ValentClipboardPreferences *self = VALENT_CLIPBOARD_PREFERENCES (object);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;

  settings = valent_device_preferences_group_get_settings (group);
  g_settings_bind (settings,        "auto-pull",
                   self->sync_pull, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (settings,        "auto-push",
                   self->sync_push, "active",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (valent_clipboard_preferences_parent_class)->constructed (object);
}

static void
valent_clipboard_preferences_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_CLIPBOARD_PREFERENCES);

  G_OBJECT_CLASS (valent_clipboard_preferences_parent_class)->dispose (object);
}

static void
valent_clipboard_preferences_class_init (ValentClipboardPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_clipboard_preferences_constructed;
  object_class->dispose = valent_clipboard_preferences_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/clipboard/valent-clipboard-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentClipboardPreferences, sync_pull);
  gtk_widget_class_bind_template_child (widget_class, ValentClipboardPreferences, sync_push);
}

static void
valent_clipboard_preferences_init (ValentClipboardPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

