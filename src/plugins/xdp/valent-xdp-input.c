// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-input"

#include "config.h"

#include <linux/input-event-codes.h>

#include <gio/gio.h>
#include <libportal/portal.h>
#include <valent.h>

#ifdef HAVE_LIBEI
# include "valent-ei-input.h"
#endif /* HAVE_LIBEI */
#include "valent-xdp-utils.h"

#include "valent-xdp-input.h"

struct _ValentXdpInput
{
  ValentInputAdapter  parent_instance;

  ValentInputAdapter *delegate;
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
  g_clear_object (&self->delegate);
  g_clear_object (&self->session);
  self->started = FALSE;
}

static void
on_session_started (XdpSession   *session,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  ValentXdpInput *self = VALENT_XDP_INPUT (user_data);
  g_autofree char *session_token = NULL;
  g_autoptr (GError) error = NULL;

  self->started = xdp_session_start_finish (session, res, &error);
  if (!self->started)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      g_clear_object (&self->session);
    }

#ifdef HAVE_LIBEI
  int ei_fd = xdp_session_connect_to_eis (self->session, &error);
  if (ei_fd > -1)
    {
      self->delegate = g_initable_new (VALENT_TYPE_EI_INPUT, NULL, &error,
                                       "parent", self,
                                       "fd",     ei_fd,
                                       NULL);
      if (self->delegate == NULL)
        g_warning ("%s(): %s", G_STRFUNC, error->message);
    }
  else
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
    }
#endif /* HAVE_LIBEI */

  session_token = xdp_session_get_restore_token (session);
  g_settings_set_string (self->settings,
                         "session-token",
                         session_token ? session_token : "");
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
  g_autofree char *restore_token = NULL;

  if (self->session_starting)
    return FALSE;

  if G_LIKELY (self->started)
    return TRUE;

  self->session_starting = TRUE;
  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  restore_token = g_settings_get_string (self->settings, "session-token");
  if (!g_uuid_string_is_valid (restore_token))
    g_clear_pointer (&restore_token, g_free);

  xdp_portal_create_remote_desktop_session_full (valent_xdp_get_default (),
                                                 (XDP_DEVICE_KEYBOARD |
                                                   XDP_DEVICE_POINTER),
                                                 XDP_OUTPUT_NONE,
                                                 XDP_REMOTE_DESKTOP_FLAG_NONE,
                                                 XDP_CURSOR_MODE_HIDDEN,
                                                 XDP_PERSIST_MODE_PERSISTENT,
                                                 restore_token,
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

  if (self->delegate != NULL)
    {
#ifdef HAVE_LIBEI
      valent_input_adapter_keyboard_keysym (self->delegate, keysym, state);
#endif /* HAVE_LIBEI */
    }
  else
    {
      // TODO: XDP_KEY_PRESSED/XDP_KEY_RELEASED
      xdp_session_keyboard_key (self->session, TRUE, keysym, state);
    }
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

  if (self->delegate != NULL)
    {
#ifdef HAVE_LIBEI
      valent_input_adapter_pointer_axis (self->delegate, dx, dy);
#endif /* HAVE_LIBEI */
    }
  else
    {
      xdp_session_pointer_axis (self->session, FALSE, dx, dy);
      xdp_session_pointer_axis (self->session, TRUE, 0.0, 0.0);
    }
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

  if (self->delegate != NULL)
    {
#ifdef HAVE_LIBEI
      valent_input_adapter_pointer_button (self->delegate, button, pressed);
#endif /* HAVE_LIBEI */
    }
  else
    {
      button = translate_to_evdev_button (button);
      xdp_session_pointer_button (self->session, button, pressed);
    }
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

  if (self->delegate != NULL)
    {
#ifdef HAVE_LIBEI
      valent_input_adapter_pointer_motion (self->delegate, dx, dy);
#endif /* HAVE_LIBEI */
    }
  else
    {
      xdp_session_pointer_motion (self->session, dx, dy);
    }
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

  if (self->delegate != NULL)
    g_clear_object (&self->delegate);

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

