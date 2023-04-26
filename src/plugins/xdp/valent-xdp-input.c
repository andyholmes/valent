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

  XdpSession         *session;
  uint8_t             session_state : 2;
};

G_DEFINE_FINAL_TYPE (ValentXdpInput, valent_xdp_input, VALENT_TYPE_INPUT_ADAPTER)

enum {
  SESSION_STATE_CLOSED,
  SESSION_STATE_STARTING = (1 << 0),
  SESSION_STATE_ACTIVE  =  (1 << 1),
};


/*
 * Portal Callbacks
 */
static void
on_session_closed (XdpSession *session,
                   gpointer    user_data)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (user_data);

  g_clear_object (&self->session);
  self->session_state = SESSION_STATE_CLOSED;
}

static void
xdp_session_start_cb (XdpSession     *session,
                      GAsyncResult   *result,
                      ValentXdpInput *self)
{
  g_autoptr (GError) error = NULL;

  g_assert (XDP_IS_SESSION (session));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!xdp_session_start_finish (session, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("%s(): %s", G_STRFUNC, error->message);
      self->session_state = SESSION_STATE_CLOSED;

      return;
    }

  if (!(xdp_session_get_devices (session) & XDP_DEVICE_POINTER) ||
      !(xdp_session_get_devices (session) & XDP_DEVICE_KEYBOARD))
    {
      g_warning ("%s(): failed to get input device", G_STRFUNC);
      self->session_state = SESSION_STATE_CLOSED;

      return;
    }

  self->session = g_object_ref (session);
  self->session_state = SESSION_STATE_ACTIVE;
}

static void
xdp_portal_create_remote_desktop_session_cb (XdpPortal      *portal,
                                             GAsyncResult   *result,
                                             ValentXdpInput *self)
{
  g_autoptr (XdpSession) session = NULL;
  g_autoptr (XdpParent) parent = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (XDP_IS_PORTAL (portal));
  g_assert (G_IS_TASK (result));

  session = xdp_portal_create_remote_desktop_session_finish (portal,
                                                             result,
                                                             &error);

  if (session == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("%s(): %s", G_STRFUNC, error->message);
      self->session_state = SESSION_STATE_CLOSED;

      return;
    }

  g_signal_connect_object (session,
                           "closed",
                           G_CALLBACK (on_session_closed),
                           self, 0);

  parent = valent_xdp_get_parent (NULL);
  xdp_session_start (session,
                     parent,
                     g_task_get_cancellable (G_TASK (result)),
                     (GAsyncReadyCallback)xdp_session_start_cb,
                     self);
}

static gboolean
ensure_session (ValentXdpInput *self)
{
  g_autoptr (GCancellable) destroy = NULL;

  if G_LIKELY (self->session_state == SESSION_STATE_ACTIVE)
    return TRUE;

  if G_LIKELY (self->session_state == SESSION_STATE_STARTING)
    return FALSE;

  self->session_state = SESSION_STATE_STARTING;
  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  xdp_portal_create_remote_desktop_session (valent_xdp_get_default (),
                                            (XDP_DEVICE_KEYBOARD |
                                             XDP_DEVICE_POINTER),
                                            XDP_OUTPUT_MONITOR,
                                            XDP_REMOTE_DESKTOP_FLAG_NONE,
                                            XDP_CURSOR_MODE_HIDDEN,
                                            destroy,
                                            (GAsyncReadyCallback)xdp_portal_create_remote_desktop_session_cb,
                                            self);

  return FALSE;
}


/*
 * ValentInputAdapter
 */
static void
valent_xdp_input_keyboard_keysym (ValentInputAdapter *adapter,
                                  unsigned int        keysym,
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
 * GObject
 */
static void
valent_xdp_input_dispose (GObject *object)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (object);

  if (self->session != NULL)
    xdp_session_close (self->session);

  G_OBJECT_CLASS (valent_xdp_input_parent_class)->dispose (object);
}

static void
valent_xdp_input_finalize (GObject *object)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (object);

  g_clear_object (&self->session);

  G_OBJECT_CLASS (valent_xdp_input_parent_class)->finalize (object);
}

static void
valent_xdp_input_class_init (ValentXdpInputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentInputAdapterClass *adapter_class = VALENT_INPUT_ADAPTER_CLASS (klass);

  object_class->dispose = valent_xdp_input_dispose;
  object_class->finalize = valent_xdp_input_finalize;

  adapter_class->keyboard_keysym = valent_xdp_input_keyboard_keysym;
  adapter_class->pointer_axis = valent_xdp_input_pointer_axis;
  adapter_class->pointer_button = valent_xdp_input_pointer_button;
  adapter_class->pointer_motion = valent_xdp_input_pointer_motion;
}

static void
valent_xdp_input_init (ValentXdpInput *self)
{
}

