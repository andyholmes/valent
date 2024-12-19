// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-plugin-row"

#include "config.h"

#include <adwaita.h>
#include <gio/gio.h>
#include <libpeas.h>
#include <valent.h>

#include "valent-plugin-row.h"


/**
 * ValentPluginRow:
 *
 * A [class@Gtk.ListBoxRow] used to toggle plugins.
 */

struct _ValentPluginRow
{
  AdwExpanderRow    parent;

  ValentDataSource *data_source;
  PeasPluginInfo   *plugin_info;
  char             *plugin_domain;
  GSettings        *settings;

  /* template */
  GtkSwitch        *plugin_enabled;
};

G_DEFINE_FINAL_TYPE (ValentPluginRow, valent_plugin_row, ADW_TYPE_EXPANDER_ROW)

typedef enum {
  PROP_DATA_SOURCE = 1,
  PROP_PLUGIN_DOMAIN,
  PROP_PLUGIN_INFO,
} ValentPluginRowProperty;

static GParamSpec *properties[PROP_PLUGIN_INFO + 1] = { NULL, };

static char *
strv_to_str (GObject            *object,
             const char * const *strv)
{
  return g_strjoinv (", ", (GStrv)strv);
}

/*
 * GObject
 */
static void
valent_plugin_row_constructed (GObject *object)
{
  ValentPluginRow *self = VALENT_PLUGIN_ROW (object);

  G_OBJECT_CLASS (valent_plugin_row_parent_class)->constructed (object);

  g_assert (self->data_source && self->plugin_info && self->plugin_domain);

  self->settings = valent_data_source_get_plugin_settings (self->data_source,
                                                           self->plugin_info,
                                                           NULL,
                                                           self->plugin_domain);
  g_settings_bind (self->settings,       "enabled",
                   self->plugin_enabled, "active",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_switch_set_active (self->plugin_enabled,
                         g_settings_get_boolean (self->settings, "enabled"));
}

static void
valent_plugin_row_dispose (GObject *object)
{
  ValentPluginRow *self = VALENT_PLUGIN_ROW (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), VALENT_TYPE_PLUGIN_ROW);

  G_OBJECT_CLASS (valent_plugin_row_parent_class)->dispose (object);
}

static void
valent_plugin_row_finalize (GObject *object)
{
  ValentPluginRow *self = VALENT_PLUGIN_ROW (object);

  g_clear_object (&self->data_source);
  g_clear_object (&self->plugin_info);
  g_clear_pointer (&self->plugin_domain, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_plugin_row_parent_class)->finalize (object);
}

static void
valent_plugin_row_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentPluginRow *self = VALENT_PLUGIN_ROW (object);

  switch ((ValentPluginRowProperty)prop_id)
    {
    case PROP_DATA_SOURCE:
      g_value_set_object (value, self->data_source);
      break;

    case PROP_PLUGIN_DOMAIN:
      g_value_set_string (value, self->plugin_domain);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_object (value, self->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_plugin_row_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ValentPluginRow *self = VALENT_PLUGIN_ROW (object);

  switch ((ValentPluginRowProperty)prop_id)
    {
    case PROP_DATA_SOURCE:
      self->data_source = g_value_dup_object (value);
      break;

    case PROP_PLUGIN_DOMAIN:
      self->plugin_domain = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_INFO:
      self->plugin_info = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_plugin_row_class_init (ValentPluginRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_plugin_row_constructed;
  object_class->dispose = valent_plugin_row_dispose;
  object_class->finalize = valent_plugin_row_finalize;
  object_class->get_property = valent_plugin_row_get_property;
  object_class->set_property = valent_plugin_row_set_property;

  properties [PROP_DATA_SOURCE] =
    g_param_spec_object ("data-source", NULL, NULL,
                         VALENT_TYPE_DATA_SOURCE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PLUGIN_DOMAIN] =
    g_param_spec_string ("plugin-domain", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-plugin-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentPluginRow, plugin_enabled);
  gtk_widget_class_bind_template_callback (widget_class, strv_to_str);
}

static void
valent_plugin_row_init (ValentPluginRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
