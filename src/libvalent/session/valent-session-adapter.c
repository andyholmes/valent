// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-session-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-session-adapter.h"


/**
 * SECTION:valentsessionadapter
 * @short_description: Interface for session adapters
 * @title: ValentSessionAdapter
 * @stability: Unstable
 * @include: libvalent-session.h
 *
 * The #ValentSessionAdapter interface should be implemented by libpeas
 * plugins that operate at the desktop level. This generally means providing
 * access to the desktop session.
 *
 * ## .plugin File ##
 *
 * Session adapter require no special entries in the `.plugin` file.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
} ValentSessionAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentSessionAdapter, valent_session_adapter, G_TYPE_OBJECT)

/**
 * ValentSessionAdapterClass:
 * @changed: class closure for #ValentSessionAdapter::changed signal
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

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


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
   * ValentSessionAdapter:active:
   *
   * Whether the session is active.
   */
  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          "Active",
                          "Whether the session is active",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentSessionAdapter:locked:
   *
   * Whether the session is locked.
   */
  properties [PROP_LOCKED] =
    g_param_spec_boolean ("locked",
                          "Locked",
                          "Whether the session is locked",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentSessionAdapter:plugin-info:
   *
   * The #PeasPluginInfo describing this adapter.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "Plugin Info",
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentSessionAdapter::changed:
   * @adapter: a #ValentSessionAdapter
   *
   * #ValentSessionAdapter::changed is emitted when @adapter changes.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentSessionAdapterClass, changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
valent_session_adapter_init (ValentSessionAdapter *adapter)
{
}

/**
 * valent_session_adapter_emit_changed:
 * @adapter: a #ValentSessionAdapter
 *
 * Emits the #ValentSessionAdapter::changed signal on @adapter.
 */
void
valent_session_adapter_emit_changed (ValentSessionAdapter *adapter)
{
  g_return_if_fail (VALENT_IS_SESSION_ADAPTER (adapter));

  g_signal_emit (G_OBJECT (adapter), signals [CHANGED], 0);
}

/**
 * valent_session_adapter_get_active:
 * @adapter: a #ValentSessionAdapter
 *
 * Get the active state of @adapter.
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
 * valent_session_adapter_get_locked:
 * @adapter: a #ValentSessionAdapter
 *
 * Get the locked state of @adapter.
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
 * valent_session_adapter_set_locked:
 * @adapter: a #ValentSessionAdapter
 * @state: locked state
 *
 * Set the locked state of @adapter to @state.
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

