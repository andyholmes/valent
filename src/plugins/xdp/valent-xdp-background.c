// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-background"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libportal/portal.h>
#include <valent.h>

#include "valent-xdp-background.h"
#include "valent-xdp-utils.h"


struct _ValentXdpBackground
{
  ValentApplicationPlugin  parent;

  GSettings               *settings;
  GCancellable            *cancellable;
  unsigned int             autostart : 1;
  unsigned long            active_id;
};

G_DEFINE_FINAL_TYPE (ValentXdpBackground, valent_xdp_background, VALENT_TYPE_APPLICATION_PLUGIN)


static gboolean
valent_xdp_background_is_active (ValentXdpBackground *self)
{
  GApplication *application = NULL;
  GtkWindow *window = NULL;

  application = g_application_get_default ();

  if (GTK_IS_APPLICATION (application))
    window = gtk_application_get_active_window (GTK_APPLICATION (application));

  if (GTK_IS_WINDOW (window))
    return gtk_window_is_active (window);

  return FALSE;
}

static void
xdp_portal_request_background_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  XdpPortal *portal = XDP_PORTAL (object);
  g_autoptr (GError) error = NULL;

  g_assert (XDP_IS_PORTAL (portal));

  if (!xdp_portal_request_background_finish (portal, result, &error))
    g_warning ("ValentXdpPlugin: %s", error->message);
}

static void
valent_xdp_background_request (ValentXdpBackground *self)
{
  g_autoptr (GPtrArray) commandline = NULL;
  g_autoptr (XdpParent) parent = NULL;
  XdpBackgroundFlags flags = XDP_BACKGROUND_FLAG_NONE;

  g_assert (VALENT_IS_XDP_BACKGROUND (self));

  parent = valent_xdp_get_parent (NULL);

  if (self->autostart)
    {
      commandline = g_ptr_array_new_with_free_func (g_free);
      g_ptr_array_add (commandline, g_strdup ("valent"));
      g_ptr_array_add (commandline, g_strdup ("--gapplication-service"));

      flags |= XDP_BACKGROUND_FLAG_AUTOSTART;
    }

  xdp_portal_request_background (valent_xdp_get_default (),
                                 parent,
                                 _("Valent wants to run as a service"),
                                 commandline,
                                 flags,
                                 self->cancellable,
                                 xdp_portal_request_background_cb,
                                 NULL);
}

static void
on_active_window_changed (GtkApplication      *application,
                          GParamSpec          *pspec,
                          ValentXdpBackground *self)
{
  GtkWindow *window = NULL;

  g_assert (VALENT_IS_XDP_BACKGROUND (self));

  window = gtk_application_get_active_window (application);

  if (GTK_IS_WINDOW (window) && gtk_window_is_active (window))
    {
      valent_xdp_background_request (self);
      g_clear_signal_handler (&self->active_id, application);
    }
}

static void
on_autostart_changed (GSettings           *settings,
                      const char          *key,
                      ValentXdpBackground *self)
{
  g_assert (VALENT_IS_XDP_BACKGROUND (self));

  self->autostart = g_settings_get_boolean (self->settings, "autostart");

  /* Already waiting for an active window */
  if (self->active_id > 0)
    return;

  /* If there is no window or Valent is not the focused application, defer the
   * request until that changes. */
  if (!valent_xdp_background_is_active (self))
    {
      self->active_id = g_signal_connect (g_application_get_default (),
                                          "notify::active-window",
                                          G_CALLBACK (on_active_window_changed),
                                          self);
      return;
    }

  valent_xdp_background_request (self);
}

/*
 * ValentApplicationPlugin
 */
static void
valent_xdp_background_enable (ValentApplicationPlugin *plugin)
{
  ValentXdpBackground *self = VALENT_XDP_BACKGROUND (plugin);

  self->cancellable = g_cancellable_new ();

  self->settings = g_settings_new ("ca.andyholmes.Valent.Plugin.xdp");
  g_signal_connect (self->settings,
                    "changed::autostart",
                    G_CALLBACK (on_autostart_changed),
                    self);

  on_autostart_changed (self->settings, "autostart", self);
}

static void
valent_xdp_background_disable (ValentApplicationPlugin *plugin)
{
  ValentXdpBackground *self = VALENT_XDP_BACKGROUND (plugin);

  g_clear_signal_handler (&self->active_id, g_application_get_default ());
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->settings);

  /* If the extension is being disabled during application shutdown, the main
   * window is already closed and this will be skipped. If the user has disabled
   * the extension, then the window must be active and it will succeed */
  if (valent_xdp_background_is_active (self))
    {
      self->autostart = FALSE;
      valent_xdp_background_request (self);
    }
}

/*
 * GObject
 */
static void
valent_xdp_background_class_init (ValentXdpBackgroundClass *klass)
{
  ValentApplicationPluginClass *plugin_class = VALENT_APPLICATION_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_xdp_background_enable;
  plugin_class->disable = valent_xdp_background_disable;
}

static void
valent_xdp_background_init (ValentXdpBackground *self)
{
}

