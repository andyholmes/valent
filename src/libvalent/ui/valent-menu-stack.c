// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

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

G_DEFINE_TYPE (ValentMenuStack, valent_menu_stack, GTK_TYPE_WIDGET)


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
valent_menu_stack_class_init (ValentMenuStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_menu_stack_dispose;

  gtk_widget_class_set_css_name (widget_class, "valent-menu-stack");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
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

ValentMenuStack *
valent_menu_stack_new (void)
{
  return g_object_new (VALENT_TYPE_MENU_STACK, NULL);
}

/**
 * valent_menu_stack_bind_model:
 * @self: a #ValentMenuStack
 * @model: the model to bind
 *
 * Binds @self to @model, to populate the stack of menus.
 */
void
valent_menu_stack_bind_model (ValentMenuStack *self,
                            GMenuModel    *model)
{
  g_return_if_fail (VALENT_IS_MENU_STACK (self));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));

  /* Destroy any existing menu lists */
  if (self->main != NULL)
    {
      gtk_stack_remove (GTK_STACK (self->stack), GTK_WIDGET (self->main));
      self->main = NULL;
    }

  if (model != NULL)
    {
      self->main = valent_menu_list_new (NULL);
      gtk_stack_add_named (GTK_STACK (self->stack),
                           GTK_WIDGET (self->main),
                           "main");
      valent_menu_list_set_model (self->main, model);
    }
}

