// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-window"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>
#include <libvalent-contacts.h>
#include <libvalent-input.h>
#include <libvalent-media.h>
#include <libvalent-mixer.h>
#include <libvalent-notifications.h>
#include <libvalent-session.h>

#include "valent-device-panel.h"
#include "valent-preferences-window.h"
#include "valent-window.h"


struct _ValentWindow
{
  AdwApplicationWindow  parent_instance;
  ValentDeviceManager  *manager;
  GSettings            *settings;

  GHashTable           *devices;

  /* Template widgets */
  GtkStack             *stack;
  GtkListBox           *device_list;
  unsigned int          refresh_id;

  GtkWindow            *preferences;
};

G_DEFINE_TYPE (ValentWindow, valent_window, ADW_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_DEVICE_MANAGER,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * Device Callbacks
 */
typedef struct
{
  GtkWidget *row;
  GtkWidget *panel;
  GtkWidget *status;
} DeviceInfo;

static int
device_sort_func (GtkListBoxRow *row1,
                  GtkListBoxRow *row2,
                  gpointer       user_data)
{
  ValentDevice *device1, *device2;
  ValentDeviceState state_a, state_b;
  gboolean connected_a, connected_b;
  gboolean paired_a, paired_b;

  device1 = g_object_get_data (G_OBJECT (row1), "device");
  device2 = g_object_get_data (G_OBJECT (row2), "device");

  state_a = valent_device_get_state (device1);
  state_b = valent_device_get_state (device2);

  // Sort connected before disconnected
  connected_a = state_a & VALENT_DEVICE_STATE_CONNECTED;
  connected_b = state_b & VALENT_DEVICE_STATE_CONNECTED;

  if (connected_a > connected_b)
    return -1;
  else if (connected_a < connected_b)
    return 1;

  // Sort paired before unpaired
  paired_a = state_a & VALENT_DEVICE_STATE_PAIRED;
  paired_b = state_b & VALENT_DEVICE_STATE_PAIRED;

  if (paired_a > paired_b)
    return -1;
  else if (paired_a < paired_b)
    return 1;

  // Sort equals by name
  return g_utf8_collate (valent_device_get_name (device1),
                         valent_device_get_name (device2));
}

static void
on_device_changed (ValentDevice *device,
                   GParamSpec   *pspec,
                   ValentWindow *self)
{
  ValentDeviceState state;
  GtkStyleContext *style;
  DeviceInfo *info;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_WINDOW (self));

  info = g_hash_table_lookup (self->devices, device);

  if G_UNLIKELY (info == NULL)
    return;

  state = valent_device_get_state (device);
  style = gtk_widget_get_style_context (info->status);

  if ((state & VALENT_DEVICE_STATE_PAIRED) == 0)
    {
      gtk_label_set_label (GTK_LABEL (info->status), _("Unpaired"));
      gtk_style_context_remove_class (style, "dim-label");
    }
  else if ((state & VALENT_DEVICE_STATE_CONNECTED) == 0)
    {
      gtk_label_set_label (GTK_LABEL (info->status), _("Disconnected"));
      gtk_style_context_add_class (style, "dim-label");
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (info->status), _("Connected"));
      gtk_style_context_remove_class (style, "dim-label");
    }

  gtk_list_box_invalidate_sort (self->device_list);
}

static void
on_device_added (ValentDeviceManager *manager,
                 ValentDevice        *device,
                 ValentWindow        *self)
{
  DeviceInfo *info;
  const char *device_id;
  const char *name;
  const char *icon_name;
  GtkStackPage *page;
  GtkWidget *box;
  GtkWidget *arrow;

  g_assert (VALENT_IS_DEVICE_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_WINDOW (self));

  info = g_new0 (DeviceInfo, 1);
  g_hash_table_insert (self->devices, device, info);

  device_id = valent_device_get_id (device);
  name = valent_device_get_name (device);
  icon_name = valent_device_get_icon_name (device);

  /* Panel */
  info->panel = g_object_new (VALENT_TYPE_DEVICE_PANEL,
                              "device", device,
                              NULL);
  page = gtk_stack_add_titled (self->stack, info->panel, device_id, name);
  g_object_bind_property (device, "name",
                          page,   "title",
                          G_BINDING_SYNC_CREATE);

  /* Row */
  info->row = g_object_new (ADW_TYPE_ACTION_ROW,
                            "action-name",   "win.device",
                            "action-target", g_variant_new_string (device_id),
                            "icon-name",     icon_name,
                            "title",         name,
                            "activatable",   TRUE,
                            "selectable",    FALSE,
                            NULL);
  g_object_set_data (G_OBJECT (info->row), "device", device);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  adw_action_row_add_suffix (ADW_ACTION_ROW (info->row), box);

  info->status = g_object_new (GTK_TYPE_LABEL, NULL);
  gtk_box_append (GTK_BOX (box), info->status);

  arrow = gtk_image_new_from_icon_name ("go-next-symbolic");
  gtk_style_context_add_class (gtk_widget_get_style_context (arrow), "dim-label");
  gtk_box_append (GTK_BOX (box), arrow);

  /* Bind to device */
  g_object_bind_property (device,    "name",
                          info->row, "title",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (device,    "icon-name",
                          info->row, "icon-name",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect (device,
                    "notify::state",
                    G_CALLBACK (on_device_changed),
                    self);
  on_device_changed (device, NULL, self);

  gtk_list_box_insert (self->device_list, info->row, -1);
}

static void
on_device_removed (ValentDeviceManager *manager,
                   ValentDevice        *device,
                   ValentWindow        *self)
{
  DeviceInfo *info;

  if ((info = g_hash_table_lookup (self->devices, device)) == NULL)
    return;

  if (gtk_stack_get_visible_child (self->stack) == info->panel)
    gtk_stack_set_visible_child_name (self->stack, "main");

  g_signal_handlers_disconnect_by_data (device, self);
  gtk_list_box_remove (self->device_list, info->row);
  gtk_stack_remove (self->stack, info->panel);

  g_hash_table_remove (self->devices, device);
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
  const char *authors[] = {
    "Andy Holmes <andrew.g.r.holmes@gmail.com>",
    NULL
  };

  g_assert (GTK_IS_WINDOW (window));

  gtk_show_about_dialog (window,
                         "logo-icon-name",     APPLICATION_ID,
                         "comments",           _("Connect, control and sync devices"),
                         "version",            PACKAGE_VERSION,
                         "authors",            authors,
                         "translator-credits", _("translator-credits"),
                         "license-type",       GTK_LICENSE_GPL_3_0,
                         "website",            PACKAGE_URL,
                         NULL);
}

static void
device_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *parameter)
{
  ValentWindow *self = VALENT_WINDOW (widget);
  const char *device_id;

  g_assert (VALENT_IS_WINDOW (self));

  device_id = g_variant_get_string (parameter, NULL);
  gtk_stack_set_visible_child_name (self->stack, device_id);
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
      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      self->preferences = g_object_new (VALENT_TYPE_PREFERENCES_WINDOW,
                                        "default-width",  allocation.width,
                                        "default-height", allocation.height,
                                        "modal",          TRUE,
                                        "transient-for",  self,
                                        NULL);

      g_object_add_weak_pointer (G_OBJECT (self->preferences),
                                 (gpointer)&self->preferences);
    }

  gtk_window_present (self->preferences);
}

static void
previous_action (GtkWidget  *widget,
                 const char *action_name,
                 GVariant   *parameter)
{
  ValentWindow *self = VALENT_WINDOW (widget);

  g_assert (VALENT_IS_WINDOW (self));

  gtk_stack_set_visible_child_name (self->stack, "main");
}

static gboolean
refresh_cb (gpointer data)
{
  ValentWindow *self = VALENT_WINDOW (data);

  self->refresh_id = 0;

  return G_SOURCE_REMOVE;
}

static void
refresh_action (GtkWidget  *widget,
                const char *action_name,
                GVariant   *parameter)
{
  ValentWindow *self = VALENT_WINDOW (widget);

  g_assert (VALENT_IS_WINDOW (self));

  if (self->refresh_id > 0)
    return;

  valent_device_manager_identify (self->manager, NULL);
  self->refresh_id = g_timeout_add_seconds (5, refresh_cb, self);
}

/*
 * GObject
 */
static void
valent_window_constructed (GObject *object)
{
  ValentWindow *self = VALENT_WINDOW (object);
  g_autoptr (GPtrArray) devices = NULL;

  g_assert (self->manager != NULL);

  /* Devices */
  devices = valent_device_manager_get_devices (self->manager);

  for (unsigned int i = 0; i < devices->len; i++)
    on_device_added (self->manager, g_ptr_array_index (devices, i), self);

  g_signal_connect (self->manager,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    self);

  g_signal_connect (self->manager,
                    "device-removed",
                    G_CALLBACK (on_device_removed),
                    self);

  G_OBJECT_CLASS (valent_window_parent_class)->constructed (object);
}

static void
valent_window_dispose (GObject *object)
{
  ValentWindow *self = VALENT_WINDOW (object);
  GHashTableIter iter;
  ValentDevice *device;

  g_clear_pointer (&self->preferences, gtk_window_destroy);
  g_clear_handle_id (&self->refresh_id, g_source_remove);
  g_signal_handlers_disconnect_by_data (self->manager, self);

  g_hash_table_iter_init (&iter, self->devices);

  while (g_hash_table_iter_next (&iter, (void **)&device, NULL))
    g_signal_handlers_disconnect_by_data (device, self);

  G_OBJECT_CLASS (valent_window_parent_class)->dispose (object);
}

static void
valent_window_finalize (GObject *object)
{
  ValentWindow *self = VALENT_WINDOW (object);

  g_clear_pointer (&self->devices, g_hash_table_unref);
  g_clear_object (&self->manager);
  g_clear_object (&self->settings);

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
  g_autoptr (GtkCssProvider) theme = NULL;

  object_class->constructed = valent_window_constructed;
  object_class->dispose = valent_window_dispose;
  object_class->finalize = valent_window_finalize;
  object_class->get_property = valent_window_get_property;
  object_class->set_property = valent_window_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-window.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, stack);
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, device_list);

  gtk_widget_class_install_action (widget_class, "win.about", NULL, about_action);
  gtk_widget_class_install_action (widget_class, "win.device", "s", device_action);
  gtk_widget_class_install_action (widget_class, "win.preferences", NULL, preferences_action);
  gtk_widget_class_install_action (widget_class, "win.previous", NULL, previous_action);
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

  /* Custom CSS */
  theme = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (theme, "/ca/andyholmes/Valent/ui/style.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (theme),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
valent_window_init (ValentWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  if (g_strcmp0 (PROFILE_NAME, "devel") == 0)
    {
      GtkStyleContext *style;

      style = gtk_widget_get_style_context (GTK_WIDGET (self));
      gtk_style_context_add_class (style, "devel");
    }

  /* Devices Page */
  gtk_list_box_set_sort_func (self->device_list, device_sort_func, NULL, NULL);
  self->devices = g_hash_table_new_full (NULL, NULL, NULL, g_free);
}

