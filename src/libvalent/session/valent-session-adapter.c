// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-session-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-session-adapter.h"


/**
 * ValentSessionAdapter:
 *
 * An abstract base class for session managers.
 *
 * #ValentSessionAdapter is a base class for plugins that provide an interface
 * to the desktop session manager. This usually means monitoring the idle state,
 * locking and unlocking the session.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-SessionAdapterPriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
} ValentSessionAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentSessionAdapter, valent_session_adapter, VALENT_TYPE_OBJECT)

/**
 * ValentSessionAdapterClass:
 * @get_active: the virtual function pointer for valent_session_adapter_get_active()
 * @get_locked: the virtual function pointer for valent_session_adapter_get_locked()
 * @set_locked: the virtual function pointer for valent_session_adapter_set_locked()
 *
 * The virtual function table for #ValentSessionAdapter.
 */

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_LOCKED,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/* LCOV_EXCL_START */
static gboolean
valent_session_adapter_real_get_active (ValentSessionAdapter *adapter)
{
  return FALSE;
}

static gboolean
valent_session_adapter_real_get_locked (ValentSessionAdapter *adapter)
{
  return FALSE;
}

static void
valent_session_adapter_real_set_locked (ValentSessionAdapter *adapter,
                                        gboolean              state)
{
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_session_adapter_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ValentSessionAdapter *self = VALENT_SESSION_ADAPTER (object);
  ValentSessionAdapterPrivate *priv = valent_session_adapter_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, valent_session_adapter_get_active (self));
      break;

    case PROP_LOCKED:
      g_value_set_boolean (value, valent_session_adapter_get_locked (self));
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_session_adapter_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ValentSessionAdapter *self = VALENT_SESSION_ADAPTER (object);
  ValentSessionAdapterPrivate *priv = valent_session_adapter_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_LOCKED:
      valent_session_adapter_set_locked (self, g_value_get_boolean (value));
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_session_adapter_class_init (ValentSessionAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_session_adapter_get_property;
  object_class->set_property = valent_session_adapter_set_property;

  klass->get_active = valent_session_adapter_real_get_active;
  klass->get_locked = valent_session_adapter_real_get_locked;
  klass->set_locked = valent_session_adapter_real_set_locked;

  /**
   * ValentSessionAdapter:active: (getter get_active)
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
   * ValentSessionAdapter:locked: (getter get_locked) (setter set_locked)
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

  /**
   * ValentSessionAdapter:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this adapter.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info", NULL, NULL,
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_session_adapter_init (ValentSessionAdapter *adapter)
{
}

/**
 * valent_session_adapter_get_active: (virtual get_active) (get-property active)
 * @adapter: a #ValentSessionAdapter
 *
 * Get whether the session is active.
 *
 * Returns: %TRUE if active, %FALSE if idle
 *
 * Since: 1.0
 */
gboolean
valent_session_adapter_get_active (ValentSessionAdapter *adapter)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_SESSION_ADAPTER (adapter), FALSE);

  ret = VALENT_SESSION_ADAPTER_GET_CLASS (adapter)->get_active (adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_session_adapter_get_locked: (virtual get_locked) (get-property locked)
 * @adapter: a #ValentSessionAdapter
 *
 * Get whether the session is locked.
 *
 * Returns: %TRUE if locked, %FALSE if unlocked
 *
 * Since: 1.0
 */
gboolean
valent_session_adapter_get_locked (ValentSessionAdapter *adapter)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_SESSION_ADAPTER (adapter), FALSE);

  ret = VALENT_SESSION_ADAPTER_GET_CLASS (adapter)->get_locked (adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_session_adapter_set_locked: (virtual set_locked) (set-property locked)
 * @adapter: a #ValentSessionAdapter
 * @state: %TRUE to lock, %FALSE to unlock
 *
 * Set whether the session is locked.
 *
 * Since: 1.0
 */
void
valent_session_adapter_set_locked (ValentSessionAdapter *adapter,
                                   gboolean              state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_SESSION_ADAPTER (adapter));

  VALENT_SESSION_ADAPTER_GET_CLASS (adapter)->set_locked (adapter, state);

  VALENT_EXIT;
}

