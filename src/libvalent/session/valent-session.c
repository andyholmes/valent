// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-session"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-session.h"
#include "valent-session-adapter.h"


/**
 * SECTION:valentsession
 * @short_description: Session Abstraction
 * @title: ValentSession
 * @stability: Unstable
 * @include: libvalent-session.h
 *
 * #ValentSession is an aggregator of desktop sessions, with a simple
 * API generally intended to be used by #ValentDevicePlugin implementations.
 *
 * Plugins can provide adapters for session selections by subclassing the
 * #ValentSeesionAdapter base class. The priority of session adapters is
 * determined by the `.plugin` file key `X-SessionAdapterPriority`.
 */

struct _ValentSession
{
  ValentComponent       parent_instance;

  GCancellable         *cancellable;
  ValentSessionAdapter *default_adapter;
};

G_DEFINE_TYPE (ValentSession, valent_session, VALENT_TYPE_COMPONENT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static ValentSession *default_adapter = NULL;


static void
on_session_adapter_changed (ValentSessionAdapter *adapter,
                            ValentSession        *self)
{
  VALENT_ENTRY;

  if (self->default_adapter == adapter)
    g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);

  VALENT_EXIT;
}

/*
 * ValentComponent
 */
static void
valent_session_extension_added (ValentComponent *component,
                                PeasExtension   *extension)
{
  ValentSession *self = VALENT_SESSION (component);
  ValentSessionAdapter *adapter = VALENT_SESSION_ADAPTER (extension);
  PeasExtension *provider;

  VALENT_ENTRY;

  g_assert (VALENT_IS_SESSION (self));

  /* Watch for changes */
  g_signal_connect_object (adapter,
                           "changed",
                           G_CALLBACK (on_session_adapter_changed),
                           component, 0);

  /* Set default provider */
  provider = valent_component_get_priority_provider (component,
                                                     "SessionAdapterPriority");

  if ((PeasExtension *)self->default_adapter != provider)
    g_set_object (&self->default_adapter, VALENT_SESSION_ADAPTER (provider));

  VALENT_EXIT;
}

static void
valent_session_extension_removed (ValentComponent *component,
                                  PeasExtension   *extension)
{
  ValentSession *self = VALENT_SESSION (component);
  ValentSessionAdapter *adapter = VALENT_SESSION_ADAPTER (extension);
  PeasExtension *provider;

  VALENT_ENTRY;

  g_assert (VALENT_IS_SESSION (self));

  /* Stop watching changes */
  g_signal_handlers_disconnect_by_data (adapter, self);

  /* Set default provider */
  provider = valent_component_get_priority_provider (component,
                                                     "SessionAdapterPriority");

  if ((PeasExtension *)self->default_adapter == extension)
    g_set_object (&self->default_adapter, VALENT_SESSION_ADAPTER (provider));

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_session_dispose (GObject *object)
{
  ValentSession *self = VALENT_SESSION (object);

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (valent_session_parent_class)->dispose (object);
}

static void
valent_session_finalize (GObject *object)
{
  ValentSession *self = VALENT_SESSION (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->default_adapter);

  G_OBJECT_CLASS (valent_session_parent_class)->finalize (object);
}

static void
valent_session_class_init (ValentSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->dispose = valent_session_dispose;
  object_class->finalize = valent_session_finalize;

  component_class->extension_added = valent_session_extension_added;
  component_class->extension_removed = valent_session_extension_removed;

  /**
   * ValentSession::changed:
   * @session: a #ValentSession
   *
   * #ValentSession::changed is emitted when the content of the default #ValentSessionAdapter
   * changes.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
valent_session_init (ValentSession *self)
{
  self->cancellable = g_cancellable_new ();
}


/**
 * valent_session_get_active:
 * @session: a #ValentSession
 *
 * Get the active state of @session.
 *
 * Returns: the idle state
 */
gboolean
valent_session_get_active (ValentSession *session)
{
  gboolean ret = FALSE;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_SESSION (session), FALSE);

  if G_LIKELY (session->default_adapter != NULL)
    ret = valent_session_adapter_get_active (session->default_adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_session_get_locked:
 * @session: a #ValentSession
 *
 * Get the locked state of @session.
 *
 * Returns: the locked state
 */
gboolean
valent_session_get_locked (ValentSession *session)
{
  gboolean ret = FALSE;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_SESSION (session), FALSE);

  if G_LIKELY (session->default_adapter != NULL)
    ret = valent_session_adapter_get_locked (session->default_adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_session_set_locked:
 * @session: a #ValentSession
 * @state: locked state
 *
 * Set the locked state of @session to @state.
 */
void
valent_session_set_locked (ValentSession *session,
                           gboolean       state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_SESSION (session));

  if G_LIKELY (session->default_adapter != NULL)
    valent_session_adapter_set_locked (session->default_adapter, state);

  VALENT_EXIT;
}

/**
 * valent_session_get_default:
 *
 * Get the default #ValentSession.
 *
 * Returns: (transfer none): The default session
 */
ValentSession *
valent_session_get_default (void)
{
  if (default_adapter == NULL)
    {
      default_adapter = g_object_new (VALENT_TYPE_SESSION,
                                      "plugin-context", "session",
                                      "plugin-type",    VALENT_TYPE_SESSION_ADAPTER,
                                      NULL);

      g_object_add_weak_pointer (G_OBJECT (default_adapter),
                                 (gpointer)&default_adapter);
    }

  return default_adapter;
}

