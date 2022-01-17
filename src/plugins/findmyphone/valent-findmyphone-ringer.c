// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-findmyphone-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include "valent-findmyphone-ringer.h"


struct _ValentFindmyphoneRinger
{
  GtkWindow    *dialog;
  GstElement   *playbin;
  unsigned int  source_id;
  gpointer      owner;
};

static ValentFindmyphoneRinger *default_ringer = NULL;


static gboolean
gtk_window_destroy_idle (gpointer data)
{
  gtk_window_destroy (GTK_WINDOW (data));
  return G_SOURCE_REMOVE;
}

static void
on_motion_event (GtkWindow *dialog)
{
  g_idle_add (gtk_window_destroy_idle, dialog);
}

static gboolean
ringer_source_func (GstBus     *bus,
                    GstMessage *message,
                    gpointer    user_data)
{
  ValentFindmyphoneRinger *ringer = user_data;

  if (message->type == GST_MESSAGE_ERROR)
    {
      g_autoptr (GError) error = NULL;
      g_autofree char *debug = NULL;

      gst_message_parse_error (message, &error, &debug);
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      g_debug ("%s: %s", G_STRFUNC, (debug) ? debug : "none");

      return G_SOURCE_REMOVE;
    }

  /* Rewind to beginning */
  if (message->type == GST_MESSAGE_EOS)
    {
      return gst_element_seek_simple (ringer->playbin,
                                      GST_FORMAT_TIME,
                                      GST_SEEK_FLAG_FLUSH,
                                      0);
    }

  return G_SOURCE_CONTINUE;
}

static void
valent_findmyphone_ringer_free (gpointer data)
{
  ValentFindmyphoneRinger *ringer = data;

  g_clear_pointer (&ringer->dialog, gtk_window_destroy);

  if (ringer->playbin != NULL)
    {
      gst_element_set_state (ringer->playbin, GST_STATE_NULL);
      gst_clear_object (&ringer->playbin);
    }

  default_ringer = NULL;
}

/**
 * valent_findmyphone_ringer_new:
 *
 * Create a new #ValentFindmyphoneRinger.
 *
 * Returns: (transfer full): a #ValentFindmyphoneRinger
 */
ValentFindmyphoneRinger *
valent_findmyphone_ringer_new (void)
{
  ValentFindmyphoneRinger *ringer;
  g_autoptr (GError) error = NULL;

  ringer = g_rc_box_new0 (ValentFindmyphoneRinger);

  if (!gst_init_check (NULL, NULL, &error))
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return ringer;
    }

  /* Playbin */
  ringer->playbin = gst_element_factory_make ("playbin", "findmyphone-ringer");

  if (ringer->playbin != NULL)
    {
      g_object_set (ringer->playbin,
                    "uri", "resource:///plugins/findmyphone/ring.oga",
                    NULL);
    }

  return ringer;
}

/**
 * valent_findmyphone_ringer_start:
 * @ringer: a #ValentFindmyphoneRinger
 *
 * Enable the ringing state of @ringer.
 */
void
valent_findmyphone_ringer_start (ValentFindmyphoneRinger *ringer)
{
  g_autoptr (GstBus) playbus = NULL;

  g_return_if_fail (ringer != NULL);

  if (ringer->playbin == NULL || ringer->source_id > 0)
    return;

  playbus = gst_element_get_bus (ringer->playbin);
  ringer->source_id = gst_bus_add_watch (playbus, ringer_source_func, ringer);

  if (gst_element_set_state (ringer->playbin, GST_STATE_PLAYING) == 0)
    g_clear_handle_id (&ringer->source_id, g_source_remove);
}

/**
 * valent_findmyphone_ringer_stop:
 * @ringer: a #ValentFindmyphoneRinger
 *
 * Disable the ringing state of @ringer.
 */
void
valent_findmyphone_ringer_stop (ValentFindmyphoneRinger *ringer)
{
  g_return_if_fail (ringer != NULL);

  if (ringer->playbin == NULL || ringer->source_id == 0)
    return;

  gst_element_set_state (ringer->playbin, GST_STATE_NULL);
  g_clear_handle_id (&ringer->source_id, g_source_remove);
  ringer->owner = NULL;
}

/**
 * valent_findmyphone_ringer_show:
 * @ringer: a #ValentFindmyphoneRinger
 *
 * Enable the ringing state of @ringer and show a dialog.
 */
void
valent_findmyphone_ringer_show (ValentFindmyphoneRinger *ringer)
{
  GtkEventController *controller;
  GtkWidget *label;

  g_return_if_fail (ringer != NULL);

  valent_findmyphone_ringer_start (ringer);

  if (!gtk_is_initialized () || ringer->dialog != NULL)
    return;

  /* Create the dialog */
  ringer->dialog = g_object_new (GTK_TYPE_WINDOW,
                                 "fullscreened", TRUE,
                                 "maximized",    TRUE,
                                 NULL);
  g_object_add_weak_pointer (G_OBJECT (ringer->dialog),
                             (gpointer)&ringer->dialog);

  label = g_object_new (ADW_TYPE_STATUS_PAGE,
                        "title",     _("Found"),
                        "icon-name", "phonelink-ring-symbolic",
                        NULL);
  gtk_window_set_child (ringer->dialog, label);

  g_signal_connect_swapped (ringer->dialog,
                            "destroy",
                            G_CALLBACK (valent_findmyphone_ringer_stop),
                            ringer);

  /* Close on keypress, pointer motion or click */
  controller = gtk_event_controller_key_new ();
  gtk_widget_add_controller (GTK_WIDGET (ringer->dialog), controller);
  g_signal_connect_swapped (G_OBJECT (controller),
                            "key-pressed",
                            G_CALLBACK (gtk_window_destroy),
                            ringer->dialog);

  controller = gtk_event_controller_motion_new ();
  gtk_widget_add_controller (GTK_WIDGET (ringer->dialog), controller);
  g_signal_connect_swapped (G_OBJECT (controller),
                            "motion",
                            G_CALLBACK (on_motion_event),
                            ringer->dialog);

  controller = (GtkEventController *)gtk_gesture_click_new ();
  gtk_widget_add_controller (GTK_WIDGET (ringer->dialog), controller);
  g_signal_connect_swapped (G_OBJECT (controller),
                            "pressed",
                            G_CALLBACK (gtk_window_destroy),
                            ringer->dialog);

  /* Show the dialog */
  gtk_window_present_with_time (ringer->dialog, GDK_CURRENT_TIME);
}

/**
 * valent_findmyphone_ringer_hide:
 * @ringer: a #ValentFindmyphoneRinger
 *
 * Disable the ringing state of @ringer and hide the dialog.
 */
void
valent_findmyphone_ringer_hide (ValentFindmyphoneRinger *ringer)
{
  g_return_if_fail (ringer != NULL);

  if (ringer->dialog != NULL)
    g_clear_pointer (&ringer->dialog, gtk_window_destroy);
  else
    valent_findmyphone_ringer_stop (ringer);
}

/**
 * valent_findmyphone_ringer_acquire:
 *
 * Acquire a reference on the default #ValentFindmyphoneRinger.
 *
 * Returns: (transfer full): a #ValentFindmyphoneRinger
 */
ValentFindmyphoneRinger *
valent_findmyphone_ringer_acquire (void)
{
  if (default_ringer == NULL)
    {
      default_ringer = valent_findmyphone_ringer_new ();
      return default_ringer;
    }

  return g_rc_box_acquire (default_ringer);
}

/**
 * valent_findmyphone_ringer_release:
 * @data: (type Valent.FindmyphoneRinger): a #ValentFindmyphoneRinger
 *
 * Release a reference on the default #ValentFindmyphoneRinger.
 */
void
valent_findmyphone_ringer_release (gpointer data)
{
  ValentFindmyphoneRinger *ringer = data;

  g_return_if_fail (ringer != NULL);

  g_rc_box_release_full (ringer, valent_findmyphone_ringer_free);
}

/**
 * valent_findmyphone_ringer_toggle:
 * @ringer: a #ValentFindmyphoneRinger
 * @owner: (type GObject.Object): a #GObject
 *
 * Toggle the ringing state of @ringer.
 */
void
valent_findmyphone_ringer_toggle (ValentFindmyphoneRinger *ringer,
                                  gpointer                 owner)
{
  g_return_if_fail (ringer != NULL);

  if (ringer->dialog != NULL || ringer->source_id > 0)
    {
      valent_findmyphone_ringer_hide (ringer);
      ringer->owner = NULL;
    }
  else
    {
      valent_findmyphone_ringer_show (ringer);
      ringer->owner = owner;
    }
}

/**
 * valent_findmyphone_ringer_is_owner:
 * @ringer: a #ValentFindmyphoneRinger
 * @owner: (type GObject.Object): a #GObject
 *
 * Check if @owner is responsible for the current state of @ringer.
 *
 * Returns: %TRUE if @owner controls the ringer
 */
gboolean
valent_findmyphone_ringer_is_owner (ValentFindmyphoneRinger *ringer,
                                    gpointer                 owner)
{
  g_return_val_if_fail (ringer != NULL, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (owner), FALSE);

  return ringer->owner == owner;
}

