// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentDevicePage"))


static void
test_device_menu_basic (ValentTestFixture *fixture,
                        gconstpointer      user_data)
{
  GtkWindow *window;
  GtkWidget *page;
  GMenuModel *device_menu;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (GMenu) section = NULL;
  g_autoptr (GMenu) submenu = NULL;
  g_autoptr (GMenu) menu_all = NULL;
  g_autoptr (GMenuItem) menu_item = NULL;

  device_menu = valent_device_get_menu (fixture->device);
  icon = g_themed_icon_new ("dialog-information-symbolic");

  page = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                       "device", fixture->device,
                       NULL);
  g_assert_nonnull (page);

  window = g_object_new (ADW_TYPE_WINDOW,
                         "content", page,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (window);
  valent_test_await_pending ();

  /* Menu Item */
  menu_item = g_menu_item_new ("Menu Item", "window.close");
  g_menu_item_set_attribute (menu_item, "hidden-when", "s", "action-disabled");
  g_menu_item_set_icon (menu_item, icon);

  g_menu_append_item (G_MENU (device_menu), menu_item);

  /* Section */
  section = g_menu_new ();
  g_menu_append_item (section, menu_item);

  g_menu_append_section (G_MENU (device_menu), "Section", G_MENU_MODEL (section));

  /* Submenu */
  submenu = g_menu_new ();
  g_menu_append_item (submenu, menu_item);

  g_menu_append_submenu (G_MENU (device_menu), "Submenu", G_MENU_MODEL (submenu));

  /* Remove Items */
  g_menu_remove (G_MENU (device_menu), 2);
  g_menu_remove (G_MENU (device_menu), 1);
  g_menu_remove (G_MENU (device_menu), 0);

  /* All Types */
  menu_all = g_menu_new ();
  g_menu_append_item (menu_all, menu_item);
  g_menu_append_section (menu_all, "Section", G_MENU_MODEL (section));
  g_menu_append_section (menu_all, "Submenu", G_MENU_MODEL (submenu));


  /* Add the menu to the device menu */
  /* g_menu_append_section (G_MENU (valent_device_get_menu (fixture->device)), */
  /*                        "Test Menu", */
  /*                        G_MENU_MODEL (menu)); */

  /* Properties */
  gtk_window_destroy (window);

  while (window != NULL)
    g_main_context_iteration (NULL, FALSE);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mock.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/ui/menu-stack/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_menu_basic,
              valent_test_fixture_clear);

  return g_test_run ();
}

