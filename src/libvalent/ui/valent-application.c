// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>

#include "valent-application.h"
#include "valent-window.h"


/**
 * SECTION:valentapplication
 * @short_description: Valent Application
 * @title: ValentApplication
 * @stability: Unstable
 *
 * #ValentApplication is the primary application class for Valent.
 */

struct _ValentApplication
{
  GtkApplication       parent_instance;

  ValentDeviceManager *manager;
  GtkWindow           *window;
};

G_DEFINE_TYPE (ValentApplication, valent_application, GTK_TYPE_APPLICATION)


static void
valent_application_present_window (ValentApplication *self,
                                   const char        *startup_id)
{
  g_assert (VALENT_IS_APPLICATION (self));

  if (self->window == NULL)
    {
      self->window = g_object_new (VALENT_TYPE_WINDOW,
                                   "application",    self,
                                   "default-width",  600,
                                   "default-height", 480,
                                   "device-manager", self->manager,
                                   NULL);
      g_object_add_weak_pointer (G_OBJECT (self->window),
                                 (gpointer) &self->window);
    }

  if (startup_id != NULL)
    gtk_window_set_startup_id (self->window, startup_id);

  gtk_window_present_with_time (self->window, GDK_CURRENT_TIME);
}

/*
 * GActions
 */
static void
device_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  ValentApplication *self = VALENT_APPLICATION (user_data);
  ValentDevice *device;
  const char *device_id;
  const char *name;
  g_autoptr (GVariantIter) targetv = NULL;
  g_autoptr (GVariant) target = NULL;

  g_assert (VALENT_IS_APPLICATION (self));

  /* Device ID, action name, array holding optional action parameter */
  g_variant_get (parameter, "(&s&sav)", &device_id, &name, &targetv);
  g_variant_iter_next (targetv, "v", &target);

  /* Forward the activation */
  device = valent_device_manager_get_device (self->manager, device_id);

  if (device != NULL)
    {
      GActionGroup *actions;

      actions = valent_device_get_actions (device);
      g_action_group_activate_action (actions, name, target);
    }
}

static void
preferences_action (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  ValentApplication *self = VALENT_APPLICATION (user_data);

  g_assert (VALENT_IS_APPLICATION (self));

  valent_application_present_window (self, NULL);
}

static void
quit_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  GApplication *application = G_APPLICATION (user_data);

  g_assert (G_IS_APPLICATION (application));

  g_application_quit (application);
}

static void
refresh_action (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  ValentApplication *self = VALENT_APPLICATION (user_data);

  g_assert (VALENT_IS_APPLICATION (self));

  valent_device_manager_identify (self->manager, NULL);
}

static const GActionEntry actions[] = {
  { "device",      device_action,      "(ssav)", NULL, NULL },
  { "preferences", preferences_action, NULL,     NULL, NULL },
  { "quit",        quit_action,        NULL,     NULL, NULL },
  { "refresh",     refresh_action,     NULL,     NULL, NULL }
};


/*
 * GApplication
 */
static void
valent_application_activate (GApplication *application)
{
  ValentApplication *self = VALENT_APPLICATION (application);

  g_assert (VALENT_IS_APPLICATION (self));

  valent_application_present_window (self, NULL);
}

static void
valent_application_open (GApplication  *application,
                         GFile        **files,
                         int            n_files,
                         const char    *hint)
{
  VALENT_TODO ("handle files and URIs");
}

static void
valent_application_startup (GApplication *application)
{
  ValentApplication *self = VALENT_APPLICATION (application);

  g_assert (VALENT_IS_APPLICATION (application));

  /* Chain-up first */
  G_APPLICATION_CLASS (valent_application_parent_class)->startup (application);

  g_application_hold (application);
  adw_init ();

  /* Service Actions */
  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   application);

  /* Start the device manager */
  valent_device_manager_start (self->manager);

  gtk_window_set_default_icon_name (APPLICATION_ID);
}

static void
valent_application_shutdown (GApplication *application)
{
  ValentApplication *self = VALENT_APPLICATION (application);

  g_assert (VALENT_IS_APPLICATION (application));

  g_clear_pointer (&self->window, gtk_window_destroy);
  valent_device_manager_stop (self->manager);

  G_APPLICATION_CLASS (valent_application_parent_class)->shutdown (application);
}

static gboolean
valent_application_dbus_register (GApplication     *application,
                                  GDBusConnection  *connection,
                                  const char       *object_path,
                                  GError          **error)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GApplicationClass *klass = G_APPLICATION_CLASS (valent_application_parent_class);

  g_assert (VALENT_IS_APPLICATION (self));

  /* Chain-up first */
  if (!klass->dbus_register (application, connection, object_path, error))
    return FALSE;

  self->manager = valent_device_manager_new_sync (NULL, NULL, error);

  if (self->manager == NULL)
    return FALSE;

  valent_device_manager_export (self->manager, connection);

  return TRUE;
}

static void
valent_application_dbus_unregister (GApplication    *application,
                                    GDBusConnection *connection,
                                    const char      *object_path)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GApplicationClass *klass = G_APPLICATION_CLASS (valent_application_parent_class);

  g_assert (VALENT_IS_APPLICATION (self));

  valent_device_manager_unexport (self->manager);
  g_clear_object (&self->manager);

  /* Chain-up last */
  klass->dbus_unregister (application, connection, object_path);
}


/*
 * GObject
 */
static void
valent_application_class_init (ValentApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  application_class->activate = valent_application_activate;
  application_class->startup = valent_application_startup;
  application_class->shutdown = valent_application_shutdown;
  application_class->open = valent_application_open;
  application_class->dbus_register = valent_application_dbus_register;
  application_class->dbus_unregister = valent_application_dbus_unregister;
}

static void
valent_application_init (ValentApplication *self)
{
}

ValentApplication *
_valent_application_new (void)
{
  return g_object_new (VALENT_TYPE_APPLICATION,
                       "application-id",     APPLICATION_ID,
                       "resource-base-path", "/ca/andyholmes/Valent",
                       "flags",              G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

