// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-plugin-row"

#include "config.h"

#include <gtk/gtk.h>
#include <adwaita.h>
#include <libpeas/peas.h>
#include <pango/pango.h>
#include <libvalent-core.h>

#include "valent-plugin-preferences.h"
#include "valent-plugin-row.h"


struct _ValentPluginRow
{
  AdwActionRow    parent_instance;

  PeasPluginInfo *plugin_info;
  char           *plugin_context;
  GType           plugin_type;
  GSettings      *settings;

  GtkWidget      *sw;
  GtkWidget      *button;
};

enum {
  PROP_0,
  PROP_PLUGIN_CONTEXT,
  PROP_PLUGIN_INFO,
  PROP_PLUGIN_TYPE,
  N_PROPERTIES
};

G_DEFINE_TYPE (ValentPluginRow, valent_plugin_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GObject
 */
static void
valent_plugin_row_constructed (GObject *object)
{
  ValentPluginRow *self = VALENT_PLUGIN_ROW (object);
  const char *module, *name, *description, *icon_name;
  g_autofree char *path = NULL;

  g_assert (self->plugin_info != NULL);

  /* Set the standard row properties from the plugin info */
  module = peas_plugin_info_get_module_name (self->plugin_info);
  name = peas_plugin_info_get_name (self->plugin_info);
  description = peas_plugin_info_get_description (self->plugin_info);
  icon_name = peas_plugin_info_get_icon_name (self->plugin_info);

  gtk_widget_set_name (GTK_WIDGET (self), module);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), name);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self), description);
  adw_action_row_set_icon_name (ADW_ACTION_ROW (self), icon_name);

  /* Plugin Toggle */
  if (self->plugin_type == VALENT_TYPE_DEVICE_PLUGIN)
    path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/plugin/%s/",
                            self->plugin_context,
                            module);
  else
    path = g_strdup_printf ("/ca/andyholmes/valent/%s/plugin/%s/",
                            self->plugin_context,
                            module);

  self->settings = g_settings_new_with_path ("ca.andyholmes.Valent.Plugin", path);

  g_settings_bind (self->settings, "enabled",
                   self->sw,       "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Plugin Settings */
  if (peas_engine_provides_extension (valent_get_engine (),
                                      self->plugin_info,
                                      VALENT_TYPE_PLUGIN_PREFERENCES))
    {
      g_autofree char *page = NULL;

      if (self->plugin_context == NULL)
        page = g_strdup_printf ("win.page::/%s", module);
      else
        page = g_strdup_printf ("win.page::/%s/%s", self->plugin_context, module);

      gtk_actionable_set_detailed_action_name (GTK_ACTIONABLE (self->button),
                                               page);
      gtk_widget_set_sensitive (GTK_WIDGET (self->button), TRUE);
    }

  G_OBJECT_CLASS (valent_plugin_row_parent_class)->constructed (object);
}

static void
valent_plugin_row_finalize (GObject *object)
{
  ValentPluginRow *self = VALENT_PLUGIN_ROW (object);

  g_clear_pointer (&self->plugin_context, g_free);
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

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      g_value_set_string (value, self->plugin_context);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, self->plugin_info);
      break;

    case PROP_PLUGIN_TYPE:
      g_value_set_gtype (value, self->plugin_type);
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

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      self->plugin_context = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_INFO:
      self->plugin_info = g_value_get_boxed (value);
      break;

    case PROP_PLUGIN_TYPE:
      self->plugin_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_plugin_row_class_init (ValentPluginRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_plugin_row_constructed;
  object_class->finalize = valent_plugin_row_finalize;
  object_class->get_property = valent_plugin_row_get_property;
  object_class->set_property = valent_plugin_row_set_property;

  /**
   * ValentPluginRow:plugin-context
   *
   * The plugin_context or scope of the plugin, usually a device ID or %NULL to
   * indicate global scope.
   */
  properties [PROP_PLUGIN_CONTEXT] =
    g_param_spec_string ("plugin-context",
                         "Plugin Context",
                         "The context or scope of the plugin",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentPluginRow:plugin-info
   *
   * The #PeasPluginInfo for the plugin.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "The plugin info",
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentPluginRow:plugin-type
   *
   * The #PeasPluginInfo for the plugin.
   */
  properties [PROP_PLUGIN_TYPE] =
    g_param_spec_gtype ("plugin-type",
                        "Plugin Type",
                        "The plugin GType",
                        PEAS_TYPE_EXTENSION,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_plugin_row_init (ValentPluginRow *self)
{
  GtkWidget *box;
  GtkWidget *separator;

  /* Row Widget */
  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing",     8,
                      "valign",      GTK_ALIGN_CENTER,
                      NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (self), box);

  /* Enabled Switch */
  self->sw = g_object_new (GTK_TYPE_SWITCH,
                           "active",  TRUE,
                           "valign",  GTK_ALIGN_CENTER,
                           NULL);
  gtk_box_append (GTK_BOX (box), self->sw);

  /* Separator */
  separator = g_object_new (GTK_TYPE_SEPARATOR,
                            "orientation", GTK_ORIENTATION_VERTICAL,
                            NULL);
  gtk_box_append (GTK_BOX (box), separator);

  /* Preferences Button */
  self->button = g_object_new (GTK_TYPE_BUTTON,
                               "icon-name",   "emblem-system-symbolic",
                               "sensitive",   FALSE,
                               "valign",      GTK_ALIGN_CENTER,
                               NULL);
  gtk_box_append (GTK_BOX (box), self->button);
}

/**
 * valent_plugin_row_new:
 * @info: a #PeasPluginInfo
 * @context: (nullable): a context for the plugin
 * 
 * Create a new #ValentPluginRow.
 * 
 * Returns: a #GtkWidget
 */
GtkWidget *
valent_plugin_row_new (PeasPluginInfo *info,
                       const char     *context)
{
  return g_object_new (VALENT_TYPE_PLUGIN_ROW,
                       "plugin-context", context,
                       "plugin-info",    info,
                       NULL);
}

