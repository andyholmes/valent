// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-utils"

#include "config.h"

#include <math.h>
#include <sys/time.h>

#include "../gconstructor.h"
#include "valent-device.h"
#include "valent-macros.h"
#include "valent-utils.h"
#include "valent-version.h"


/**
 * SECTION:valentutils
 * @short_description: A collection of miscellaneous helpers
 * @title: Utilities
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * A small collection of miscellaneous helpers for working with Valent.
 */

static GThread *main_thread;
static gboolean in_flatpak;


#if defined (G_HAS_CONSTRUCTORS)
# ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#  pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(valent_init_ctor)
# endif
G_DEFINE_CONSTRUCTOR(valent_init_ctor)
#else
# error Your platform/compiler is missing constructor support
#endif

static void
valent_init_ctor (void)
{
  main_thread = g_thread_self ();
  in_flatpak = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
}

/**
 * valent_get_main_thread:
 *
 * Gets #GThread of the main thread.
 *
 * Generally this is used by macros to determine what thread the code is
 * currently running within.
 *
 * Returns: (transfer none): a #GThread
 */
GThread *
valent_get_main_thread (void)
{
  return main_thread;
}

/**
 * valent_in_flatpak:
 *
 * Checks whether Valent is running in a Flatpak.
 *
 * Returns: %TRUE if running in a Flatpak, or %FALSE if not
 */
gboolean
valent_in_flatpak (void)
{
  return in_flatpak;
}

/**
 * valent_timestamp_ms:
 *
 * Get a current UNIX epoch timestamp in milliseconds.
 *
 * Returns: a 64-bit timestamp
 */
gint64
valent_timestamp_ms (void)
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec ts;

  clock_gettime (CLOCK_REALTIME, &ts);

  return (ts.tv_sec * 1000L) + ts.tv_nsec / 1000000L;
#else
  struct timeval tv;

  gettimeofday (&tv, NULL);

  return (tv.tv_sec * 1000L) + tv.tv_usec / 1000L;
#endif
}

/**
 * valent_load_plugins:
 * @engine: (nullable): a #PeasEngine
 *
 * Initialize the global #PeasEngine and load plugins.
 */
void
valent_load_plugins (PeasEngine *engine)
{
  g_autofree char *user_dir = NULL;
  g_autoptr (GError) error = NULL;
  const GList *plugins = NULL;
  static gboolean loaded = FALSE;

  if (loaded)
    return;

  if (engine == NULL)
    engine = peas_engine_get_default ();

  /* Ensure that we have all our required GObject Introspection packages */
  if (g_irepository_require (NULL, "Gio",  "2.0", 0, &error) &&
      g_irepository_require (NULL, "GLib", "2.0", 0, &error) &&
      g_irepository_require (NULL, "Gdk",  "4.0", 0, &error) &&
      g_irepository_require (NULL, "Gtk",  "4.0", 0, &error) &&
      g_irepository_require (NULL, "Json", "1.0", 0, &error) &&
      g_irepository_require (NULL, "Peas", "1.0", 0, &error) &&
      g_irepository_require (NULL, "Valent", VALENT_API_VERSION, 0, &error))
    peas_engine_enable_loader (engine, "python3");
  else
    g_message ("Disabling Python3 plugins: %s", error->message);

  /* Built-in & Bundled Plugins */
  peas_engine_add_search_path (engine, "resource:///plugins", NULL);
  peas_engine_add_search_path (engine, PACKAGE_PLUGINSDIR, NULL);

  /* User Plugins */
  user_dir = g_build_filename (g_get_user_data_dir (),
                               PACKAGE_NAME, "plugins", NULL);
  peas_engine_prepend_search_path (engine, user_dir, NULL);

  if (valent_in_flatpak ())
    {
      g_autofree char *extensions_dir = NULL;
      g_autofree char *flatpak_dir = NULL;

      /* Flatpak Extensions */
      extensions_dir = g_build_filename ("/app", "extensions", "lib",
                                         PACKAGE_NAME, "plugins", NULL);
      peas_engine_prepend_search_path (engine, extensions_dir, extensions_dir);

      /* User Plugins (xdg-data/valent/plugins) */
      flatpak_dir = g_build_filename (g_get_home_dir (), ".local", "share",
                                      PACKAGE_NAME, "plugins", NULL);
      peas_engine_prepend_search_path (engine, flatpak_dir, flatpak_dir);
    }

  /* Load plugins */
  plugins = peas_engine_get_plugin_list (engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    peas_engine_load_plugin (engine, iter->data);

  loaded = TRUE;
}

/**
 * valent_get_engine:
 *
 * Get the global #PeasEngine. The first call to this function initializes the
 * engine and loads the plugins.
 *
 * Returns: (transfer none): a #PeasEngine
 */
PeasEngine *
valent_get_engine (void)
{
  valent_load_plugins (NULL);

  return peas_engine_get_default ();
}

// TODO move to libvalent-notification

/**
 * valent_notification_set_device_action:
 * @notification: a #GNotification
 * @device: a #ValentDevice
 * @action: the device action name
 * @target: (nullable): the action target
 *
 * Set the default action for @notification. @action is wrapped in the special
 * `device` action for @device, which allows it to be activated from the `app`
 * action scope.
 */
void
valent_notification_set_device_action (GNotification *notification,
                                       ValentDevice  *device,
                                       const char    *action,
                                       GVariant      *target)
{
  GVariantBuilder builder;

  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (action != NULL && *action != '\0');

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

  if (target != NULL)
    g_variant_builder_add (&builder, "v", target);

  g_notification_set_default_action_and_target (notification,
                                                "app.device",
                                                "(ssav)",
                                                valent_device_get_id (device),
                                                action,
                                                &builder);
}

/**
 * valent_notification_add_device_button:
 * @notification: a #GNotification
 * @device: a #ValentDevice
 * @label: the button label
 * @action: the device action name
 * @target: (nullable): the action target
 *
 * Add an action button to @notification. @action is wrapped in the special
 * `device` action for @device, which allows it to be activated from the `app`
 * action scope.
 */
void
valent_notification_add_device_button (GNotification *notification,
                                       ValentDevice  *device,
                                       const char    *label,
                                       const char    *action,
                                       GVariant      *target)
{
  GVariantBuilder builder;

  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (label != NULL && *label != '\0');
  g_return_if_fail (action != NULL && *action != '\0');

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

  if (target != NULL)
    g_variant_builder_add (&builder, "v", target);

  g_notification_add_button_with_target (notification,
                                         label,
                                         "app.device",
                                         "(ssav)",
                                         valent_device_get_id (device),
                                         action,
                                         &builder);
}

