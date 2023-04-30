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
#include <valent.h>

#include "valent-mutter-input.h"

#define SERVICE_NAME "org.gnome.Shell"
#define SERVICE_PATH "/org/gnome/Mutter/RemoteDesktop"
#define SERVICE_IFACE "org.gnome.Mutter.RemoteDesktop"

#define SESSION_NAME "org.gnome.Shell"
#define SESSION_IFACE "org.gnome.Mutter.RemoteDesktop.Session"


struct _ValentMutterInput
{
  ValentInputAdapter  parent_instance;

  GDBusProxy         *proxy;
  GDBusProxy         *session;
  uint8_t             session_state : 2;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMutterInput, valent_mutter_input, VALENT_TYPE_INPUT_ADAPTER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init));

enum {
  SESSION_STATE_CLOSED,
  SESSION_STATE_STARTING = (1 << 0),
  SESSION_STATE_ACTIVE  =  (1 << 1),
};

/*< private >
 * KeyboardKeyState:
 * @KEYBOARD_KEY_RELEASED: The key is pressed
 * @KEYBOARD_KEY_PRESSED: The key is released
 *
 * Enumeration of keyboard bey states.
 */
typedef enum {
  KEYBOARD_KEY_RELEASED,
  KEYBOARD_KEY_PRESSED,
} KeyboardKeyState;

/*< private >
 * PointerAxisFlags:
 * @POINTER_AXIS_FINISH: scroll motion was finished (e.g. fingers lifted)
 * @POINTER_AXIS_WHEEL: The scroll event is originated by a mouse wheel.
 * @POINTER_AXIS_TOUCH: The scroll event is originated by one or more fingers
 *                       on the device (eg. touchpads).
 * @POINTER_AXIS_CONTINUOUS: The scroll event is originated by the motion of
 *                           some device (eg. a scroll button is set).
 *
 * Flags for pointer axis events.
 */
typedef enum {
  POINTER_AXIS_NONE,
  POINTER_AXIS_FINISH     = (1 << 0),
  POINTER_AXIS_WHEEL      = (1 << 1),
  POINTER_AXIS_TOUCH      = (1 << 2),
  POINTER_AXIS_CONTINUOUS = (1 << 3),
} PointerAxisFlags;

/*< private >
 * PointerAxisOrientation:
 * @POINTER_AXIS_HORIZONTAL: The x-axis
 * @POINTER_AXIS_VERTICAL: The y-axis
 *
 * Enumeration of pointer axis.
 */
typedef enum {
  POINTER_AXIS_HORIZONTAL,
  POINTER_AXIS_VERTICAL,
} PointerAxisOrientation;


/*
 * org.gnome.Mutter.RemoteDesktop.Session Callbacks
 */
static void
on_g_signal (GDBusProxy        *proxy,
             const char        *sender_name,
             const char        *signal_name,
             GVariant          *parameters,
             ValentMutterInput *self)
{
  g_assert (G_IS_DBUS_PROXY (proxy));
  g_assert (sender_name != NULL && *sender_name != '\0');
  g_assert (VALENT_MUTTER_INPUT (self));

  /* This is the only signal relevant to this adapter */
  if (g_str_equal (signal_name, "Closed"))
    {
      g_signal_handlers_disconnect_by_func (proxy, self, on_g_signal);

      if (self->session == proxy)
        {
          g_clear_object (&self->session);
          self->session_state = SESSION_STATE_CLOSED;
        }
    }
}

static void
remote_desktop_start_screencast_cb (GDBusConnection   *connection,
                                    GAsyncResult      *result,
                                    ValentMutterInput *self)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (G_IS_TASK (result));

  if ((reply = g_dbus_connection_call_finish (connection, result, &error)) == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("%s(): %s", G_STRFUNC, error->message);

      g_clear_object (&self->session);
      self->session_state = SESSION_STATE_CLOSED;
      return;
    }

  self->session_state = SESSION_STATE_ACTIVE;
}

static void
remote_desktop_start_session_cb (GDBusProxy        *proxy,
                                 GAsyncResult      *result,
                                 ValentMutterInput *self)
{
  g_autoptr (GVariant) reply = NULL;
  GDBusConnection *connection = NULL;
  GVariantBuilder options;
  g_autoptr (GVariant) session_id = NULL;
  GCancellable *destroy = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_DBUS_PROXY (proxy));
  g_assert (G_IS_TASK (result));

  if ((reply = g_dbus_proxy_call_finish (proxy, result, &error)) == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      g_clear_object (&self->session);
      self->session_state = SESSION_STATE_CLOSED;
      return;
    }

  session_id = g_dbus_proxy_get_cached_property (self->session, "SessionId");
  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "disable-animations",
                         g_variant_new_boolean (FALSE));
  g_variant_builder_add (&options, "{sv}", "remote-desktop-session-id",
                         session_id);

  connection = g_dbus_proxy_get_connection (self->session);
  destroy = g_task_get_cancellable (G_TASK (result));
  g_dbus_connection_call (connection,
                          "org.gnome.Mutter.ScreenCast",
                          "/org/gnome/Mutter/ScreenCast",
                          "org.gnome.Mutter.ScreenCast",
                          "CreateSession",
                          g_variant_new ("(a{sv})", &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          destroy,
                          (GAsyncReadyCallback)remote_desktop_start_screencast_cb,
                          self);
}

static void
remote_desktop_create_proxy_cb (GDBusProxy        *proxy,
                                GAsyncResult      *result,
                                ValentMutterInput *self)
{
  GCancellable *destroy = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_DBUS_PROXY (proxy));
  g_assert (G_IS_TASK (result));

  if ((self->session = g_dbus_proxy_new_for_bus_finish (result, &error)) == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      self->session_state = SESSION_STATE_CLOSED;
      return;
    }

  g_signal_connect_object (self->session,
                           "g-signal",
                           G_CALLBACK (on_g_signal),
                           self, 0);

  destroy = g_task_get_cancellable (G_TASK (result));
  g_dbus_proxy_call (self->session,
                     "Start",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     destroy,
                     (GAsyncReadyCallback)remote_desktop_start_session_cb,
                     self);
}

static void
remote_desktop_create_session_cb (GDBusProxy        *proxy,
                                  GAsyncResult      *result,
                                  ValentMutterInput *self)
{
  g_autoptr (GVariant) reply = NULL;
  const char *object_path = NULL;
  GCancellable *destroy = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_DBUS_PROXY (proxy));
  g_assert (G_IS_TASK (result));

  if ((reply = g_dbus_proxy_call_finish (proxy, result, &error)) == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("%s(): %s", G_STRFUNC, error->message);
      self->session_state = SESSION_STATE_CLOSED;

      return;
    }

  g_return_if_fail (g_variant_is_of_type (reply, G_VARIANT_TYPE ("(o)")));
  g_variant_get (reply, "(&o)", &object_path);

  destroy = g_task_get_cancellable (G_TASK (result));
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            SESSION_NAME,
                            object_path,
                            SESSION_IFACE,
                            destroy,
                            (GAsyncReadyCallback)remote_desktop_create_proxy_cb,
                            self);
}

static inline gboolean
valent_mutter_input_check (ValentMutterInput *self)
{
  g_autoptr (GCancellable) destroy = NULL;

  if G_LIKELY (self->session_state == SESSION_STATE_ACTIVE)
    return TRUE;

  if (self->session_state == SESSION_STATE_STARTING)
    return FALSE;

  self->session_state = SESSION_STATE_STARTING;
  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  g_dbus_proxy_call (self->proxy,
                     "CreateSession",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     destroy,
                     (GAsyncReadyCallback)remote_desktop_create_session_cb,
                     self);

  return FALSE;
}

static void
valent_mutter_input_close (ValentMutterInput *self)
{
  if (self->session_state == SESSION_STATE_CLOSED)
    return;

  g_dbus_proxy_call (self->session,
                     "Stop",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL, NULL, NULL);
  self->session_state = SESSION_STATE_CLOSED;
}


/*
 * ValentInputAdapter
 */
static void
valent_mutter_input_keyboard_keysym (ValentInputAdapter *adapter,
                                     unsigned int        keysym,
                                     gboolean            state)
{
  ValentMutterInput *self = VALENT_MUTTER_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MUTTER_INPUT (self));

  if G_UNLIKELY (!valent_mutter_input_check (self))
    return;

  // TODO: XDP_KEY_PRESSED/XDP_KEY_RELEASED
  g_dbus_proxy_call (self->session,
                     "NotifyKeyboardKeysym",
                     g_variant_new ("(ub)", keysym, state),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL, NULL, NULL);
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
valent_mutter_input_pointer_axis (ValentInputAdapter *adapter,
                                  double              dx,
                                  double              dy)
{
  ValentMutterInput *self = VALENT_MUTTER_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MUTTER_INPUT (self));
  g_assert (!G_APPROX_VALUE (dx, 0.0, 0.01) || !G_APPROX_VALUE (dy, 0.0, 0.01));

  if G_UNLIKELY (!valent_mutter_input_check (self))
    return;

  g_dbus_proxy_call (self->session,
                     "NotifyPointerAxis",
                     g_variant_new ("(ddu)", dx, dy, POINTER_AXIS_TOUCH),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL, NULL, NULL);
  g_dbus_proxy_call (self->session,
                     "NotifyPointerAxis",
                     g_variant_new ("(ddu)", 0.0, 0.0, POINTER_AXIS_FINISH),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL, NULL, NULL);
}

static void
valent_mutter_input_pointer_button (ValentInputAdapter *adapter,
                                    unsigned int        button,
                                    gboolean            pressed)
{
  ValentMutterInput *self = VALENT_MUTTER_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MUTTER_INPUT (self));

  if G_UNLIKELY (!valent_mutter_input_check (self))
    return;

  /* Translate the button to EVDEV constant */
  button = translate_to_evdev_button (button);
  g_dbus_proxy_call (self->session,
                     "NotifyPointerButton",
                     g_variant_new ("(ib)", (int32_t)button, pressed),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL, NULL, NULL);
}

static void
valent_mutter_input_pointer_motion (ValentInputAdapter *adapter,
                                    double              dx,
                                    double              dy)
{
  ValentMutterInput *self = VALENT_MUTTER_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MUTTER_INPUT (self));

  if G_UNLIKELY (!valent_mutter_input_check (self))
    return;

  g_dbus_proxy_call (self->session,
                     "NotifyPointerMotionRelative",
                     g_variant_new ("(dd)", dx, dy),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL, NULL, NULL);
}

/*
 * GAsyncInitable
 */
static void
on_name_owner_changed (GDBusProxy        *proxy,
                       GParamSpec        *pspec,
                       ValentMutterInput *self)
{
  g_autofree char *name_owner = NULL;

  g_assert (VALENT_IS_MUTTER_INPUT (self));

  if ((name_owner = g_dbus_proxy_get_name_owner (proxy)) != NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ACTIVE,
                                             NULL);
    }
  else
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_INACTIVE,
                                             NULL);
    }
}

static void
g_dbus_proxy_new_for_bus_cb (GDBusProxy   *proxy,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentMutterInput *self = g_task_get_source_object (task);
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if ((self->proxy = g_dbus_proxy_new_for_bus_finish (result, &error)) == NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  g_signal_connect_object (self->proxy,
                           "notify::g-name-owner",
                           G_CALLBACK (on_name_owner_changed),
                           self, 0);
  on_name_owner_changed (self->proxy, NULL, self);

  g_task_return_boolean (task, TRUE);
}

static void
valent_mutter_input_init_async (GAsyncInitable      *initable,
                                int                  io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_MUTTER_INPUT (initable));

  /* Cede the primary position until complete */
  valent_extension_plugin_state_changed (VALENT_EXTENSION (initable),
                                         VALENT_PLUGIN_STATE_INACTIVE,
                                         NULL);

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_attach_cancellable (VALENT_OBJECT (initable),
                                              cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_mutter_input_init_async);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            SERVICE_NAME,
                            SERVICE_PATH,
                            SERVICE_IFACE,
                            destroy,
                            (GAsyncReadyCallback)g_dbus_proxy_new_for_bus_cb,
                            g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_mutter_input_init_async;
}


/*
 * GObject
 */
static void
valent_mutter_input_dispose (GObject *object)
{
  ValentMutterInput *self = VALENT_MUTTER_INPUT (object);

  valent_mutter_input_close (self);

  G_OBJECT_CLASS (valent_mutter_input_parent_class)->dispose (object);
}

static void
valent_mutter_input_finalize (GObject *object)
{
  ValentMutterInput *self = VALENT_MUTTER_INPUT (object);

  g_clear_object (&self->proxy);
  g_clear_object (&self->session);

  G_OBJECT_CLASS (valent_mutter_input_parent_class)->finalize (object);
}

static void
valent_mutter_input_class_init (ValentMutterInputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentInputAdapterClass *adapter_class = VALENT_INPUT_ADAPTER_CLASS (klass);

  object_class->dispose = valent_mutter_input_dispose;
  object_class->finalize = valent_mutter_input_finalize;

  adapter_class->keyboard_keysym = valent_mutter_input_keyboard_keysym;
  adapter_class->pointer_axis = valent_mutter_input_pointer_axis;
  adapter_class->pointer_button = valent_mutter_input_pointer_button;
  adapter_class->pointer_motion = valent_mutter_input_pointer_motion;
}

static void
valent_mutter_input_init (ValentMutterInput *self)
{
}


