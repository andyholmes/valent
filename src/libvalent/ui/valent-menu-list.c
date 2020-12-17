// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-menu-list"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <pango/pango.h>

#include "valent-menu-list.h"


struct _ValentMenuList
{
  GtkWidget       parent_instance;

  GtkListBox     *list;
  GMenuModel     *model;
  ValentMenuList *parent;
  gulong          items_changed_id;
};

G_DEFINE_TYPE (ValentMenuList, valent_menu_list, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_MODEL,
  PROP_SUBMENU_OF,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
valent_menu_list_add_row (ValentMenuList *self,
                          gint            index)
{
  GtkWidget *row;
  GtkGrid *grid;
  GtkWidget *row_icon;
  GtkWidget *row_label;
  g_autofree char *hidden_when = NULL;
  g_autofree char *label = NULL;
  g_autofree char *action_name = NULL;
  g_autoptr (GVariant) action_target = NULL;
  g_autoptr (GVariant) vicon = NULL;
  g_autoptr (GIcon) gicon = NULL;

  /* Row Label */
  if (!g_menu_model_get_item_attribute (self->model, index,
                                        "label", "s", &label))
    return;

  /* GAction */
  if (g_menu_model_get_item_attribute (self->model, index,
                                       "action", "s", &action_name))
    {
      action_target = g_menu_model_get_item_attribute_value (self->model, index,
                                                             "target", NULL);
    }

  /* Icon */
  vicon = g_menu_model_get_item_attribute_value (self->model, index,
                                                 "icon",  NULL);

  if (vicon != NULL)
    gicon = g_icon_deserialize (vicon);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "action-name",    action_name,
                      "action-target",  action_target,
                      "height_request", 56,
                      "selectable",     FALSE,
                      NULL);

  grid = g_object_new (GTK_TYPE_GRID,
                       "column-spacing", 12,
                       "margin-start",   20,
                       "margin-end",     20,
                       "margin-bottom",   8,
                       "margin-top",      8,
                       NULL);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), GTK_WIDGET (grid));

  row_icon = g_object_new (GTK_TYPE_IMAGE,
                           "gicon",     gicon,
                           "icon-size", GTK_ICON_SIZE_NORMAL,
                           NULL);
  gtk_grid_attach (grid, row_icon, 0, 0, 1, 1);

  row_label = g_object_new (GTK_TYPE_LABEL,
                            "label",   label,
                            "halign",  GTK_ALIGN_START,
                            "hexpand", TRUE,
                            "valign",  GTK_ALIGN_CENTER,
                            "vexpand", TRUE,
                            NULL);
  gtk_grid_attach (grid, row_label, 1, 0, 1, 1);

  /* Emulate hidden-when/action-disabled */
  if (g_menu_model_get_item_attribute (self->model, index,
                                       "hidden-when", "s", &hidden_when))
    {
      if (g_strcmp0 (hidden_when, "action-disabled") == 0)
        g_object_bind_property (G_OBJECT (row), "sensitive",
                                G_OBJECT (row), "visible",
                                G_BINDING_SYNC_CREATE);
    }

  /* Account for "Go Previous" item */
  if (self->parent != NULL)
    index += 1;

  gtk_list_box_insert (self->list, GTK_WIDGET (row), index);
}

static void
valent_menu_list_add_section (ValentMenuList *self,
                              gint            index,
                              GMenuModel     *model)
{
  GtkListBoxRow *row;
  GtkWidget *grid;
  GtkWidget *arrow;

  g_assert (VALENT_IS_MENU_LIST (self));
  g_assert (G_IS_MENU_MODEL (model));

  g_debug ("[%s] IS SECTION", G_STRFUNC);

  row = gtk_list_box_get_row_at_index (self->list, index);
  grid = gtk_list_box_row_get_child (row);

  arrow = gtk_image_new_from_icon_name ("go-next-symbolic");
  gtk_style_context_add_class (gtk_widget_get_style_context (arrow), "dim-label");
  gtk_grid_attach (GTK_GRID (grid), arrow, 0, 2, 1, 1);
}

static void
valent_menu_list_add_submenu (ValentMenuList *self,
                              gint            index,
                              GMenuModel     *model)
{
  GtkWidget *stack;
  ValentMenuList *submenu;
  GtkListBoxRow *row;
  GtkWidget *grid;
  GtkWidget *arrow;
  g_autofree char *label = NULL;
  g_autofree char *action_detail = NULL;

  g_assert (VALENT_IS_MENU_LIST (self));
  g_assert (G_IS_MENU_MODEL (model));

  g_debug ("[%s] IS SUBMENU", G_STRFUNC);

  /* Amend the row */
  row = gtk_list_box_get_row_at_index (self->list, index);
  g_menu_model_get_item_attribute (self->model, index, "label", "s", &label);

  action_detail = g_strdup_printf ("menu.submenu::%s", label);
  gtk_actionable_set_detailed_action_name (GTK_ACTIONABLE (row), action_detail);

  grid = gtk_list_box_row_get_child (row);
  arrow = gtk_image_new_from_icon_name ("go-next-symbolic");
  gtk_style_context_add_class (gtk_widget_get_style_context (arrow), "dim-label");
  gtk_grid_attach (GTK_GRID (grid), arrow, 2, 0, 1, 1);

  /* Add the submenu */
  submenu = g_object_new (VALENT_TYPE_MENU_LIST,
                          "model",      model,
                          "submenu-of", self,
                          NULL);
  stack = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_STACK);
  gtk_stack_add_titled (GTK_STACK (stack), GTK_WIDGET (submenu), label, label);
}

static void
valent_menu_list_add (ValentMenuList *self,
                      gint            index)
{
  g_autoptr (GMenuLinkIter) iter = NULL;
  const char *link_name;
  GMenuModel *link_value;

  valent_menu_list_add_row (self, index);

  //
  iter = g_menu_model_iterate_item_links (self->model, index);

  while (g_menu_link_iter_get_next (iter, &link_name, &link_value))
    {
      if (g_strcmp0 (link_name, "section") == 0)
        valent_menu_list_add_section (self, index, link_value);

      else if (g_strcmp0 (link_name, "submenu") == 0)
        valent_menu_list_add_submenu (self, index, link_value);

      g_object_unref (link_value);
    }
}

static void
valent_menu_list_remove (ValentMenuList *self,
                         gint            index)
{
  GtkListBoxRow *row;

  g_assert (VALENT_IS_MENU_LIST (self));

  if ((row = gtk_list_box_get_row_at_index (self->list, index)))
    gtk_list_box_remove (self->list, GTK_WIDGET (row));
}

static void
on_items_changed (GMenuModel *model,
                  gint        position,
                  gint        removed,
                  gint        added,
                  gpointer    user_data)
{
  ValentMenuList *self = user_data;
  gint i, index;

  g_assert (G_IS_MENU_MODEL (model));
  g_assert (VALENT_IS_MENU_LIST (self));

  // Remove items
  while (removed > 0)
    {
      valent_menu_list_remove (self, position);
      removed--;
    }

  // Add items
  for (i = 0; i < added; i++)
    {
      index = i + position;
      valent_menu_list_add (self, index);
    }
}

/*
 * GActions
 */
static void
submenu_action (GtkWidget  *widget,
                const char *name,
                GVariant   *parameter)
{
  GtkWidget *stack;
  const char *child_name;

  stack = gtk_widget_get_ancestor (widget, GTK_TYPE_STACK);
  child_name = g_variant_get_string (parameter, NULL);
  gtk_stack_set_visible_child_name (GTK_STACK (stack), child_name);
}

/*
 * GObject
 */
static void
valent_menu_list_dispose (GObject *object)
{
  ValentMenuList *self = VALENT_MENU_LIST (object);

  g_clear_signal_handler (&self->items_changed_id, self->model);

  if (GTK_IS_WIDGET (self->list))
    gtk_widget_unparent (GTK_WIDGET (self->list));

  G_OBJECT_CLASS (valent_menu_list_parent_class)->dispose (object);
}

static void
valent_menu_list_finalize (GObject *object)
{
  ValentMenuList *self = VALENT_MENU_LIST (object);

  g_clear_object (&self->model);
  g_clear_object (&self->parent);

  G_OBJECT_CLASS (valent_menu_list_parent_class)->finalize (object);
}

static void
valent_menu_list_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ValentMenuList *self = VALENT_MENU_LIST (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_SUBMENU_OF:
      g_value_set_object (value, self->parent);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_menu_list_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ValentMenuList *self = VALENT_MENU_LIST (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      valent_menu_list_set_model (self, g_value_get_object (value));
      break;

    case PROP_SUBMENU_OF:
      valent_menu_list_set_submenu_of (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_menu_list_class_init (ValentMenuListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_menu_list_dispose;
  object_class->finalize = valent_menu_list_finalize;
  object_class->get_property = valent_menu_list_get_property;
  object_class->set_property = valent_menu_list_set_property;

  gtk_widget_class_install_action (widget_class, "menu.submenu", "s", submenu_action);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_GRID_LAYOUT);

  /**
   * ValentMenuList:model
   *
   * The "model" property holds the #GMenuModel used to build this list.
   */
  properties [PROP_MODEL] =
    g_param_spec_object ("model",
                         "Menu Model",
                         "The GMenuModel for this section",
                         G_TYPE_MENU_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMenuList:submenu-of
   *
   * The parent #ValentMenuList this is a submenu for.
   */
  properties [PROP_SUBMENU_OF] =
    g_param_spec_object ("submenu-of",
                         "Submenu Of",
                         "The parent menu list",
                         VALENT_TYPE_MENU_LIST,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_menu_list_init (ValentMenuList *self)
{
  GtkWidget *placeholder;

  /* Create the GtkListBox */
  self->list = g_object_new (GTK_TYPE_LIST_BOX,
                             "show-separators", TRUE,
                             "hexpand",         TRUE,
                             NULL);
  gtk_widget_set_parent (GTK_WIDGET (self->list), GTK_WIDGET (self));
  gtk_widget_insert_after (GTK_WIDGET (self->list),
                           GTK_WIDGET (self),
                           NULL);

  /* Placeholder */
  placeholder = g_object_new (GTK_TYPE_LABEL,
                              "label", _("No Actions"),
                              "margin-top", 18,
                              "margin-bottom", 18,
                              NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (placeholder),
                               "dim-label");
  gtk_list_box_set_placeholder (self->list, placeholder);
}

/**
 * valent_menu_list_new:
 * @model: (nullable): a #GMenuModel
 *
 * Create a new #ValentMenuList.
 *
 * Returns: (transfer full): a #ValentMenuList
 */
ValentMenuList *
valent_menu_list_new (GMenuModel *model)
{
  return g_object_new (VALENT_TYPE_MENU_LIST,
                       "model", model,
                       NULL);
}

/**
 * valent_menu_list_get_model:
 * @self: a #ValentMenuList
 *
 * Get the #GMenuModel for @self.
 *
 * Returns: (transfer none): a #GMenuModel
 */
GMenuModel *
valent_menu_list_get_model (ValentMenuList *self)
{
  g_assert (VALENT_IS_MENU_LIST (self));

  return self->model;
}

/**
 * valent_menu_list_set_model:
 * @self: a #ValentMenuList
 * @model: (nullable): a #GMenuModel
 *
 * Set the #GMenuModel for @self.
 */
void
valent_menu_list_set_model (ValentMenuList *list,
                            GMenuModel     *model)
{
  guint n_items;

  g_return_if_fail (VALENT_IS_MENU_LIST (list));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));

  if (list->model != NULL)
    g_clear_signal_handler (&list->items_changed_id, list->model);

  /* Hold a reference to @model */
  g_set_object (&list->model, model);

  if (list->model != NULL)
    {
      n_items = g_menu_model_get_n_items (model);
      on_items_changed (model, 0, 0, n_items, list);

      list->items_changed_id = g_signal_connect (list->model,
                                                 "items-changed",
                                                 G_CALLBACK (on_items_changed),
                                                 list);
    }
}

/**
 * valent_menu_list_get_submenu_of:
 * @self: a #ValentMenuList
 *
 * Get the parent #ValentMenuList.
 *
 * Returns: (transfer none) (nullable): a #ValentMenuList
 */
ValentMenuList *
valent_menu_list_get_submenu_of (ValentMenuList *self)
{
  g_assert (VALENT_IS_MENU_LIST (self));

  return self->parent;
}

/**
 * valent_menu_list_set_submenu_of:
 * @self: a #ValentMenuList
 * @parent: (nullable): a #GMenuModel
 *
 * Set the #GMenuModel for @self.
 */
void
valent_menu_list_set_submenu_of (ValentMenuList *self,
                                 ValentMenuList *parent)
{
  GtkWidget *row;
  GtkWidget *grid;
  GtkWidget *icon;
  GtkWidget *label;

  g_assert (VALENT_IS_MENU_LIST (self));
  // g_assert (VALENT_IS_MENU_LIST (parent));

  if (!VALENT_IS_MENU_LIST (parent))
    return;

  self->parent = g_object_ref (parent);

  /* stack = gtk_widget_get_ancestor (GTK_WIDGET (parent), GTK_TYPE_STACK); */
  /* page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (parent)); */
  /* title = gtk_stack_page_get_title (page); */

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "action-name",    "menu.submenu",
                      "action-target",  g_variant_new_string ("main"),
                      "height_request", 56,
                      "selectable",     FALSE,
                      NULL);

  grid = g_object_new (GTK_TYPE_GRID,
                       "column-spacing", 12,
                       "margin-start",   20,
                       "margin-end",     20,
                       "margin-bottom",   8,
                       "margin-top",      8,
                       NULL);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), GTK_WIDGET (grid));

  icon = g_object_new (GTK_TYPE_IMAGE,
                       "icon-name", "go-previous-symbolic",
                       "icon-size", GTK_ICON_SIZE_NORMAL,
                       NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (icon), "dim-label");
  gtk_grid_attach (GTK_GRID (grid), icon, 0, 0, 1, 1);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label",   _("Previous"),
                        "halign",  GTK_ALIGN_CENTER,
                        "hexpand", TRUE,
                        "valign",  GTK_ALIGN_CENTER,
                        "vexpand", TRUE,
                        NULL);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  gtk_list_box_insert (self->list, GTK_WIDGET (row), 0);
}

