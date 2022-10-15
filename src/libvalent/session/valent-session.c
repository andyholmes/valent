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

  ValentSessionAdapter *default_adapter;
};

G_DEFINE_TYPE (ValentSession, valent_session, VALENT_TYPE_COMPONENT)

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_LOCKED,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static ValentSession *default_adapter = NULL;


static void
on_active_changed (ValentSessionAdapter *adapter,
                   GParamSpec           *pspec,
                   ValentSession        *self)
{
  VALENT_ENTRY;

  if (self->default_adapter == adapter)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE]);

  VALENT_EXIT;
}

static void
on_locked_changed (ValentSessionAdapter *adapter,
                   GParamSpec           *pspec,
                   ValentSession        *self)
{
  VALENT_ENTRY;

  if (self->default_adapter == adapter)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOCKED]);

  VALENT_EXIT;
}

/*
 * ValentComponent
 */
static void
valent_session_bind_extension (ValentComponent *component,
                               PeasExtension   *extension)
{
  ValentSession *self = VALENT_SESSION (component);
  ValentSessionAdapter *adapter = VALENT_SESSION_ADAPTER (extension);
  PeasExtension *new_primary = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_SESSION (self));
  g_assert (VALENT_IS_SESSION_ADAPTER (adapter));

  g_signal_connect_object (adapter,
                           "notify::active",
                           G_CALLBACK (on_active_changed),
                           component, 0);
  g_signal_connect_object (adapter,
                           "notify::locked",
                           G_CALLBACK (on_locked_changed),
                           component, 0);

  /* Set default provider */
  new_primary = valent_component_get_primary (component);
  self->default_adapter = VALENT_SESSION_ADAPTER (new_primary);

  VALENT_EXIT;
}

static void
valent_session_unbind_extension (ValentComponent *component,
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
valent_session_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ValentSession *self = VALENT_SESSION (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, valent_session_get_active (self));
      break;

    case PROP_LOCKED:
      g_value_set_boolean (value, valent_session_get_locked (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_session_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ValentSession *self = VALENT_SESSION (object);

  switch (prop_id)
    {
    case PROP_LOCKED:
      valent_session_set_locked (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_session_class_init (ValentSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->get_property = valent_session_get_property;
  object_class->set_property = valent_session_set_property;

  component_class->bind_extension = valent_session_bind_extension;
  component_class->unbind_extension = valent_session_unbind_extension;

  /**
   * ValentSession:active: (getter get_active)
   *
   * Whether the session is active.
   *
   * Since: 1.0
   */
  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentSession:locked: (getter get_locked) (setter set_locked)
   *
   * Whether the session is locked.
   *
   * Since: 1.0
   */
  properties [PROP_LOCKED] =
    g_param_spec_boolean ("locked", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

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
 * valent_session_get_active: (get-property active)
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
 * valent_session_get_locked: (get-property locked)
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
 * valent_session_set_locked: (set-property locked)
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

