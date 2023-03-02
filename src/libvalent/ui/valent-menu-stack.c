// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-menu-stack"

#include "config.h"

#include <gtk/gtk.h>

#include "valent-menu-list.h"
#include "valent-menu-stack.h"


struct _ValentMenuStack
{
  GtkWidget       parent_instance;

  GtkWidget      *stack;
  ValentMenuList *main;
};

G_DEFINE_FINAL_TYPE (ValentMenuStack, valent_menu_stack, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_MENU_MODEL,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GObject
 */
static void
valent_menu_stack_dispose (GObject *object)
{
  ValentMenuStack *self = VALENT_MENU_STACK (object);

  g_clear_pointer (&self->stack, gtk_widget_unparent);

  G_OBJECT_CLASS (valent_menu_stack_parent_class)->dispose (object);
}

static void
valent_menu_stack_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentMenuStack *self = VALENT_MENU_STACK (object);

  switch (prop_id)
    {
    case PROP_MENU_MODEL:
      g_value_set_object (value, valent_menu_stack_get_menu_model (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_menu_stack_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ValentMenuStack *self = VALENT_MENU_STACK (object);

  switch (prop_id)
    {
    case PROP_MENU_MODEL:
      valent_menu_stack_set_menu_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_menu_stack_class_init (ValentMenuStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_menu_stack_dispose;
  object_class->get_property = valent_menu_stack_get_property;
  object_class->set_property = valent_menu_stack_set_property;

  gtk_widget_class_set_css_name (widget_class, "valent-menu-stack");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  /**
   * ValentMenuStack:model:
   *
   * The #GMenuModel for this #ValentMenuStack.
   */
  properties [PROP_MENU_MODEL] =
    g_param_spec_object ("menu-model", NULL, NULL,
                         G_TYPE_MENU_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_menu_stack_init (ValentMenuStack *self)
{
  self->stack = g_object_new (GTK_TYPE_STACK,
                              "interpolate-size", TRUE,
                              "transition-type",  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT,
                              "vhomogeneous",     FALSE,
                              NULL);
  gtk_widget_insert_before (self->stack, GTK_WIDGET (self), NULL);
}

/**
 * valent_menu_stack_new:
 * @menu_model: (nullable): a #GMenuModel
 *
 * Create a new #ValentMenuStack.
 *
 * Returns: a #GtkWidget
 */
GtkWidget *
valent_menu_stack_new (GMenuModel *menu_model)
{
  return g_object_new (VALENT_TYPE_MENU_STACK,
                       "menu-model", menu_model,
                       NULL);
}

/**
 * valent_menu_stack_get_model:
 * @stack: a #ValentMenuStack
 *
 * Get the #GMenuModel for @stack.
 *
 * Returns: (transfer none) (nullable): a #GMenuModel
 */
GMenuModel *
valent_menu_stack_get_menu_model (ValentMenuStack *stack)
{
  g_return_val_if_fail (VALENT_IS_MENU_STACK (stack), NULL);

  if (stack->main == NULL)
    return NULL;

  return valent_menu_list_get_menu_model (stack->main);
}

/**
 * valent_menu_stack_set_model:
 * @stack: a #ValentMenuStack
 * @menu_model: (nullable): a #GMenuModel
 *
 * Set the #GMenuModel for @stack to @menu_model.
 */
void
valent_menu_stack_set_menu_model (ValentMenuStack *stack,
                                  GMenuModel      *menu_model)
{
  g_return_if_fail (VALENT_IS_MENU_STACK (stack));
  g_return_if_fail (menu_model == NULL || G_IS_MENU_MODEL (menu_model));

  /* Destroy any existing menu lists */
  if (stack->main != NULL)
    {
      gtk_stack_remove (GTK_STACK (stack->stack), GTK_WIDGET (stack->main));
      stack->main = NULL;
    }

  if (menu_model != NULL)
    {
      stack->main = valent_menu_list_new (NULL);
      gtk_stack_add_named (GTK_STACK (stack->stack),
                           GTK_WIDGET (stack->main),
                           "main");
      valent_menu_list_set_menu_model (stack->main, menu_model);
    }
}

