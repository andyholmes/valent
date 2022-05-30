// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-session"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-component-private.h"
#include "valent-session.h"
#include "valent-session-adapter.h"


/**
 * ValentSession:
 *
 * A class for monitoring the session state.
 *
 * #ValentSession is an abstraction of session managers, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.SessionAdapter] to provide an interface
 * to monitor and control the session state.
 *
 * Since: 1.0
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
valent_session_enable_extension (ValentComponent *component,
                                 PeasExtension   *extension)
{
  ValentSession *self = VALENT_SESSION (component);
  ValentSessionAdapter *adapter = VALENT_SESSION_ADAPTER (extension);
  PeasExtension *new_primary = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_SESSION (self));
  g_assert (VALENT_IS_SESSION_ADAPTER (adapter));

  /* Watch for changes */
  g_signal_connect_object (adapter,
                           "changed",
                           G_CALLBACK (on_session_adapter_changed),
                           component, 0);

  /* Set default provider */
  new_primary = valent_component_get_primary (component);
  self->default_adapter = VALENT_SESSION_ADAPTER (new_primary);

  VALENT_EXIT;
}

static void
valent_session_disable_extension (ValentComponent *component,
                                  PeasExtension   *extension)
{
  ValentSession *self = VALENT_SESSION (component);
  ValentSessionAdapter *adapter = VALENT_SESSION_ADAPTER (extension);
  PeasExtension *new_primary = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_SESSION (self));
  g_assert (VALENT_IS_SESSION_ADAPTER (adapter));

  /* Stop watching changes */
  g_signal_handlers_disconnect_by_data (adapter, self);

  /* Set default provider */
  new_primary = valent_component_get_primary (component);
  self->default_adapter = VALENT_SESSION_ADAPTER (new_primary);

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

  G_OBJECT_CLASS (valent_session_parent_class)->finalize (object);
}

static void
valent_session_class_init (ValentSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->dispose = valent_session_dispose;
  object_class->finalize = valent_session_finalize;

  component_class->enable_extension = valent_session_enable_extension;
  component_class->disable_extension = valent_session_disable_extension;

  /**
   * ValentSession::changed:
   * @session: a #ValentSession
   *
   * Emitted when the state of the primary [class@Valent.SessionAdapter]
   * changes.
   *
   * Since: 1.0
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
 * valent_session_get_default:
 *
 * Get the default [class@Valent.Session].
 *
 * Returns: (transfer none) (nullable): a #ValentSession
 *
 * Since: 1.0
 */
ValentSession *
valent_session_get_default (void)
{
  if (default_adapter == NULL)
    {
      default_adapter = g_object_new (VALENT_TYPE_SESSION,
                                      "plugin-context",  "session",
                                      "plugin-priority", "SessionAdapterPriority",
                                      "plugin-type",     VALENT_TYPE_SESSION_ADAPTER,
                                      NULL);

      g_object_add_weak_pointer (G_OBJECT (default_adapter),
                                 (gpointer)&default_adapter);
    }

  return default_adapter;
}

/**
 * valent_session_get_active:
 * @session: a #ValentSession
 *
 * Get the active state of the primary [class@Valent.SessionAdapter].
 *
 * Returns: %TRUE if the session is active, or %FALSE if not
 *
 * Since: 1.0
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
 * Get the locked state of the primary [class@Valent.SessionAdapter].
 *
 * Returns: %TRUE if the session is locked, or %FALSE if unlocked
 *
 * Since: 1.0
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
 * @state: %TRUE to lock, or %FALSE to unlock
 *
 * Set the locked state of the primary [class@Valent.SessionAdapter].
 *
 * Since: 1.0
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

