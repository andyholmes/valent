// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-gnome-session"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-session.h>

#include "valent-gnome-session.h"


#define GNOME_SCREENSAVER_NAME "org.gnome.ScreenSaver"
#define GNOME_SCREENSAVER_OBJECT_PATH "/org/gnome/ScreenSaver"
#define GNOME_SCREENSAVER_INTERFACE "org.gnome.ScreenSaver"


struct _ValentGnomeSession
{
  ValentSessionAdapter  parent_instance;

  GDBusProxy           *proxy;
  GCancellable         *cancellable;

  unsigned int          locked : 1;
};

G_DEFINE_TYPE (ValentGnomeSession, valent_gnome_session, VALENT_TYPE_SESSION_ADAPTER)


/*
 * DBus Callbacks
 */
static void
get_active_cb (GDBusProxy         *proxy,
               GAsyncResult       *result,
               ValentGnomeSession *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GVariant) value = NULL;

  g_assert (VALENT_IS_GNOME_SESSION (self));

  if ((reply = g_dbus_proxy_call_finish (proxy, result, &error)) == NULL)
    {
      g_warning ("%s: %s", G_STRFUNC, error->message);
      return;
    }

  value = g_variant_get_child_value (reply, 0);
  self->locked = g_variant_get_boolean (value);

  g_object_notify (G_OBJECT (self), "locked");
  valent_session_adapter_emit_changed (VALENT_SESSION_ADAPTER (self));
}

static void
on_signal (GDBusProxy         *proxy,
           const char         *sender_name,
           const char         *signal_name,
           GVariant           *parameters,
           ValentGnomeSession *self)
{
  gboolean locked;

  if (!g_str_equal (signal_name, "ActiveChanged"))
    return;

  g_variant_get (parameters, "(b)", &locked);
  self->locked = locked;

  g_object_notify (G_OBJECT (self), "locked");
  valent_session_adapter_emit_changed (VALENT_SESSION_ADAPTER (self));
}

/*
 * ValentSessionAdapter
 */
static gboolean
valent_gnome_session_get_active (ValentSessionAdapter *adapter)
{
  ValentGnomeSession *self = VALENT_GNOME_SESSION (adapter);

  g_assert (VALENT_IS_GNOME_SESSION (self));

  return !self->locked;
}

static gboolean
valent_gnome_session_get_locked (ValentSessionAdapter *adapter)
{
  ValentGnomeSession *self = VALENT_GNOME_SESSION (adapter);

  g_assert (VALENT_IS_GNOME_SESSION (self));

  return self->locked;
}

static void
valent_gnome_session_set_locked (ValentSessionAdapter *adapter,
                                 gboolean              state)
{
  ValentGnomeSession *self = VALENT_GNOME_SESSION (adapter);

  g_assert (VALENT_IS_GNOME_SESSION (self));

  if (self->proxy == NULL)
    return;

  g_dbus_proxy_call (self->proxy,
                     "SetActive",
                     g_variant_new ("(b)", state),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);

  if (state)
    g_dbus_proxy_call (self->proxy,
                       "Lock",
                       NULL,
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       NULL,
                       NULL,
                       NULL);
}

static void
new_proxy_cb (GObject            *object,
              GAsyncResult       *result,
              ValentGnomeSession *self)
{
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_GNOME_SESSION (self));

  if ((self->proxy = g_dbus_proxy_new_for_bus_finish (result, &error)) == NULL)
    {
      g_warning ("%s: %s", G_STRFUNC, error->message);
      return;
    }

  g_dbus_proxy_call (self->proxy,
                     "GetActive",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     (GAsyncReadyCallback)get_active_cb,
                     self);

  g_signal_connect (self->proxy,
                    "g-signal",
                    G_CALLBACK (on_signal),
                    self);
}


/*
 * GObject
 */
static void
valent_gnome_session_constructed (GObject *object)
{
  ValentGnomeSession *self = VALENT_GNOME_SESSION (object);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            GNOME_SCREENSAVER_NAME,
                            GNOME_SCREENSAVER_OBJECT_PATH,
                            GNOME_SCREENSAVER_INTERFACE,
                            self->cancellable,
                            (GAsyncReadyCallback)new_proxy_cb,
                            self);

  G_OBJECT_CLASS (valent_gnome_session_parent_class)->constructed (object);
}

static void
valent_gnome_session_dispose (GObject *object)
{
  ValentGnomeSession *self = VALENT_GNOME_SESSION (object);

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  if (self->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->proxy, self);
      g_clear_object (&self->proxy);
    }

  G_OBJECT_CLASS (valent_gnome_session_parent_class)->dispose (object);
}

static void
valent_gnome_session_finalize (GObject *object)
{
  ValentGnomeSession *self = VALENT_GNOME_SESSION (object);

  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (valent_gnome_session_parent_class)->finalize (object);
}

static void
valent_gnome_session_class_init (ValentGnomeSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentSessionAdapterClass *session_class = VALENT_SESSION_ADAPTER_CLASS (klass);

  object_class->constructed = valent_gnome_session_constructed;
  object_class->dispose = valent_gnome_session_dispose;
  object_class->finalize = valent_gnome_session_finalize;

  session_class->get_active = valent_gnome_session_get_active;
  session_class->get_locked = valent_gnome_session_get_locked;
  session_class->set_locked = valent_gnome_session_set_locked;
}

static void
valent_gnome_session_init (ValentGnomeSession *self)
{
  self->cancellable = g_cancellable_new ();
}

