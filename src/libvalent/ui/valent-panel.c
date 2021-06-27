// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-panel"

#include "config.h"

#include <gtk/gtk.h>
#include <adwaita.h>

#include "valent-panel.h"


/**
 * SECTION:valentpanel
 * @short_description: A preferences panel base
 * @title: ValentPanel
 * @stability: Unstable
 *
 * #ValentPanel is a convenience widget for scrollable panels. Widgets can be
 * added to the scrollable area with valent_panel_append() and
 * valent_panel_prepend(). Header and footer widgets can be set with
 * valent_panel_set_header() and valent_panel_set_footer() that will always
 * stay in view.
 */

typedef struct
{
  GtkWidget *header;
  GtkWidget *scroll;
  GtkBox    *body;
  GtkWidget *footer;

  char      *title;
  char      *icon_name;
} ValentPanelPrivate;

static void gtk_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentPanel, valent_panel, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (ValentPanel)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_buildable_iface_init))

/**
 * ValentPanelClass:
 *
 * The virtual function table for #ValentPanelClass.
 */

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_TITLE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GtkBuildable
 */
static GtkBuildableIface *parent_buildable_iface;

static void
valent_panel_buildable_add_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  GObject      *child,
                                  const char   *type)
{
  ValentPanel *self = VALENT_PANEL (buildable);
  ValentPanelPrivate *priv = valent_panel_get_instance_private (self);

  if (type != NULL)
    {
      if (g_strcmp0 ("header", type) == 0)
        valent_panel_set_header (self, GTK_WIDGET (child));
      else if (g_strcmp0 ("footer", type) == 0)
        valent_panel_set_footer (self, GTK_WIDGET (child));
      else
        GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
    }
  else if (priv->body && GTK_IS_WIDGET (child))
    {
      gtk_box_append (priv->body, GTK_WIDGET (child));
    }
  else
    {
      parent_buildable_iface->add_child (buildable, builder, child, type);
    }
}

static void
gtk_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = valent_panel_buildable_add_child;
}


/*
 * GObject
 */
static void
valent_panel_dispose (GObject *object)
{
  ValentPanel *self = VALENT_PANEL (object);
  ValentPanelPrivate *priv = valent_panel_get_instance_private (self);

  g_clear_pointer (&priv->header, gtk_widget_unparent);
  g_clear_pointer (&priv->scroll, gtk_widget_unparent);
  g_clear_pointer (&priv->footer, gtk_widget_unparent);

  G_OBJECT_CLASS (valent_panel_parent_class)->dispose (object);
}

static void
valent_panel_finalize (GObject *object)
{
  ValentPanel *self = VALENT_PANEL (object);
  ValentPanelPrivate *priv = valent_panel_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->icon_name, g_free);

  G_OBJECT_CLASS (valent_panel_parent_class)->finalize (object);
}

static void
valent_panel_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ValentPanel *self = VALENT_PANEL (object);
  ValentPanelPrivate *priv = valent_panel_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, priv->icon_name);
      break;

    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_panel_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ValentPanel *self = VALENT_PANEL (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      valent_panel_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      valent_panel_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_panel_class_init (ValentPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_panel_dispose;
  object_class->finalize = valent_panel_finalize;
  object_class->get_property = valent_panel_get_property;
  object_class->set_property = valent_panel_set_property;

  /* Layout */
  gtk_widget_class_set_css_name (widget_class, "preferencespage");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-panel.ui");
  gtk_widget_class_bind_template_child_private (widget_class, ValentPanel, scroll);
  gtk_widget_class_bind_template_child_private (widget_class, ValentPanel, body);

  /**
   * ValentPanel:icon-name:
   *
   * The themed icon name of the panel.
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Themed icon name of the panel",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentPanel:title:
   *
   * The title text for the panel.
   */
  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title of the panel",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_panel_init (ValentPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * valent_panel_new:
 *
 * Create a new #ValentPanel.
 *
 * Returns: a new #ValentPanel
 */
GtkWidget *
valent_panel_new (void)
{
  return g_object_new (VALENT_TYPE_PANEL, NULL);
}

/**
 * valent_panel_append:
 * @panel: a #ValentPanel
 * @child: a #GtkWidget
 *
 * Append @child to the scrollable area of @panel.
 */
void
valent_panel_append (ValentPanel *panel,
                     GtkWidget   *child)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_if_fail (VALENT_IS_PANEL (panel));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_box_append (priv->body, child);
}

/**
 * valent_panel_prepend:
 * @panel: a #ValentPanel
 * @child: a #GtkWidget
 *
 * Prepend @child to the scrollable area of @panel.
 */
void
valent_panel_prepend (ValentPanel *panel,
                      GtkWidget   *child)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_if_fail (VALENT_IS_PANEL (panel));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_box_prepend (priv->body, child);
}

/**
 * valent_panel_get_title:
 * @panel: a #ValentPanel
 *
 * Get the title text for @panel.
 *
 * Returns: (nullable) (transfer none): the panel title
 */
const char *
valent_panel_get_title (ValentPanel *panel)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_val_if_fail (VALENT_IS_PANEL (panel), NULL);

  return priv->title;
}

/**
 * valent_panel_set_title:
 * @panel: a #ValentPanel
 * @title: (nullable): a title
 *
 * Set the title text for @panel to @title.
 */
void
valent_panel_set_title (ValentPanel *panel,
                        const char  *title)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_if_fail (VALENT_IS_PANEL (panel));

  if (g_strcmp0 (priv->title, title) == 0)
    return;

  g_clear_pointer (&priv->title, g_free);
  priv->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (panel), properties [PROP_TITLE]);
}

/**
 * valent_panel_get_icon_name:
 * @panel: a #ValentPanel
 *
 * Get the themed icon name for @panel.
 *
 * Returns: (nullable) (transfer none): a themed icon name
 */
const char *
valent_panel_get_icon_name (ValentPanel *panel)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_val_if_fail (VALENT_IS_PANEL (panel), NULL);

  return priv->icon_name;
}

/**
 * valent_panel_set_icon_name:
 * @panel: a #ValentPanel
 * @icon_name: (nullable): a themed icon name
 *
 * Set the #GIcon for @panel to @icon.
 */
void
valent_panel_set_icon_name (ValentPanel *panel,
                            const char  *icon_name)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_if_fail (VALENT_IS_PANEL (panel));

  if (g_strcmp0 (priv->icon_name, icon_name) == 0)
    return;

  g_clear_pointer (&priv->icon_name, g_free);
  priv->icon_name = g_strdup (icon_name);
  g_object_notify_by_pspec (G_OBJECT (panel), properties [PROP_ICON_NAME]);
}

/**
 * valent_panel_get_header:
 * @panel: a #ValentPanel
 *
 * Get the header widget for @panel.
 *
 * Returns: (nullable) (transfer none): a #GtkWidget
 */
GtkWidget *
valent_panel_get_header (ValentPanel *panel)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_val_if_fail (VALENT_IS_PANEL (panel), NULL);

  return priv->header;
}

/**
 * valent_panel_set_header:
 * @panel: a #ValentPanel
 * @child: (nullable): a #GtkWidget
 *
 * Set the header widget for @panel to @child, which is placed above the
 * scrollable area. This is useful for widgets that are not intended to scroll
 * out of view.
 */
void
valent_panel_set_header (ValentPanel *panel,
                         GtkWidget   *child)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_if_fail (VALENT_IS_PANEL (panel));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  g_clear_pointer (&priv->header, gtk_widget_unparent);

  if (GTK_IS_WIDGET (child))
    {
      priv->header = child;
      gtk_widget_insert_before (child, GTK_WIDGET (panel), priv->scroll);
    }
}

/**
 * valent_panel_get_footer:
 * @panel: a #ValentPanel
 *
 * Get the footer widget for @panel.
 *
 * Returns: (nullable) (transfer none): a #GtkWidget
 */
GtkWidget *
valent_panel_get_footer (ValentPanel *panel)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_val_if_fail (VALENT_IS_PANEL (panel), NULL);

  return priv->footer;
}

/**
 * valent_panel_set_footer:
 * @panel: a #ValentPanel
 * @child: (nullable): a #GtkWidget
 *
 * Set the footer widget for @panel to @child, which is placed below the
 * scrollable area. This is useful for widgets like #GtkActionBar that are
 * not intended to scroll out of view.
 */
void
valent_panel_set_footer (ValentPanel *panel,
                         GtkWidget   *child)
{
  ValentPanelPrivate *priv = valent_panel_get_instance_private (panel);

  g_return_if_fail (VALENT_IS_PANEL (panel));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  g_clear_pointer (&priv->footer, gtk_widget_unparent);

  if (GTK_IS_WIDGET (child))
    {
      priv->footer = child;
      gtk_widget_insert_after (child, GTK_WIDGET (panel), priv->scroll);
    }
}

