// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-application-plugin"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-device.h>

#include "valent-application-plugin.h"


/**
 * ValentApplicationPlugin:
 *
 * An abstract base class for application plugins.
 *
 * #ValentApplicationPlugin is a base class for plugins that operate in the
 * scope of the application. This usually means integrating the application with
 * the host environment (eg. XDG Autostart).
 *
 * ## Implementation Notes
 *
 * Implementations may handle application events by overriding the appropriate
 * virtual function, including [vfunc@Valent.ApplicationPlugin.activate] to
 * handle activation, [vfunc@Valent.ApplicationPlugin.command_line] to handle
 * CLI options, or [vfunc@Valent.ApplicationPlugin.open] to handle files.
 *
 * For plugin preferences see [class@Valent.PreferencesPage].
 *
 * ## `.plugin` File
 *
 * Application plugins have no special fields in the `.plugin` file.
 *
 * Since: 1.0
 */

typedef struct
{
  PeasPluginInfo      *plugin_info;
  GApplication        *application;
  ValentDeviceManager *manager;
} ValentApplicationPluginPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentApplicationPlugin, valent_application_plugin, VALENT_TYPE_OBJECT)

/**
 * ValentApplicationPluginClass:
 * @enable: the virtual function pointer for valent_application_plugin_enable()
 * @disable: the virtual function pointer for valent_application_plugin_disable()
 * @activate: the virtual function pointer for valent_application_plugin_activate()
 * @command_line: the virtual function pointer for valent_application_plugin_command_line()
 * @open: the virtual function pointer for valent_application_plugin_open()
 *
 * The virtual function table for #ValentApplicationPlugin.
 */

enum {
  PROP_0,
  PROP_APPLICATION,
  PROP_DEVICE_MANAGER,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/* LCOV_EXCL_START */
static void
valent_application_plugin_real_disable (ValentApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_APPLICATION_PLUGIN (plugin));
}

static void
valent_application_plugin_real_enable (ValentApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_APPLICATION_PLUGIN (plugin));
}

static gboolean
valent_application_plugin_real_activate (ValentApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_APPLICATION_PLUGIN (plugin));

  return FALSE;
}

static int
valent_application_plugin_real_command_line (ValentApplicationPlugin *plugin,
                                             GApplicationCommandLine *command_line)
{
  g_assert (VALENT_IS_APPLICATION_PLUGIN (plugin));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (command_line));

  return 0;
}

static gboolean
valent_application_plugin_real_open (ValentApplicationPlugin  *plugin,
                                     GFile                   **files,
                                     int                       n_files,
                                     const char               *hint)
{
  g_assert (VALENT_IS_APPLICATION_PLUGIN (plugin));
  g_assert (files != NULL);
  g_assert (n_files > 0);
  g_assert (hint != NULL);

  return FALSE;
}
/* LCOV_EXCL_STOP */


/*
 * GObject
 */
static void
valent_application_plugin_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ValentApplicationPlugin *self = VALENT_APPLICATION_PLUGIN (object);
  ValentApplicationPluginPrivate *priv = valent_application_plugin_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_APPLICATION:
      g_value_set_object (value, priv->application);
      break;

    case PROP_DEVICE_MANAGER:
      g_value_set_object (value, priv->manager);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_application_plugin_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ValentApplicationPlugin *self = VALENT_APPLICATION_PLUGIN (object);
  ValentApplicationPluginPrivate *priv = valent_application_plugin_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_APPLICATION:
      priv->application = g_value_get_object (value);
      break;

    case PROP_DEVICE_MANAGER:
      priv->manager = g_value_get_object (value);
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_application_plugin_class_init (ValentApplicationPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_application_plugin_get_property;
  object_class->set_property = valent_application_plugin_set_property;

  klass->disable = valent_application_plugin_real_disable;
  klass->enable = valent_application_plugin_real_enable;
  klass->activate = valent_application_plugin_real_activate;
  klass->command_line = valent_application_plugin_real_command_line;
  klass->open = valent_application_plugin_real_open;

  /**
   * ValentApplicationPlugin:application: (getter get_application):
   *
   * The [class@Gio.Application] this plugin is bound to.
   *
   * Since: 1.0
   */
  properties [PROP_APPLICATION] =
    g_param_spec_object ("application", NULL, NULL,
                         G_TYPE_APPLICATION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentApplicationPlugin:device-manager: (getter get_device_manager):
   *
   * The [class@Valent.DeviceManager] this plugin is bound to.
   *
   * Since: 1.0
   */
  properties [PROP_DEVICE_MANAGER] =
    g_param_spec_object ("device-manager", NULL, NULL,
                         VALENT_TYPE_DEVICE_MANAGER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentApplicationPlugin:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this plugin.
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
valent_application_plugin_init (ValentApplicationPlugin *adapter)
{
}

/**
 * valent_application_plugin_get_application: (get-property application)
 * @plugin: a #ValentApplicationPlugin
 *
 * Get the application this plugin is bound to.
 *
 * Returns: (transfer none): a #GApplication
 *
 * Since: 1.0
 */
GApplication *
valent_application_plugin_get_application (ValentApplicationPlugin *plugin)
{
  ValentApplicationPluginPrivate *priv = valent_application_plugin_get_instance_private (plugin);

  g_return_val_if_fail (VALENT_IS_APPLICATION_PLUGIN (plugin), NULL);

  return priv->application;
}

/**
 * valent_application_plugin_get_device_manager: (get-property device-manager)
 * @plugin: a #ValentApplicationPlugin
 *
 * Get the [class@Valent.DeviceManager] of the application.
 *
 * Returns: (transfer none): a #ValentDeviceManager
 *
 * Since: 1.0
 */
ValentDeviceManager *
valent_application_plugin_get_device_manager (ValentApplicationPlugin *plugin)
{
  ValentApplicationPluginPrivate *priv = valent_application_plugin_get_instance_private (plugin);

  g_return_val_if_fail (VALENT_IS_APPLICATION_PLUGIN (plugin), NULL);

  return priv->manager;
}

/**
 * valent_application_plugin_enable: (virtual enable)
 * @plugin: a #ValentApplicationPlugin
 *
 * Enable the plugin.
 *
 * Implementations should override this method to prepare anything the plugin
 * needs to perform its function.
 *
 * Since: 1.0
 */
void
valent_application_plugin_enable (ValentApplicationPlugin *plugin)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_APPLICATION_PLUGIN (plugin));

  VALENT_APPLICATION_PLUGIN_GET_CLASS (plugin)->enable (plugin);

  VALENT_EXIT;
}

/**
 * valent_application_plugin_disable: (virtual disable)
 * @plugin: a #ValentApplicationPlugin
 *
 * Disable the plugin.
 *
 * Implementations should override this method to cleanup any resources that
 * were allocated in [method@Valent.ApplicationPlugin.enable].
 *
 * Since: 1.0
 */
void
valent_application_plugin_disable (ValentApplicationPlugin *plugin)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_APPLICATION_PLUGIN (plugin));

  VALENT_APPLICATION_PLUGIN_GET_CLASS (plugin)->disable (plugin);

  VALENT_EXIT;
}

/**
 * valent_application_plugin_activate: (virtual activate)
 * @plugin: a #ValentApplicationPlugin
 *
 * Handle activation of the application.
 *
 * Implementations should override this method to handle activation, as
 * a result of [signal@Gio.Application::activate] being emitted on the primary
 * instance of the application.
 *
 * Returns: %TRUE if handled, or %FALSE if not
 *
 * Since: 1.0
 */
gboolean
valent_application_plugin_activate (ValentApplicationPlugin *plugin)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_APPLICATION_PLUGIN (plugin), FALSE);

  ret = VALENT_APPLICATION_PLUGIN_GET_CLASS (plugin)->activate (plugin);

  VALENT_RETURN (ret);
}

/**
 * valent_application_plugin_command_line: (virtual command_line)
 * @plugin: a #ValentApplicationPlugin
 * @command_line: a #GApplicationCommandLine
 *
 * Handle the given command-line options.
 *
 * Implementations should override this method to handle command-line options,
 * as a result of [signal@Gio.Application::command-line] being emitted on the
 * primary instance of the application.
 *
 * Returns: an integer that is set as the exit status for the calling process
 *
 * Since: 1.0
 */
int
valent_application_plugin_command_line (ValentApplicationPlugin *plugin,
                                        GApplicationCommandLine *command_line)
{
  int ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_APPLICATION_PLUGIN (plugin), 1);
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (command_line), 1);

  ret = VALENT_APPLICATION_PLUGIN_GET_CLASS (plugin)->command_line (plugin,
                                                                    command_line);

  VALENT_RETURN (ret);
}

/**
 * valent_application_plugin_open: (virtual open)
 * @plugin: a #ValentApplicationPlugin
 * @files: (array length=n_files): an array of #GFiles to open
 * @n_files: the length of the @files array
 * @hint: (not nullable): a hint (or "")
 *
 * Open the given files.
 *
 * Implementations should override this method to handle files and URIs, as
 * a result of [signal@Gio.Application::open] being emitted on the primary
 * instance of the application.
 *
 * Returns: %TRUE if handled, or %FALSE if not
 *
 * Since: 1.0
 */
gboolean
valent_application_plugin_open (ValentApplicationPlugin  *plugin,
                                GFile                   **files,
                                int                       n_files,
                                const char               *hint)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_APPLICATION_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (files != NULL, FALSE);
  g_return_val_if_fail (n_files > 0, FALSE);
  g_return_val_if_fail (hint != NULL, FALSE);

  ret = VALENT_APPLICATION_PLUGIN_GET_CLASS (plugin)->open (plugin,
                                                            files,
                                                            n_files,
                                                            hint);

  VALENT_RETURN (ret);
}

