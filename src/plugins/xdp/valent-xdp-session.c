// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-session"

#include "config.h"

#include <libportal/portal.h>
#include <libvalent-session.h>

#include "valent-xdp-session.h"
#include "valent-xdp-utils.h"


struct _ValentXdpSession
{
  ValentSessionAdapter  parent_instance;

  unsigned int          active : 1;
  unsigned int          locked : 1;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentXdpSession, valent_xdp_session, VALENT_TYPE_SESSION_ADAPTER,
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
    return g_task_return_error (task, g_steal_pointer (&error));

  g_signal_connect_object (portal,
                           "session-state-changed",
                           G_CALLBACK (on_session_state_changed),
                           self, 0);
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
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (XdpParent) parent = NULL;

  g_assert (VALENT_IS_XDP_SESSION (initable));

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_attach_cancellable (VALENT_OBJECT (initable),
                                              cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_xdp_session_init_async);

  parent = valent_xdp_get_parent (NULL);
  xdp_portal_session_monitor_start (valent_xdp_get_default (),
                                    parent,
                                    XDP_SESSION_MONITOR_FLAG_NONE,
                                    destroy,
                                    xdp_portal_session_monitor_start_cb,
                                    g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_xdp_session_init_async;
}

/*
 * GObject
 */
static void
valent_xdp_session_dispose (GObject *object)
{
  ValentXdpSession *self = VALENT_XDP_SESSION (object);
  XdpPortal *portal = valent_xdp_get_default ();

  g_signal_handlers_disconnect_by_data (portal, self);
  xdp_portal_session_monitor_stop (portal);

  G_OBJECT_CLASS (valent_xdp_session_parent_class)->dispose (object);
}

static void
valent_xdp_session_class_init (ValentXdpSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentSessionAdapterClass *session_class = VALENT_SESSION_ADAPTER_CLASS (klass);

  object_class->dispose = valent_xdp_session_dispose;

  session_class->get_active = valent_xdp_session_get_active;
  session_class->get_locked = valent_xdp_session_get_locked;
}

static void
valent_xdp_session_init (ValentXdpSession *self)
{
  self->active = TRUE;
  self->locked = FALSE;
}

