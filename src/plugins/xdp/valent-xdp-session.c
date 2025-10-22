// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-session"

#include "config.h"

#include <libportal/portal.h>
#include <valent.h>

#include "valent-xdp-session.h"
#include "valent-xdp-utils.h"


struct _ValentXdpSession
{
  ValentSessionAdapter  parent_instance;

  unsigned int          active : 1;
  unsigned int          locked : 1;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentXdpSession, valent_xdp_session, VALENT_TYPE_SESSION_ADAPTER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))


static void
on_session_state_changed (XdpPortal            *portal,
                          gboolean              screensaver_active,
                          XdpLoginSessionState  state,
                          ValentXdpSession     *self)
{
  g_assert (VALENT_IS_XDP_SESSION (self));

  if (self->active != (state == XDP_LOGIN_SESSION_RUNNING))
    {
      self->active = (state == XDP_LOGIN_SESSION_RUNNING);
      g_object_notify (G_OBJECT (self), "active");
    }

  if (self->locked != screensaver_active)
    {
      self->locked = screensaver_active;
      g_object_notify (G_OBJECT (self), "locked");
    }

  xdp_portal_session_monitor_query_end_response (portal);
}

/*
 * ValentSessionAdapter
 */
static gboolean
valent_xdp_session_get_active (ValentSessionAdapter *adapter)
{
  ValentXdpSession *self = VALENT_XDP_SESSION (adapter);

  return self->active;
}

static gboolean
valent_xdp_session_get_locked (ValentSessionAdapter *adapter)
{
  ValentXdpSession *self = VALENT_XDP_SESSION (adapter);

  return self->locked;
}

/*
 * GAsyncInitable
 */
static void
xdp_portal_session_monitor_start_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  XdpPortal *portal = XDP_PORTAL (object);
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentXdpSession *self = g_task_get_source_object (task);
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (VALENT_IS_XDP_SESSION (self));

  if (!xdp_portal_session_monitor_start_finish (portal, result, &error))
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_signal_connect_object (portal,
                           "session-state-changed",
                           G_CALLBACK (on_session_state_changed),
                           self,
                           G_CONNECT_DEFAULT);

  valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                         VALENT_PLUGIN_STATE_ACTIVE,
                                         NULL);
  g_task_return_boolean (task, TRUE);
}

static void
valent_xdp_session_init_async (GAsyncInitable      *initable,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (XdpParent) parent = NULL;

  g_assert (VALENT_IS_XDP_SESSION (initable));

  task = g_task_new (initable, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_xdp_session_init_async);

  parent = valent_xdp_get_parent ();
  xdp_portal_session_monitor_start (valent_xdp_get_default (),
                                    parent,
                                    XDP_SESSION_MONITOR_FLAG_NONE,
                                    cancellable,
                                    xdp_portal_session_monitor_start_cb,
                                    g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_xdp_session_init_async;
}

/*
 * ValentObject
 */
static void
valent_xdp_session_destroy (ValentObject *object)
{
  ValentXdpSession *self = VALENT_XDP_SESSION (object);
  XdpPortal *portal = valent_xdp_get_default ();

  g_signal_handlers_disconnect_by_data (portal, self);
  xdp_portal_session_monitor_stop (portal);

  VALENT_OBJECT_CLASS (valent_xdp_session_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_xdp_session_class_init (ValentXdpSessionClass *klass)
{
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentSessionAdapterClass *session_class = VALENT_SESSION_ADAPTER_CLASS (klass);

  vobject_class->destroy = valent_xdp_session_destroy;

  session_class->get_active = valent_xdp_session_get_active;
  session_class->get_locked = valent_xdp_session_get_locked;
}

static void
valent_xdp_session_init (ValentXdpSession *self)
{
}

