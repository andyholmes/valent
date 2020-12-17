// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-input"

#include "config.h"

#include <glib-object.h>
#include <libportal/portal.h>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#  include <gdk/x11/gdkx.h>
#endif
#include <libvalent-core.h>
#include <libvalent-input.h>

#include "valent-xdp-input.h"
#include "valent-xdp-utils.h"

#define REMOTE_SESSION_TIMEOUT 15


struct _ValentXdpInput
{
  PeasExtensionBase  parent_instance;

  GCancellable      *cancellable;
  GSettings         *settings;

  XdpSession        *session;
  unsigned long      session_id;
  gint64             session_expiry;
  guint              session_expiry_id;
  gboolean           session_starting;
  gboolean           started;
};

/* Interfaces */
static void valent_input_controller_iface_init (ValentInputControllerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentXdpInput, valent_xdp_input, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_INPUT_CONTROLLER, valent_input_controller_iface_init))


/*
 * Portal Callbacks
 */
static void
session_update (ValentXdpInput *self)
{
  g_autoptr (GDateTime) now = NULL;

  now = g_date_time_new_now_local ();
  self->session_expiry = g_date_time_to_unix (now) + REMOTE_SESSION_TIMEOUT;
}

static void
on_session_closed (XdpSession *session,
                   gpointer    user_data)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (user_data);

  /* Mark the session as inactive */
  self->started = FALSE;
  g_clear_signal_handler (&self->session_id, self->session);
  g_clear_object (&self->session);
}

static gboolean
on_session_expired (gpointer user_data)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (user_data);
  g_autoptr (GDateTime) now = NULL;
  gint remainder;

  /* If the session has been used recently, schedule a new expiry */
  now = g_date_time_new_now_local ();
  remainder = self->session_expiry - g_date_time_to_unix (now);

  if (remainder > 0)
    {
      self->session_expiry_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                            remainder,
                                                            on_session_expired,
                                                            g_object_ref (self),
                                                            g_object_unref);

      return G_SOURCE_REMOVE;
    }

  /* Otherwise if there's an active session, close it */
  if (self->session != NULL)
    on_session_closed (self->session, self);

  // Reset the GSource Id
  self->session_expiry_id = 0;

  return G_SOURCE_REMOVE;
}

static void
on_session_started (XdpSession   *session,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (user_data);
  g_autoptr (GError) error = NULL;
  unsigned int timeout;

  if (!xdp_session_start_finish (session, res, &error))
    {
      g_warning ("[%s] %s", G_STRFUNC, error->message);
      g_clear_object (&self->session);
      self->session_starting = FALSE;

      return;
    }

  if (!(xdp_session_get_devices (session) & XDP_DEVICE_POINTER) ||
      !(xdp_session_get_devices (session) & XDP_DEVICE_KEYBOARD))
    {
      g_warning ("[%s] Failed to get input device", G_STRFUNC);
      g_clear_object (&self->session);
      self->session_starting = FALSE;

      return;
    }

  /* Hold a reference to the session and watch for closes */
  self->session_id = g_signal_connect (self->session,
                                       "closed",
                                       G_CALLBACK (on_session_closed),
                                       self);

  /* Set a timeout */
  timeout = g_settings_get_int (self->settings, "xdp-session-timeout");

  if (timeout > 0)
    {
      self->session_expiry_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                            timeout,
                                                            on_session_expired,
                                                            g_object_ref (self),
                                                            g_object_unref);
      session_update (self);
    }

  self->session_starting = FALSE;
  self->started = TRUE;
}

static void
on_session_created (XdpPortal    *portal,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (user_data);
  g_autoptr (GError) error = NULL;

  self->session = xdp_portal_create_remote_desktop_session_finish (portal,
                                                                   result,
                                                                   &error);

  if (self->session == NULL)
    {
      g_warning ("[%s] %s", G_STRFUNC, error->message);
      self->session_starting = FALSE;

      return;
    }

  /* Hold a reference to the session and queue the start */
  xdp_session_start (self->session,
                     NULL,
                     self->cancellable,
                     (GAsyncReadyCallback)on_session_started,
                     self);
}

static gboolean
ensure_session (ValentXdpInput *self)
{
  if G_LIKELY (self->started)
    {
      session_update (self);
      return TRUE;
    }

  if (self->session_starting)
    return FALSE;

  /* Try to acquire a new session */
  self->session_starting = TRUE;
  xdp_portal_create_remote_desktop_session (valent_xdp_get_default (),
                                            (XDP_DEVICE_KEYBOARD |
                                             XDP_DEVICE_POINTER),
                                            XDP_OUTPUT_MONITOR,
                                            XDP_REMOTE_DESKTOP_FLAG_NONE,
                                            self->cancellable,
                                            (GAsyncReadyCallback)on_session_created,
                                            self);

  return FALSE;
}

/*
 * ValentInputController
 */
static void
valent_xdp_input_keyboard_keysym (ValentInputController *controller,
                                       guint                  keysym,
                                       gboolean               state)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (controller);

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_XDP_INPUT (self));

  if G_UNLIKELY (!ensure_session (self))
    return;

  // TODO: XDP_KEY_PRESSED/XDP_KEY_RELEASED

  xdp_session_keyboard_key (self->session, TRUE, keysym, state);
}

static gint
translate_pointer_button (ValentPointerButton button)
{
  switch (button)
    {
    case VALENT_POINTER_PRIMARY:
      return 0x110;

    case VALENT_POINTER_MIDDLE:
      return 0x112;

    case VALENT_POINTER_SECONDARY:
      return 0x111;

    case VALENT_POINTER_WHEEL_DOWN:
      return 0x110; // FIXME

    case VALENT_POINTER_WHEEL_UP:
      return 0x10F;

    default:
      return 0x110;
    }
}

static void
valent_xdp_input_pointer_axis (ValentInputController *controller,
                                    double                 dx,
                                    double                 dy)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (controller);

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_XDP_INPUT (self));
  g_assert (dx != 0.0 || dy != 0.0);

  if G_UNLIKELY (!ensure_session (self))
    return;

  /* On X11 we use discrete axis steps (eg. mouse wheel) because the absolute
   * axis change doesn't seem to work
   */
  #ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
      {
        g_debug ("[%s] X11: using discrete axis step", G_STRFUNC);

        if (dy < 0.0)
          xdp_session_pointer_axis_discrete (self->session,
                                             XDP_AXIS_VERTICAL_SCROLL,
                                             1);
        else if (dy > 0.0)
          xdp_session_pointer_axis_discrete (self->session,
                                             XDP_AXIS_VERTICAL_SCROLL,
                                             -1);

        return;
      }
  #endif

  xdp_session_pointer_axis (self->session, FALSE, dx, dy);
  xdp_session_pointer_axis (self->session, TRUE, 0.0, 0.0);
}

static void
valent_xdp_input_pointer_button (ValentInputController *controller,
                                      ValentPointerButton    button,
                                      gboolean               pressed)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (controller);

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_XDP_INPUT (self));
  g_assert (button > 0 && button < 8);

  if G_UNLIKELY (!ensure_session (self))
    return;

  /* Translate the button to EVDEV constant */
  button = translate_pointer_button (button);
  xdp_session_pointer_button (self->session, button, pressed);
}

static void
valent_xdp_input_pointer_motion (ValentInputController *controller,
                                      double                 dx,
                                      double                 dy)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (controller);

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_XDP_INPUT (self));
  g_return_if_fail (dx != 0 || dy != 0);

  if G_UNLIKELY (!ensure_session (self))
    return;

  xdp_session_pointer_motion (self->session, dx, dy);
}

static void
valent_xdp_input_pointer_position (ValentInputController *controller,
                                        double                 x,
                                        double                 y)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (controller);

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_XDP_INPUT (self));

  if G_UNLIKELY (!ensure_session (self))
    return;

  // FIXME: stream = 0?
  xdp_session_pointer_position (self->session, 0, x, y);
}

static void
valent_input_controller_iface_init (ValentInputControllerInterface *iface)
{
  iface->keyboard_keysym = valent_xdp_input_keyboard_keysym;
  iface->pointer_axis = valent_xdp_input_pointer_axis;
  iface->pointer_button = valent_xdp_input_pointer_button;
  iface->pointer_motion = valent_xdp_input_pointer_motion;
  iface->pointer_position = valent_xdp_input_pointer_position;
}

/*
 * GObject
 */
static void
valent_xdp_input_dispose (GObject *object)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (object);

  g_cancellable_cancel (self->cancellable);

  /* Stop and dispose any active session */
  if (self->session != NULL)
    {
      if (self->started)
        xdp_session_close (self->session);

      g_clear_signal_handler (&self->session_id, self->session);
      g_clear_object (&self->session);
    }

  G_OBJECT_CLASS (valent_xdp_input_parent_class)->dispose (object);
}

static void
valent_xdp_input_finalize (GObject *object)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->settings);
  g_clear_object (&self->session);

  G_OBJECT_CLASS (valent_xdp_input_parent_class)->finalize (object);
}

static void
valent_xdp_input_class_init (ValentXdpInputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_xdp_input_dispose;
  object_class->finalize = valent_xdp_input_finalize;
}

static void
valent_xdp_input_init (ValentXdpInput *self)
{
  self->cancellable = g_cancellable_new ();
  self->settings = g_settings_new ("ca.andyholmes.valent.xdp");

  self->session = NULL;
  self->session_id = 0;
  self->started = FALSE;
}

