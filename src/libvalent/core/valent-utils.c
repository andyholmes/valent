// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-utils"

#include "config.h"

#include <sys/time.h>

#include "../gconstructor.h"
#include "valent-device.h"
#include "valent-macros.h"
#include "valent-utils.h"

#define DEFAULT_EXPIRATION (60L*60L*24L*10L*365L)
#define DEFAULT_KEY_SIZE   4096


/**
 * SECTION:valent-utils
 * @short_description: A collection of miscellaneous helpers
 * @title: Utilities
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * A small collection of miscellaneous helpers for working with Valent.
 */

static GThread *main_thread;
static gboolean in_flatpak = -1;


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
 * Return %TRUE or %FALSE to indicate whether Valent is running in a flatpak.
 *
 * Returns: %TRUE if running in a flatpak
 */
gboolean
valent_in_flatpak (void)
{
  if (in_flatpak == -1)
    in_flatpak = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);

  return in_flatpak;
}

static char *
get_base_path (const char *name)
{
  g_autoptr (GKeyFile) keyfile = g_key_file_new ();

  if (g_key_file_load_from_file (keyfile, "/.flatpak-info", 0, NULL))
    return g_key_file_get_string (keyfile, "Instance", name, NULL);

  return NULL;
}

/**
 * valent_get_host_path:
 * @path: a relocatable path
 *
 * Gets the path to a resource that may be relocatable at runtime. When used for
 * targetting files in a flatpak sandbox, the prefix is empty (eg. `/usr/bin` is
 * just `/bin`).
 *
 * Returns: (transfer full): a new string containing the path
 */
char *
valent_get_host_path (const char *path)
{
  static char *base_path;

  if G_UNLIKELY (base_path == NULL)
    base_path = get_base_path ("app-path");

  return g_build_filename (base_path, path, NULL);
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
  struct timeval tv;

  gettimeofday (&tv, NULL);

  return (tv.tv_sec * 1000L) + tv.tv_usec / 1000L;
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
      g_irepository_require (NULL, "Valent", PACKAGE_API, 0, &error))
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

/**
 * valent_menu_find_item:
 * @menu: a #GMenu
 * @attribute: an attribute name to match
 * @value: an attribute value to match
 *
 * Search the top-level of a #GMenuModel for the index of an item with the
 * attribute @name holding @value.
 *
 * Returns: position of the item or -1 if not found
 */
gint
valent_menu_find_item (GMenu          *menu,
                       const char     *attribute,
                       const GVariant *value)
{
  GMenuModel *model = G_MENU_MODEL (menu);
  gint i, n_items;

  g_assert (G_IS_MENU (menu));
  g_assert (attribute != NULL);
  g_assert (value != NULL);

  n_items = g_menu_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr (GVariant) ivar = NULL;

      ivar = g_menu_model_get_item_attribute_value (model, i, attribute, NULL);

      if (ivar == NULL)
        continue;

      if (g_variant_equal (value, ivar))
        return i;
    }

  return -1;
}

/**
 * valent_menu_remove_item:
 * @menu: a #GMenu
 * @attribute: an attribute name to match
 * @value: an attribute value to match
 *
 * Removes an item in @menu with a the specified attribute and value. If @menu
 * contains a top-level item with @attribute holding @value it is removed.
 *
 * Returns: the index of the removed item or -1 if not found.
 */
gint
valent_menu_remove_item (GMenu          *menu,
                         const char     *attribute,
                         const GVariant *value)
{
  gint position;

  g_return_val_if_fail (G_IS_MENU (menu), -1);
  g_return_val_if_fail (attribute != NULL, -1);
  g_return_val_if_fail (value != NULL, -1);

  position = valent_menu_find_item (menu, attribute, value);

  if (position > -1)
    g_menu_remove (menu, position);

  return position;
}

/**
 * valent_menu_replace_item:
 * @menu: a #GMenu
 * @item: a #GMenuItem
 * @attribute: an attribute name to match
 *
 * Replaces an item in @menu with @item. If @menu contains a top-level item with
 * the same action name as @item, it is removed and @item is inserted at the
 * same position. Otherwise @item is appended to @menu.
 */
void
valent_menu_replace_item (GMenu      *menu,
                          GMenuItem  *item,
                          const char *attribute)
{
  g_autoptr (GVariant) value = NULL;
  gint position = -1;

  g_return_if_fail (G_IS_MENU (menu));
  g_return_if_fail (G_IS_MENU_ITEM (item));

  value = g_menu_item_get_attribute_value (item, attribute, NULL);

  if (value != NULL)
    position = valent_menu_remove_item (menu, attribute, value);

  if (position > -1)
    g_menu_insert_item (menu, position, item);
  else
    g_menu_append_item (menu, item);
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
  const char *device_id;
  gboolean has_target;

  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (action != NULL);

  device_id = valent_device_get_id (device);
  has_target = (target != NULL);

  if (!has_target)
    target = g_variant_new_string ("");

  g_notification_set_default_action_and_target (notification,
                                                "app.device",
                                                "(ssbv)",
                                                device_id,
                                                action,
                                                has_target,
                                                target);
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
  const char *device_id;
  gboolean has_target;

  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (VALENT_IS_DEVICE (device));
  g_return_if_fail (label != NULL);
  g_return_if_fail (action != NULL);

  device_id = valent_device_get_id (device);
  has_target = (target != NULL);

  if (!has_target)
    target = g_variant_new_string ("");

  g_notification_add_button_with_target (notification,
                                         label,
                                         "app.device",
                                         "(ssbv)",
                                         device_id,
                                         action,
                                         has_target,
                                         target);
}

