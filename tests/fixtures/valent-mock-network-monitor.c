// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>

#include "valent-mock-network-monitor.h"

struct _ValentMockNetworkMonitor
{
  GObject       parent_instance;

  unsigned int  network_available : 1;
  unsigned int  network_metered : 1;
};

static void  g_network_monitor_iface_init (GNetworkMonitorInterface *iface);
static void  g_initable_iface_init        (GInitableIface           *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMockNetworkMonitor, valent_mock_network_monitor, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_NETWORK_MONITOR, g_network_monitor_iface_init)
                         g_io_extension_point_implement (G_NETWORK_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "valent",
                                                         0))

typedef enum
{
  PROP_NETWORK_AVAILABLE = 1,
  PROP_NETWORK_METERED,
  PROP_CONNECTIVITY
} ValentMockNetworkMonitorProperty;

/*
 * GNetworkMonitor
 */
static gboolean
valent_mock_network_monitor_can_reach (GNetworkMonitor     *monitor,
                                       GSocketConnectable  *connectable,
                                       GCancellable        *cancellable,
                                       GError             **error)
{
  ValentMockNetworkMonitor *self = VALENT_MOCK_NETWORK_MONITOR (monitor);

  if (!self->network_available)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NETWORK_UNREACHABLE,
                           "Network unreachable");
      return FALSE;
    }

  if (!self->network_available)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_HOST_UNREACHABLE,
                           "Host unreachable");
      return FALSE;
    }

  return TRUE;
}

static gboolean
valent_mock_network_monitor_can_reach_cb (gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  if (!valent_mock_network_monitor_can_reach (g_task_get_source_object (task),
                                              g_task_get_task_data (task),
                                              g_task_get_cancellable (task),
                                              &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return G_SOURCE_REMOVE;
    }

  g_task_return_boolean (task, TRUE);
  return G_SOURCE_REMOVE;
}

static void
valent_mock_network_monitor_can_reach_async (GNetworkMonitor     *monitor,
                                             GSocketConnectable  *connectable,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GSource) source = NULL;

  task = g_task_new (monitor, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_network_monitor_can_reach_async);
  g_task_set_task_data (task, g_object_ref (connectable), g_object_unref);

  source = g_idle_source_new ();
  g_task_attach_source (task, source, valent_mock_network_monitor_can_reach_cb);
}

static gboolean
valent_mock_network_monitor_can_reach_finish (GNetworkMonitor  *monitor,
                                              GAsyncResult     *result,
                                              GError          **error)
{
  g_return_val_if_fail (g_task_is_valid (result, monitor), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_network_monitor_iface_init (GNetworkMonitorInterface *iface)
{
  iface->can_reach = valent_mock_network_monitor_can_reach;
  iface->can_reach_async = valent_mock_network_monitor_can_reach_async;
  iface->can_reach_finish = valent_mock_network_monitor_can_reach_finish;
}

/*
 * GInitable
 */
static gboolean
valent_mock_network_monitor_initable_init (GInitable     *initable,
                                           GCancellable  *cancellable,
                                           GError       **error)
{
  return TRUE;
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = valent_mock_network_monitor_initable_init;
}

/*
 * GObject
 */
static void
valent_mock_network_monitor_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  ValentMockNetworkMonitor *self = VALENT_MOCK_NETWORK_MONITOR (object);

  switch ((ValentMockNetworkMonitorProperty)prop_id)
    {
    case PROP_NETWORK_AVAILABLE:
      g_value_set_boolean (value, self->network_available);
      break;

    case PROP_NETWORK_METERED:
      g_value_set_boolean (value, self->network_metered);
      break;

    case PROP_CONNECTIVITY:
      g_value_set_enum (value, self->network_available
                                 ? G_NETWORK_CONNECTIVITY_FULL
                                 : G_NETWORK_CONNECTIVITY_LOCAL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
valent_mock_network_monitor_class_init (ValentMockNetworkMonitorClass *monitor_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (monitor_class);

  object_class->get_property = valent_mock_network_monitor_get_property;

  g_object_class_override_property (object_class,
                                    PROP_NETWORK_AVAILABLE,
                                    "network-available");
  g_object_class_override_property (object_class,
                                    PROP_NETWORK_METERED,
                                    "network-metered");
  g_object_class_override_property (object_class,
                                    PROP_CONNECTIVITY,
                                    "connectivity");
}

static void
valent_mock_network_monitor_init (ValentMockNetworkMonitor *self)
{
  self->network_available = TRUE;
  self->network_metered = FALSE;
}

