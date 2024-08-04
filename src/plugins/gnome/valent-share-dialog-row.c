// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-dialog-row"

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-share-dialog-row.h"


struct _ValentShareDialogRow
{
  GtkListBoxRow     parent_instance;

  ValentDevice     *device;
  AdwAnimation     *animation;
  unsigned int      selected : 1;
  unsigned int      selection_mode : 1;

  /* template */
  GtkRevealer      *revealer;
  GtkCheckButton   *selected_button;
  GtkImage         *icon_image;
  GtkLabel         *name_label;
  GtkImage         *next_image;
};

G_DEFINE_FINAL_TYPE (ValentShareDialogRow, valent_share_dialog_row, GTK_TYPE_LIST_BOX_ROW)


enum {
  PROP_0,
  PROP_DEVICE,
  PROP_SELECTED,
  PROP_SELECTION_MODE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
on_selection_enable (ValentShareDialogRow *self)
{
  GtkRoot *root = NULL;

  if (self->selection_mode)
    return;

  if ((root = gtk_widget_get_root (GTK_WIDGET (self))) == NULL)
    return;

  g_object_set (root, "selection-mode", TRUE, NULL);
  valent_share_dialog_row_set_selected (self, TRUE);
}

static void
on_selection_disable (ValentShareDialogRow *self)
{
  if (!gtk_revealer_get_child_revealed (self->revealer))
    valent_share_dialog_row_set_selected (self, FALSE);
}

/*
 * ValentShareDialogRow
 */
static void
valent_share_dialog_row_set_device (ValentShareDialogRow *self,
                                    ValentDevice         *device)
{
  g_assert (VALENT_IS_SHARE_DIALOG_ROW (self));
  g_assert (device == NULL || VALENT_IS_DEVICE (device));

  if (!g_set_object (&self->device, device))
    return;

  g_object_bind_property (device,           "icon-name",
                          self->icon_image, "icon-name",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (device,           "name",
                          self->name_label, "label",
                          G_BINDING_SYNC_CREATE);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEVICE]);
}

/*
 * GObject
 */
static void
valent_share_dialog_row_dispose (GObject *object)
{
  ValentShareDialogRow *self = VALENT_SHARE_DIALOG_ROW (object);

  if (self->animation != NULL)
    {
      adw_animation_skip (self->animation);
      g_clear_object (&self->animation);
    }

  g_clear_object (&self->device);
  gtk_widget_dispose_template (GTK_WIDGET (self), VALENT_TYPE_SHARE_DIALOG_ROW);

  G_OBJECT_CLASS (valent_share_dialog_row_parent_class)->dispose (object);
}

static void
valent_share_dialog_row_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentShareDialogRow *self = VALENT_SHARE_DIALOG_ROW (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    case PROP_SELECTED:
      g_value_set_boolean (value, self->selected);
      break;

    case PROP_SELECTION_MODE:
      g_value_set_boolean (value, self->selection_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_dialog_row_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentShareDialogRow *self = VALENT_SHARE_DIALOG_ROW (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      valent_share_dialog_row_set_device (self, g_value_get_object (value));
      break;

    case PROP_SELECTED:
      valent_share_dialog_row_set_selected (self, g_value_get_boolean (value));
      break;

    case PROP_SELECTION_MODE:
      valent_share_dialog_row_set_selection_mode (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_dialog_row_class_init (ValentShareDialogRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_share_dialog_row_dispose;
  object_class->get_property = valent_share_dialog_row_get_property;
  object_class->set_property = valent_share_dialog_row_set_property;

  /**
   * ValentShareDialogRow:device: (getter get_device)
   *
   * The [class@Valent.Device] this row displays.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentShareDialogRow:selected: (getter get_selected) (setter set_selected)
   *
   * Whether the row is selected.
   */
  properties [PROP_SELECTED] =
    g_param_spec_boolean ("selected", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentShareDialogRow:selection-mode: (getter get_selection_mode) (setter set_selection_mode)
   *
   * Whether the row is in selection mode.
   */
  properties [PROP_SELECTION_MODE] =
    g_param_spec_boolean ("selection-mode", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-share-dialog-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialogRow, revealer);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialogRow, selected_button);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialogRow, icon_image);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialogRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialogRow, next_image);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_enable);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_disable);
}

static void
valent_share_dialog_row_init (ValentShareDialogRow *self)
{
  AdwAnimationTarget *target = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  target = adw_property_animation_target_new (G_OBJECT (self->next_image),
                                              "opacity");
  self->animation = adw_timed_animation_new (GTK_WIDGET (self),
                                             0.0, 1.0, 250,
                                             target);
}

/**
 * valent_share_dialog_row_get_device: (set-property device)
 * @row: a `ValentShareDialogRow`
 *
 * Get the device.
 *
 * Returns: (transfer none) (nullable): a `ValentDevice`
 */
ValentDevice *
valent_share_dialog_row_get_device (ValentShareDialogRow *row)
{
  g_return_val_if_fail (VALENT_IS_SHARE_DIALOG_ROW (row), NULL);

  return row->device;
}

/**
 * valent_share_dialog_row_get_selected: (get-property selected)
 * @row: a `ValentShareDialogRow`
 *
 * Get whether the row is selected.
 *
 * Returns: %TRUE if the row is selected, or %FALSE if not
 */
gboolean
valent_share_dialog_row_get_selected (ValentShareDialogRow *row)
{
  g_return_val_if_fail (VALENT_IS_SHARE_DIALOG_ROW (row), FALSE);

  return row->selected;
}

/**
 * valent_share_dialog_row_set_selected: (set-property selected)
 * @row: a `ValentShareDialogRow`
 * @selected: whether to select the row
 *
 * Set whether the row is selected.
 */
void
valent_share_dialog_row_set_selected (ValentShareDialogRow *row,
                                      gboolean              selected)
{
  g_return_if_fail (VALENT_IS_SHARE_DIALOG_ROW (row));

  selected = !!selected;
  if (row->selected == selected)
    return;

  gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                               GTK_ACCESSIBLE_STATE_SELECTED, selected,
                               -1);

  row->selected = selected;
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_SELECTED]);
}

/**
 * valent_share_dialog_row_get_selection_mode:
 * @row: a `ValentShareDialogRow`
 *
 * Get whether selection mode is enabled.
 *
 * Returns: %TRUE if selection mode is enabled, or %FALSE if not
 */
gboolean
valent_share_dialog_row_get_selection_mode (ValentShareDialogRow *row)
{
  g_return_val_if_fail (VALENT_IS_SHARE_DIALOG_ROW (row), FALSE);

  return row->selection_mode;
}

/**
 * valent_share_dialog_row_set_selection_mode: (set-property selection-mode)
 * @row: a `ValentShareDialogRow`
 * @selection_mode: whether to select the row
 *
 * Set whether selection mode is enabled.
 */
void
valent_share_dialog_row_set_selection_mode (ValentShareDialogRow *row,
                                            gboolean              selection_mode)
{
  g_return_if_fail (VALENT_IS_SHARE_DIALOG_ROW (row));

  selection_mode = !!selection_mode;
  if (row->selection_mode == selection_mode)
    return;

  if (selection_mode)
    {
      gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                                   GTK_ACCESSIBLE_STATE_SELECTED, FALSE,
                                   -1);
    }
  else
    {
      gtk_accessible_reset_state (GTK_ACCESSIBLE (row),
                                  GTK_ACCESSIBLE_STATE_SELECTED);
    }

  adw_animation_skip (row->animation);
  adw_timed_animation_set_reverse (ADW_TIMED_ANIMATION (row->animation),
                                   selection_mode);
  adw_animation_play (row->animation);

  row->selection_mode = selection_mode;
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_SELECTION_MODE]);
}

