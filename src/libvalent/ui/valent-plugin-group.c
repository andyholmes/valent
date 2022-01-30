// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-plugin-group"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-plugin-group.h"
#include "valent-plugin-preferences.h"
#include "valent-plugin-row.h"


struct _ValentPluginGroup
{
  AdwPreferencesGroup  parent_instance;

  PeasEngine          *engine;
  char                *plugin_context;
  GType                plugin_type;

  GtkListBox          *plugin_list;
  GHashTable          *plugin_rows;
};

G_DEFINE_TYPE (ValentPluginGroup, valent_plugin_group, ADW_TYPE_PREFERENCES_GROUP)


enum {
  PROP_0,
  PROP_PLUGIN_CONTEXT,
  PROP_PLUGIN_TYPE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * PeasEngine Callbacks
 */
static void
on_load_plugin (PeasEngine        *engine,
                PeasPluginInfo    *info,
                ValentPluginGroup *self)
{
  GtkWidget *row;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_PLUGIN_GROUP (self));

  if (!peas_engine_provides_extension (engine, info, self->plugin_type))
    return;

  row = g_object_new (VALENT_TYPE_PLUGIN_ROW,
                      "plugin-context", self->plugin_context,
                      "plugin-info",    info,
                      "plugin-type",    self->plugin_type,
                      NULL);

  gtk_list_box_insert (self->plugin_list, row, -1);
  g_hash_table_insert (self->plugin_rows, info, row);
}

static void
on_unload_plugin (PeasEngine        *engine,
                  PeasPluginInfo    *info,
                  ValentPluginGroup *self)
{
  gpointer row;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_PLUGIN_GROUP (self));

  if (!peas_engine_provides_extension (engine, info, self->plugin_type))
    return;

  if (g_hash_table_steal_extended (self->plugin_rows, info, NULL, &row))
    gtk_list_box_remove (self->plugin_list, row);
}


/*
 * GObject
 */
static void
valent_plugin_group_constructed (GObject *object)
{
  ValentPluginGroup *self = VALENT_PLUGIN_GROUP (object);
  const GList *plugins = NULL;

  plugins = peas_engine_get_plugin_list (self->engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    on_load_plugin (self->engine, iter->data, self);

  g_signal_connect_after (self->engine,
                          "load-plugin",
                          G_CALLBACK (on_load_plugin),
                          self);
  g_signal_connect (self->engine,
                    "unload-plugin",
                    G_CALLBACK (on_unload_plugin),
                    self);

  G_OBJECT_CLASS (valent_plugin_group_parent_class)->constructed (object);
}

static void
valent_plugin_group_dispose (GObject *object)
{
  ValentPluginGroup *self = VALENT_PLUGIN_GROUP (object);

  g_signal_handlers_disconnect_by_func (self->engine, on_load_plugin, self);
  g_signal_handlers_disconnect_by_func (self->engine, on_unload_plugin, self);

  G_OBJECT_CLASS (valent_plugin_group_parent_class)->dispose (object);
}

static void
valent_plugin_group_finalize (GObject *object)
{
  ValentPluginGroup *self = VALENT_PLUGIN_GROUP (object);

  g_clear_pointer (&self->plugin_context, g_free);
  g_clear_pointer (&self->plugin_rows, g_hash_table_unref);

  G_OBJECT_CLASS (valent_plugin_group_parent_class)->finalize (object);
}

static void
valent_plugin_group_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentPluginGroup *self = VALENT_PLUGIN_GROUP (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      g_value_set_string (value, self->plugin_context);
      break;

    case PROP_PLUGIN_TYPE:
      g_value_set_gtype (value, self->plugin_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_plugin_group_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ValentPluginGroup *self = VALENT_PLUGIN_GROUP (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      self->plugin_context = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_TYPE:
      self->plugin_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_plugin_group_class_init (ValentPluginGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_plugin_group_constructed;
  object_class->dispose = valent_plugin_group_dispose;
  object_class->finalize = valent_plugin_group_finalize;
  object_class->get_property = valent_plugin_group_get_property;
  object_class->set_property = valent_plugin_group_set_property;

  properties [PROP_PLUGIN_CONTEXT] =
    g_param_spec_string ("plugin-context",
                         "Plugin Context",
                         "A context for the plugin type displayed by this group",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PLUGIN_TYPE] =
    g_param_spec_gtype ("plugin-type",
                        "Plugin Type",
                        "The GType of the plugins for this group",
                        G_TYPE_NONE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_plugin_group_init (ValentPluginGroup *self)
{
  GtkWidget *frame;
  GtkWidget *placeholder;

  self->engine = valent_get_engine ();
  self->plugin_rows = g_hash_table_new (NULL, NULL);

  /* Placeholder */
  frame = gtk_frame_new (NULL);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (self), frame);

  self->plugin_list = g_object_new (GTK_TYPE_LIST_BOX,
                                    "hexpand",         TRUE,
                                    "selection-mode",  GTK_SELECTION_NONE,
                                    "show-separators", TRUE,
                                    NULL);
  gtk_frame_set_child (GTK_FRAME (frame), GTK_WIDGET (self->plugin_list));

  gtk_list_box_set_sort_func (self->plugin_list,
                              valent_plugin_preferences_row_sort,
                              NULL, NULL);

  placeholder = g_object_new (GTK_TYPE_LABEL,
                              "label",          _("No Plugins"),
                              "height-request", 56,
                              NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (placeholder),
                               "dim-label");
  gtk_list_box_set_placeholder (self->plugin_list, placeholder);
}

