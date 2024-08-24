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

#include <gio/gio.h>
#include <libportal/portal.h>
#include <valent.h>

#include "valent-xdp-input.h"
#include "valent-xdp-utils.h"


struct _ValentXdpInput
{
  ValentInputAdapter  parent_instance;

  GSettings          *settings;
  XdpSession         *session;
  gboolean            session_starting;
  gboolean            started;
};

G_DEFINE_FINAL_TYPE (ValentXdpInput, valent_xdp_input, VALENT_TYPE_INPUT_ADAPTER)


/*
 * Portal Callbacks
 */
static void
on_session_closed (ValentXdpInput *self)
{
  g_clear_object (&self->session);
  self->started = FALSE;
}

static void
on_session_started (XdpSession   *session,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (user_data);
  g_autoptr (GError) error = NULL;

  self->started = xdp_session_start_finish (session, res, &error);
  if (!self->started)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      g_clear_object (&self->session);
    }

  self->session_starting = FALSE;
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
                           self,
                           G_CONNECT_SWAPPED);

  parent = valent_xdp_get_parent ();
  xdp_session_start (self->session,
                     parent,
                     g_task_get_cancellable (G_TASK (result)),
                     (GAsyncReadyCallback)on_session_started,
                     self);
}

static gboolean
ensure_session (ValentXdpInput *self)
{
  g_autoptr (GCancellable) cancellable = NULL;

  if G_LIKELY (self->started)
    return TRUE;

  if (self->session_starting)
    return FALSE;

  self->session_starting = TRUE;
  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  xdp_portal_create_remote_desktop_session (valent_xdp_get_default (),
                                            (XDP_DEVICE_KEYBOARD |
                                             XDP_DEVICE_POINTER),
                                            XDP_OUTPUT_NONE,
                                            XDP_REMOTE_DESKTOP_FLAG_NONE,
                                            XDP_CURSOR_MODE_HIDDEN,
                                            cancellable,
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
  self->settings = g_settings_new ("ca.andyholmes.Valent.Plugin.xdp");
}

