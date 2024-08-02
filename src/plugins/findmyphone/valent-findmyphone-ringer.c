// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-findmyphone-plugin"

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gst/gst.h>

#include "valent-findmyphone-ringer.h"


struct _ValentFindmyphoneRinger
{
  GActionGroup  *actions;
  GNotification *notification;
  GstElement    *playbin;
  unsigned int   source_id;
  gpointer       owner;
};

static ValentFindmyphoneRinger *default_ringer = NULL;


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

static inline void
app_ringer_action (GSimpleAction *action,
                   GVariant      *parameters,
                   gpointer       user_data)
{
  ValentFindmyphoneRinger *self = (ValentFindmyphoneRinger *)user_data;

  g_assert (self != NULL);

  valent_findmyphone_ringer_toggle (self, NULL);
}

static const GActionEntry app_actions[] = {
  { "ringer", app_ringer_action, NULL, NULL, NULL },
};

static void
valent_findmyphone_ringer_free (gpointer data)
{
  ValentFindmyphoneRinger *ringer = data;

  if (ringer->notification != NULL)
    {
      GApplication *application = g_application_get_default ();

      g_action_map_remove_action (G_ACTION_MAP (application), "ringer");
      g_application_withdraw_notification (application, "findmyphone::ringer");
      g_clear_object (&ringer->notification);
    }

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
 * Create a new `ValentFindmyphoneRinger`.
 *
 * Returns: (transfer full): a `ValentFindmyphoneRinger`
 */
ValentFindmyphoneRinger *
valent_findmyphone_ringer_new (void)
{
  GApplication *application = g_application_get_default ();
  ValentFindmyphoneRinger *ringer;
  g_autoptr (GError) error = NULL;

  ringer = g_rc_box_new0 (ValentFindmyphoneRinger);

  /* Notification
   */
  if (application != NULL)
    {
      g_autoptr (GIcon) icon = NULL;

      g_action_map_add_action_entries (G_ACTION_MAP (application),
                                       app_actions,
                                       G_N_ELEMENTS (app_actions),
                                       ringer);
      icon = g_icon_new_for_string ("phonelink-ring-symbolic", NULL);

      ringer->notification = g_notification_new (_("Find My Device"));
      g_notification_set_icon (ringer->notification, icon);
      g_notification_set_priority (ringer->notification,
                                   G_NOTIFICATION_PRIORITY_URGENT);
      g_notification_set_default_action (ringer->notification, "app.ringer");
    }

  /* Playbin
   */
  if (!gst_init_check (NULL, NULL, &error))
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return ringer;
    }

  ringer->playbin = gst_element_factory_make ("playbin", "findmyphone-ringer");
  if (ringer->playbin != NULL)
    {
      gst_object_ref_sink (ringer->playbin);
      g_object_set (ringer->playbin,
                    "uri", "resource:///plugins/findmyphone/alert.oga",
                    NULL);
    }

  return ringer;
}

/**
 * valent_findmyphone_ringer_start:
 * @ringer: a `ValentFindmyphoneRinger`
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
 * @ringer: a `ValentFindmyphoneRinger`
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
 * @ringer: a `ValentFindmyphoneRinger`
 *
 * Enable the ringing state of @ringer and show a dialog.
 */
void
valent_findmyphone_ringer_show (ValentFindmyphoneRinger *ringer)
{
  GApplication *application = g_application_get_default ();

  g_return_if_fail (ringer != NULL);

  valent_findmyphone_ringer_start (ringer);
  if (application != NULL)
    {
      g_application_send_notification (application,
                                       "findmyphone::ringer",
                                       ringer->notification);
    }
}

/**
 * valent_findmyphone_ringer_hide:
 * @ringer: a `ValentFindmyphoneRinger`
 *
 * Disable the ringing state of @ringer and hide the dialog.
 */
void
valent_findmyphone_ringer_hide (ValentFindmyphoneRinger *ringer)
{
  GApplication *application = g_application_get_default ();

  g_return_if_fail (ringer != NULL);

  if (ringer->notification != NULL)
    g_application_withdraw_notification (application, "findmyphone::ringer");

  valent_findmyphone_ringer_stop (ringer);
}

/**
 * valent_findmyphone_ringer_acquire:
 *
 * Acquire a reference on the default `ValentFindmyphoneRinger`.
 *
 * Returns: (transfer full): a `ValentFindmyphoneRinger`
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
 * @data: (type Valent.FindmyphoneRinger): a `ValentFindmyphoneRinger`
 *
 * Release a reference on the default `ValentFindmyphoneRinger`.
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
 * @ringer: a `ValentFindmyphoneRinger`
 * @owner: (type GObject.Object): a `GObject`
 *
 * Toggle the ringing state of @ringer.
 */
void
valent_findmyphone_ringer_toggle (ValentFindmyphoneRinger *ringer,
                                  gpointer                 owner)
{
  g_return_if_fail (ringer != NULL);

  if (ringer->source_id > 0)
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
 * @ringer: a `ValentFindmyphoneRinger`
 * @owner: (type GObject.Object): a `GObject`
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

