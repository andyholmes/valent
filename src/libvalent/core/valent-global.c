// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-global"

#include "config.h"

#include <time.h>

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libportal/portal.h>

#include "valent-global.h"
#include "valent-version.h"


static GThread *main_thread;
static PeasEngine *default_engine = NULL;


/*
 * libvalent Constructor
 */
#ifndef __has_attribute
# define __has_attribute(x) 0  /* Compatibility with non-clang compilers. */
#endif
#if __has_attribute(constructor)
static void __attribute__((constructor))
valent_init_ctor (void)
{
  main_thread = g_thread_self ();
}
#else
# error Your platform/compiler is missing constructor support
#endif


/**
 * valent_get_main_thread: (skip)
 *
 * Get the main thread.
 *
 * Use the macro `VALENT_IS_MAIN_THREAD()` to determine whether or not the code
 * is currently running on the main thread.
 *
 * Returns: (transfer none): a #GThread
 *
 * Since: 1.0
 */
GThread *
valent_get_main_thread (void)
{
  return main_thread;
}

/**
 * valent_get_plugin_engine:
 *
 * Get the global #PeasEngine.
 *
 * The first call to this function initializes the #PeasEngine and adds
 * `resource:///plugins` to the search path, where emebedded plugins should be
 * registered. If running in a Flatpak, `/app/extensions/lib/valent/plugins`
 * will also be added to the search path for Flatpak extensions.
 *
 * Returns: (transfer none): a #PeasEngine
 *
 * Since: 1.0
 */
PeasEngine *
valent_get_plugin_engine (void)
{
  if (default_engine == NULL)
    {
      g_autoptr (GError) error = NULL;
      const GList *plugins = NULL;

      default_engine = peas_engine_get_default ();
      g_object_add_weak_pointer (G_OBJECT (default_engine),
                                 (gpointer)&default_engine);

      /* Ensure we have the minimum required typelibs for Python plugins */
      if (g_irepository_require (NULL, "Gio",  "2.0", 0, &error) &&
          g_irepository_require (NULL, "GLib", "2.0", 0, &error) &&
          g_irepository_require (NULL, "Json", "1.0", 0, &error) &&
          g_irepository_require (NULL, "Peas", "1.0", 0, &error) &&
          g_irepository_require (NULL, "Gdk",  "4.0", 0, &error) &&
          g_irepository_require (NULL, "Gtk",  "4.0", 0, &error) &&
          g_irepository_require (NULL, "Valent", VALENT_API_VERSION, 0, &error))
        peas_engine_enable_loader (default_engine, "python3");
      else
        g_message ("Disabling Python3 plugins: %s", error->message);

      /* Built-in Plugins */
      peas_engine_add_search_path (default_engine, "resource:///plugins", NULL);

      /* Flatpak Extensions */
      if (xdp_portal_running_under_flatpak ())
        {
          g_autofree char *flatpak_dir = NULL;

          flatpak_dir = g_build_filename ("/app", "extensions", "lib",
                                          "valent", "plugins", NULL);
          peas_engine_prepend_search_path (default_engine, flatpak_dir, NULL);
        }

      /* Load built-in plugins and Flatpak extensions automatically */
      plugins = peas_engine_get_plugin_list (default_engine);

      for (const GList *iter = plugins; iter; iter = iter->next)
        peas_engine_load_plugin (default_engine, iter->data);
    }

  return default_engine;
}

/**
 * valent_get_user_directory:
 * @directory: a #GUserDirectory
 *
 * Returns the full path of a special directory using its logical id.
 *
 * This function is a safe wrapper around [func@GLib.get_user_special_dir] that
 * always returns a valid path to an existing directory. The fallback path used
 * is the return value of [func@GLib.get_home_dir].
 *
 * Returns: (transfer none): the path to the specified special directory
 *
 * Since: 1.0
 */
const char *
valent_get_user_directory (GUserDirectory directory)
{
  const char *path = NULL;

  g_return_val_if_fail (directory >= G_USER_DIRECTORY_DESKTOP &&
                        directory < G_USER_N_DIRECTORIES, NULL);

  if ((path = g_get_user_special_dir (directory)) == NULL)
    path = g_get_home_dir ();

  if (g_mkdir_with_parents (path, 0755) == -1)
    g_warning ("%s(): [%s]: %s", G_STRFUNC, path, g_strerror (errno));

  return path;
}

/**
 * valent_get_user_file:
 * @dirname: a directory path
 * @basename: (type filename): a file name
 * @unique: whether the resulting filepath should be unique
 *
 * A convenience for creating a [iface@Gio.File].
 *
 * If @unique is true, the returned file is guaranteed not to exist. If
 * @basename exists in @dirname, the resulting file's name will have a
 * parenthesized number appended to it (e.g. `image.png (2)`).
 *
 * Returns: (transfer full): a #GFile
 *
 * Since: 1.0
 */
GFile *
valent_get_user_file (const char *dirname,
                      const char *basename,
                      gboolean    unique)
{
  g_autofree char *basepath = NULL;
  g_autofree char *filepath = NULL;
  unsigned int copy_num = 0;

  g_return_val_if_fail (dirname != NULL, NULL);
  g_return_val_if_fail (basename != NULL, NULL);

  basepath = g_build_filename (dirname, basename, NULL);
  filepath = g_strdup (basepath);

  /* If a unique path is requested, loop until we find a free name */
  while (unique && g_file_test (filepath, G_FILE_TEST_EXISTS))
    {
      g_free (filepath);
      filepath = g_strdup_printf ("%s (%d)", basepath, ++copy_num);
    }

  return g_file_new_for_path (filepath);
}

/**
 * valent_timestamp_ms:
 *
 * Get a current UNIX epoch timestamp in milliseconds.
 *
 * This timestamp format is used in several parts of the KDE Connect protocol.
 *
 * Returns: a 64-bit timestamp
 *
 * Since: 1.0
 */
int64_t
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
#endif /* HAVE_CLOCK_GETTIME */
}

