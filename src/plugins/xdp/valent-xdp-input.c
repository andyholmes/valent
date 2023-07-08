// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-input"

#include "config.h"

#ifdef __linux__
# include <linux/input-event-codes.h>
#else
# define BTN_LEFT    0x110
# define BTN_RIGHT   0x111
# define BTN_MIDDLE  0x112
#endif /* __linux */

#include <glib-object.h>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#  include <gdk/x11/gdkx.h>
#endif /* GDK_WINDOWING_X11 */
#include <libportal/portal.h>
#include <valent.h>

#include "valent-xdp-input.h"
#include "valent-xdp-utils.h"


struct _ValentXdpInput
{
  ValentInputAdapter  parent_instance;

  GCancellable       *cancellable;
  GSettings          *settings;

  XdpSession         *session;
  int64_t             session_expiry;
  guint               session_expiry_id;
  gboolean            session_starting;
  gboolean            started;
};

G_DEFINE_FINAL_TYPE (ValentXdpInput, valent_xdp_input, VALENT_TYPE_INPUT_ADAPTER)


/*
 * Portal Callbacks
 */
static void
session_update (ValentXdpInput *self)
{
  g_autoptr (GDateTime) now = NULL;
  unsigned int timeout = 0;

  now = g_date_time_new_now_local ();
  timeout = g_settings_get_uint (self->settings, "xdp-session-timeout");
  self->session_expiry = g_date_time_to_unix (now) + timeout;
}

static void
on_session_closed (XdpSession *session,
                   gpointer    user_data)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (user_data);

  /* Mark the session as inactive */
  self->started = FALSE;
  g_clear_handle_id (&self->session_expiry_id, g_source_remove);
  g_clear_object (&self->session);
}

static gboolean
on_session_expired (gpointer user_data)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (user_data);
  g_autoptr (GDateTime) now = NULL;
  int remainder;

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
    xdp_session_close (self->session);

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
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      g_clear_object (&self->session);
      self->session_starting = FALSE;

      return;
    }

  if (!(xdp_session_get_devices (session) & XDP_DEVICE_POINTER) ||
      !(xdp_session_get_devices (session) & XDP_DEVICE_KEYBOARD))
    {
      g_warning ("%s(): failed to get input device", G_STRFUNC);
      g_clear_object (&self->session);
      self->session_starting = FALSE;

      return;
    }

  /* Set a timeout */
  timeout = g_settings_get_uint (self->settings, "xdp-session-timeout");

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
  g_autoptr (XdpParent) parent = NULL;
  g_autoptr (GError) error = NULL;

  self->session = xdp_portal_create_remote_desktop_session_finish (portal,
                                                                   result,
                                                                   &error);

  if (self->session == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      self->session_starting = FALSE;

      return;
    }

  g_signal_connect_object (self->session,
                           "closed",
                           G_CALLBACK (on_session_closed),
                           self, 0);

  parent = valent_xdp_get_parent (NULL);
  xdp_session_start (self->session,
                     parent,
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
                                            XDP_CURSOR_MODE_HIDDEN,
                                            self->cancellable,
                                            (GAsyncReadyCallback)on_session_created,
                                            self);

  return FALSE;
}


/*
 * ValentInputAdapter
 */
static void
valent_xdp_input_keyboard_keysym (ValentInputAdapter *adapter,
                                  uint32_t            keysym,
                                  gboolean            state)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_XDP_INPUT (self));

  if G_UNLIKELY (!ensure_session (self))
    return;

  // TODO: XDP_KEY_PRESSED/XDP_KEY_RELEASED

  xdp_session_keyboard_key (self->session, TRUE, keysym, state);
}

static unsigned int
translate_to_evdev_button (unsigned int button)
{
  switch (button)
    {
    case VALENT_POINTER_PRIMARY:
      return BTN_LEFT;

    case VALENT_POINTER_MIDDLE:
      return BTN_MIDDLE;

    case VALENT_POINTER_SECONDARY:
      return BTN_RIGHT;

    default:
      /* Any other buttons go after the legacy scroll buttons (4-7). */
      return button + (BTN_LEFT - 1) - 4;
    }
}

static void
valent_xdp_input_pointer_axis (ValentInputAdapter *adapter,
                               double              dx,
                               double              dy)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_XDP_INPUT (self));
  g_assert (!G_APPROX_VALUE (dx, 0.0, 0.01) || !G_APPROX_VALUE (dy, 0.0, 0.01));

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
  #endif /* GDK_WINDOWING_X11 */

  xdp_session_pointer_axis (self->session, FALSE, dx, dy);
  xdp_session_pointer_axis (self->session, TRUE, 0.0, 0.0);
}

static void
valent_xdp_input_pointer_button (ValentInputAdapter *adapter,
                                 unsigned int        button,
                                 gboolean            pressed)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_XDP_INPUT (self));

  if G_UNLIKELY (!ensure_session (self))
    return;

  /* Translate the button to EVDEV constant */
  button = translate_to_evdev_button (button);
  xdp_session_pointer_button (self->session, button, pressed);
}

static void
valent_xdp_input_pointer_motion (ValentInputAdapter *adapter,
                                 double              dx,
                                 double              dy)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_XDP_INPUT (self));

  if G_UNLIKELY (!ensure_session (self))
    return;

  xdp_session_pointer_motion (self->session, dx, dy);
}


/*
 * ValentObject
 */
static void
valent_xdp_input_destroy (ValentObject *object)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (object);

  g_cancellable_cancel (self->cancellable);

  if (self->session != NULL)
    xdp_session_close (self->session);

  VALENT_OBJECT_CLASS (valent_xdp_input_parent_class)->destroy (object);
}

/*
 * GObject
 */
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
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentInputAdapterClass *adapter_class = VALENT_INPUT_ADAPTER_CLASS (klass);

  object_class->finalize = valent_xdp_input_finalize;

  vobject_class->destroy = valent_xdp_input_destroy;

  adapter_class->keyboard_keysym = valent_xdp_input_keyboard_keysym;
  adapter_class->pointer_axis = valent_xdp_input_pointer_axis;
  adapter_class->pointer_button = valent_xdp_input_pointer_button;
  adapter_class->pointer_motion = valent_xdp_input_pointer_motion;
}

static void
valent_xdp_input_init (ValentXdpInput *self)
{
  self->cancellable = g_cancellable_new ();
  self->settings = g_settings_new ("ca.andyholmes.Valent.Plugin.xdp");
}

