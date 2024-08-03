// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-plugin-row"

#include "config.h"

#include <adwaita.h>
#include <valent.h>

#include "valent-plugin-row.h"


/**
 * ValentPluginRow:
 *
 * A [class@Gtk.ListBoxRow] used to toggle plugins.
 */

struct _ValentPluginRow
{
  AdwExpanderRow  parent;

  ValentContext  *context;
  PeasPluginInfo *plugin_info;
  GSettings      *settings;

  /* template */
  GtkSwitch      *plugin_enabled;
};

G_DEFINE_FINAL_TYPE (ValentPluginRow, valent_plugin_row, ADW_TYPE_EXPANDER_ROW)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static char *
strv_to_str (GObject            *object,
             const char * const *strv)
{
  return g_strjoinv (", ", (GStrv)strv);
}

static void
valent_plugin_row_set_context (ValentPluginRow *self,
                               ValentContext   *context)
{
  g_assert (VALENT_IS_PLUGIN_ROW (self));
  g_assert (context == NULL || VALENT_IS_CONTEXT (context));

  if (!g_set_object (&self->context, context))
    return;

  self->settings = valent_context_create_settings (self->context,
                                                   "ca.andyholmes.Valent.Plugin");
  g_settings_bind (self->settings,       "enabled",
                   self->plugin_enabled, "active",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_switch_set_active (self->plugin_enabled,
                         g_settings_get_boolean (self->settings, "enabled"));
}

/*
 * GObject
 */
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

  g_clear_object (&self->context);
  g_clear_object (&self->plugin_info);
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
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
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

  switch (prop_id)
    {
    case PROP_CONTEXT:
      valent_plugin_row_set_context (self, g_value_get_object (value));
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

  object_class->dispose = valent_plugin_row_dispose;
  object_class->finalize = valent_plugin_row_finalize;
  object_class->get_property = valent_plugin_row_get_property;
  object_class->set_property = valent_plugin_row_set_property;

  /**
   * ValentPluginRow:context:
   *
   * The [class@Valent.Context] for the [class@Valent.Extension].
   */
  properties [PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         VALENT_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentPluginRow:plugin-info:
   *
   * The [class@Peas.PluginInfo] describing the plugin.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-plugin-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentPluginRow, plugin_enabled);
  gtk_widget_class_bind_template_callback (widget_class, strv_to_str);
}

static void
valent_plugin_row_init (ValentPluginRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
