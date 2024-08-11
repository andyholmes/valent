// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-window"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libportal/portal.h>
#include <valent.h>

#include "valent-application-credits.h"
#include "valent-device-page.h"
#include "valent-device-row.h"
#include "valent-preferences-dialog.h"
#include "valent-ui-utils-private.h"
#include "valent-version-vcs.h"

#include "valent-window.h"


struct _ValentWindow
{
  AdwApplicationWindow  parent_instance;
  ValentDeviceManager  *manager;

  AdwAnimation         *scan;
  AdwAnimation         *fade;

  /* template */
  AdwNavigationView    *view;
  GtkProgressBar       *progress_bar;
  GtkListBox           *device_list;
  AdwDialog            *preferences;
};

G_DEFINE_FINAL_TYPE (ValentWindow, valent_window, ADW_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_DEVICE_MANAGER,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static JsonNode *
valent_get_debug_info (void)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autofree char *os_name = NULL;
  const char *desktop = NULL;
  const char *session = NULL;
  const char *environment = NULL;
  PeasEngine *engine = NULL;
  unsigned int n_plugins = 0;

  os_name = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);
  desktop = g_getenv ("XDG_CURRENT_DESKTOP");
  session = g_getenv ("XDG_SESSION_TYPE");
  environment = xdp_portal_running_under_flatpak () ? "flatpak" :
                  (xdp_portal_running_under_snap (NULL) ? "snap" : "host");

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  /* Application */
  json_builder_set_member_name (builder, "application");
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, APPLICATION_ID);
  json_builder_set_member_name (builder, "version");
  json_builder_add_string_value (builder, VALENT_VERSION);
  json_builder_set_member_name (builder, "commit");
  json_builder_add_string_value (builder, VALENT_VCS_TAG);
  json_builder_end_object (builder);

  /* Runtime */
  json_builder_set_member_name (builder, "runtime");
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "os");
  json_builder_add_string_value (builder, os_name != NULL ? os_name : "unknown");
  json_builder_set_member_name (builder, "desktop");
  json_builder_add_string_value (builder, desktop != NULL ? desktop : "unknown");
  json_builder_set_member_name (builder, "session");
  json_builder_add_string_value (builder, session != NULL ? session : "unknown");
  json_builder_set_member_name (builder, "environment");
  json_builder_add_string_value (builder, environment);
  json_builder_end_object (builder);

  /* Plugins */
  json_builder_set_member_name (builder, "plugins");
  json_builder_begin_object (builder);

  engine = valent_get_plugin_engine ();
  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (engine));

  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) info = NULL;
      const char *name = NULL;
      gboolean loaded = FALSE;

      info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      name = peas_plugin_info_get_module_name (info);
      loaded = peas_plugin_info_is_loaded (info);

      json_builder_set_member_name (builder, name);
      json_builder_add_boolean_value (builder, loaded);
    }
  json_builder_end_object (builder);

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

/*
 * AdwAnimation Callbacks
 */
static gboolean
refresh_cb (gpointer data)
{
  g_assert (VALENT_IS_DEVICE_MANAGER (data));

  valent_device_manager_refresh (VALENT_DEVICE_MANAGER (data));

  return G_SOURCE_CONTINUE;
}

static void
on_animation_state_changed (AdwAnimation *animation,
                            GParamSpec   *pspec,
                            ValentWindow *self)
{
  AdwAnimationState state = adw_animation_get_state (animation);
  static unsigned int refresh_id = 0;

  g_clear_handle_id (&refresh_id, g_source_remove);

  switch (state)
    {
    case ADW_ANIMATION_PLAYING:
      if (self->scan == animation)
        {
          valent_device_manager_refresh (self->manager);
          refresh_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                   2,
                                                   refresh_cb,
                                                   g_object_ref (self->manager),
                                                   g_object_unref);
          gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.refresh", FALSE);
        }
      break;

    case ADW_ANIMATION_FINISHED:
      if (self->scan == animation)
        {
          adw_animation_play (self->fade);
          gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.refresh", TRUE);
        }
      break;

    case ADW_ANIMATION_IDLE:
    case ADW_ANIMATION_PAUSED:
    default:
      gtk_progress_bar_set_fraction (self->progress_bar, 0.0);
      gtk_widget_set_opacity (GTK_WIDGET (self->progress_bar), 1.0);
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.refresh", TRUE);
      break;
    }
}

static GtkWidget *
valent_window_create_row_func (gpointer item,
                               gpointer user_data)
{
  ValentWindow *self = VALENT_WINDOW (user_data);
  ValentDevice *device = VALENT_DEVICE (item);
  const char *device_id;

  g_assert (VALENT_IS_WINDOW (self));
  g_assert (VALENT_IS_DEVICE (item));

  /* A device was added, so stop the refresh */
  if (self->scan != NULL)
    adw_animation_skip (self->scan);

  device_id = valent_device_get_id (device);
  return g_object_new (VALENT_TYPE_DEVICE_ROW,
                       "device",        device,
                       "action-name",   "win.page",
                       "action-target", g_variant_new_string (device_id),
                       "activatable",   TRUE,
                       "selectable",    FALSE,
                       NULL);
}

/*
 * GActions
 */
static void
about_action (GtkWidget  *widget,
              const char *action_name,
              GVariant   *parameter)
{
  GtkWindow *window = GTK_WINDOW (widget);
  AdwDialog *dialog = NULL;
  g_autoptr (JsonNode) debug_json = NULL;
  g_autofree char *debug_info = NULL;

  g_assert (GTK_IS_WINDOW (window));

  debug_json = valent_get_debug_info ();
  debug_info = json_to_string (debug_json, TRUE);

  dialog = g_object_new (ADW_TYPE_ABOUT_DIALOG,
                         "application-icon",    APPLICATION_ID,
                         "application-name",    _("Valent"),
                         "copyright",           "© Andy Holmes",
                         "issue-url",           PACKAGE_BUGREPORT,
                         "license-type",        GTK_LICENSE_GPL_3_0,
                         "debug-info",          debug_info,
                         "debug-info-filename", "valent-debug.json",
                         "artists",             valent_application_credits_artists,
                         "designers",           valent_application_credits_designers,
                         "developers",          valent_application_credits_developers,
                         "documenters",         valent_application_credits_documenters,
                         "translator-credits",  _("translator-credits"),
                         "version",             PACKAGE_VERSION,
                         "website",             PACKAGE_URL,
                         NULL);
  adw_about_dialog_add_acknowledgement_section (ADW_ABOUT_DIALOG (dialog),
                                                _("Sponsors"),
                                                valent_application_credits_sponsors);

  adw_dialog_present (dialog, GTK_WIDGET (window));
}

static void
page_action (GtkWidget  *widget,
             const char *action_name,
             GVariant   *parameter)
{
  ValentWindow *self = VALENT_WINDOW (widget);
  unsigned int n_devices = 0;
  const char *tag;

  g_assert (VALENT_IS_WINDOW (self));

  tag = g_variant_get_string (parameter, NULL);

  if (*tag == '\0' || g_strcmp0 (tag, "main") == 0)
    {
      adw_navigation_view_pop (self->view);
      return;
    }


  g_clear_pointer (&self->preferences, adw_dialog_force_close);

  n_devices = g_list_model_get_n_items (G_LIST_MODEL (self->manager));
  for (unsigned int i = 0; i < n_devices; i++)
    {
      g_autoptr (ValentDevice) device = NULL;
      AdwNavigationPage *page;

      device = g_list_model_get_item (G_LIST_MODEL (self->manager), i);

      if (g_strcmp0 (valent_device_get_id (device), tag) == 0)
        {
          page = g_object_new (VALENT_TYPE_DEVICE_PAGE,
                               "device", device,
                               NULL);
          adw_navigation_view_push (self->view, page);
          break;
        }
    }
}

static void
preferences_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameter)
{
  ValentWindow *self = VALENT_WINDOW (widget);

  g_assert (VALENT_IS_WINDOW (self));

  if (self->preferences == NULL)
    {
      self->preferences = g_object_new (VALENT_TYPE_PREFERENCES_DIALOG, NULL);
      g_object_add_weak_pointer (G_OBJECT (self->preferences),
                                 (gpointer)&self->preferences);
    }

  adw_dialog_present (ADW_DIALOG (self->preferences), GTK_WIDGET (self));
}

static void
refresh_action (GtkWidget  *widget,
                const char *action_name,
                GVariant   *parameter)
{
  ValentWindow *self = VALENT_WINDOW (widget);

  g_assert (VALENT_IS_WINDOW (self));

  if (!adw_get_enable_animations (widget))
    {
      valent_device_manager_refresh (self->manager);
      return;
    }

  if (self->scan == NULL && self->fade == NULL)
    {
      AdwAnimationTarget *target = NULL;

      target = adw_property_animation_target_new (G_OBJECT (self->progress_bar),
                                                  "fraction");
      self->scan = adw_timed_animation_new (widget, 0.0, 1.0, 5000, target);
      g_signal_connect_object (self->scan,
                               "notify::state",
                               G_CALLBACK (on_animation_state_changed),
                               self, 0);

      target = adw_property_animation_target_new (G_OBJECT (self->progress_bar),
                                                  "opacity");
      self->fade = adw_timed_animation_new (widget, 1.0, 0.0, 500, target);
      g_signal_connect_object (self->fade,
                               "notify::state",
                               G_CALLBACK (on_animation_state_changed),
                               self, 0);
    }
  else
    {
      adw_animation_reset (self->fade);
      adw_animation_reset (self->scan);
    }

  adw_animation_play (self->scan);
}

/*
 * GObject
 */
static void
valent_window_constructed (GObject *object)
{
  ValentWindow *self = VALENT_WINDOW (object);

  g_assert (self->manager != NULL);

  gtk_list_box_bind_model (self->device_list,
                           G_LIST_MODEL (self->manager),
                           valent_window_create_row_func,
                           self, NULL);

  G_OBJECT_CLASS (valent_window_parent_class)->constructed (object);
}

static void
valent_window_dispose (GObject *object)
{
  ValentWindow *self = VALENT_WINDOW (object);

  if (self->scan != NULL)
    adw_animation_reset (self->scan);
  g_clear_object (&self->scan);

  if (self->fade != NULL)
    adw_animation_reset (self->fade);
  g_clear_object (&self->fade);

  g_clear_pointer (&self->preferences, adw_dialog_force_close);
  gtk_widget_dispose_template (GTK_WIDGET (object), VALENT_TYPE_WINDOW);

  G_OBJECT_CLASS (valent_window_parent_class)->dispose (object);
}

static void
valent_window_finalize (GObject *object)
{
  ValentWindow *self = VALENT_WINDOW (object);

  g_clear_object (&self->manager);

  G_OBJECT_CLASS (valent_window_parent_class)->finalize (object);
}

static void
valent_window_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ValentWindow *self = VALENT_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DEVICE_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_window_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ValentWindow *self = VALENT_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DEVICE_MANAGER:
      self->manager = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_window_class_init (ValentWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_window_constructed;
  object_class->dispose = valent_window_dispose;
  object_class->finalize = valent_window_finalize;
  object_class->get_property = valent_window_get_property;
  object_class->set_property = valent_window_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-window.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, view);
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, progress_bar);
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, device_list);

  gtk_widget_class_install_action (widget_class, "win.about", NULL, about_action);
  gtk_widget_class_install_action (widget_class, "win.page", "s", page_action);
  gtk_widget_class_install_action (widget_class, "win.preferences", NULL, preferences_action);
  gtk_widget_class_install_action (widget_class, "win.refresh", NULL, refresh_action);

  /**
   * ValentWindow:device-manager:
   *
   * The [class@Valent.DeviceManager] that the window represents.
   */
  properties [PROP_DEVICE_MANAGER] =
    g_param_spec_object ("device-manager", NULL, NULL,
                         VALENT_TYPE_DEVICE_MANAGER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_window_init (ValentWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

