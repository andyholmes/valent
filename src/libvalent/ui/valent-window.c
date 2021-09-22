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
#include "valent-panel.h"
#include "valent-plugin-group.h"
#include "valent-window.h"


struct _ValentWindow
{
  AdwApplicationWindow  parent_instance;
  ValentManager        *manager;
  GSettings            *settings;

  GHashTable           *devices;
  GQueue               *history;

  /* Template widgets */
  GtkStack             *stack;
  GtkListBox           *device_list;
  GtkSpinner           *device_list_spinner;
  unsigned int          device_list_spinner_id;

  GtkDialog            *rename_dialog;
  GtkEntry             *rename_entry;
  GtkLabel             *rename_label;
  GtkButton            *rename_button;
};

G_DEFINE_TYPE (ValentWindow, valent_window, ADW_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_MANAGER,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * Device Name Dialog
 */
static void
on_rename_entry_changed (GtkEntry     *entry,
                         ValentWindow *self)
{
  const char *name = NULL;
  const char *new_name = NULL;

  name = gtk_label_get_text (self->rename_label);
  new_name = gtk_editable_get_text (GTK_EDITABLE (entry));

  gtk_widget_set_sensitive (GTK_WIDGET (self->rename_button),
                            (g_strcmp0 (name, new_name) != 0));
}

static void
on_rename_dialog_open (GtkListBox    *box,
                       GtkListBoxRow *row,
                       ValentWindow  *self)
{
  g_autofree char *name = NULL;

  name = g_settings_get_string (self->settings, "name");
  gtk_editable_set_text (GTK_EDITABLE (self->rename_entry), name);

  gtk_window_present_with_time (GTK_WINDOW (self->rename_dialog),
                                GDK_CURRENT_TIME);
}

static void
on_rename_dialog_response (GtkDialog       *dialog,
                           GtkResponseType  response_id,
                           ValentWindow    *self)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      const char *name;

      name = gtk_editable_get_text (GTK_EDITABLE (self->rename_entry));
      g_settings_set_string (self->settings, "name", name);
    }

  gtk_widget_hide (GTK_WIDGET (dialog));
}

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
  gboolean connected_a, connected_b;
  gboolean paired_a, paired_b;

  device1 = g_object_get_data (G_OBJECT (row1), "device");
  device2 = g_object_get_data (G_OBJECT (row2), "device");

  // Sort connected before disconnected
  connected_a = valent_device_get_connected (device1);
  connected_b = valent_device_get_connected (device2);

  if (connected_a > connected_b)
    return -1;
  else if (connected_a < connected_b)
    return 1;

  // Sort paired before unpaired
  paired_a = valent_device_get_paired (device1);
  paired_b = valent_device_get_paired (device2);

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
  GtkStyleContext *style;
  DeviceInfo *info;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_WINDOW (self));

  info = g_hash_table_lookup (self->devices, device);

  if G_UNLIKELY (info == NULL)
    return;

  style = gtk_widget_get_style_context (info->status);

  if (!valent_device_get_paired (device))
    {
      gtk_label_set_label (GTK_LABEL (info->status), _("Unpaired"));
      gtk_style_context_remove_class (style, "dim-label");
    }
  else if (!valent_device_get_connected (device))
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
on_device_added (ValentManager *manager,
                 ValentDevice  *device,
                 ValentWindow  *self)
{
  DeviceInfo *info;
  const char *id;
  const char *name;
  const char *icon_name;
  g_autofree char *path = NULL;
  GtkStackPage *page;
  GtkWidget *box;
  GtkWidget *arrow;

  g_assert (VALENT_IS_MANAGER (manager));
  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_WINDOW (self));

  info = g_new0 (DeviceInfo, 1);
  g_hash_table_insert (self->devices, device, info);

  id = valent_device_get_id (device);
  name = valent_device_get_name (device);
  icon_name = valent_device_get_icon_name (device);

  /* Panel */
  info->panel = valent_device_panel_new (device);
  page = gtk_stack_add_titled (self->stack, info->panel, id, name);
  g_object_bind_property (device, "name",
                          page,   "title",
                          G_BINDING_SYNC_CREATE);

  /* Row */
  path = g_strdup_printf ("/%s", id);
  info->row = g_object_new (ADW_TYPE_ACTION_ROW,
                            "action-name",   "win.page",
                            "action-target", g_variant_new_string (path),
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
                    "notify::connected",
                    G_CALLBACK (on_device_changed),
                    self);
  g_signal_connect (device,
                    "notify::paired",
                    G_CALLBACK (on_device_changed),
                    self);
  on_device_changed (device, NULL, self);

  gtk_list_box_insert (self->device_list, info->row, -1);
}

static void
on_device_removed (ValentManager *manager,
                   ValentDevice  *device,
                   ValentWindow  *self)
{
  DeviceInfo *info;

  if ((info = g_hash_table_lookup (self->devices, device)) == NULL)
    return;

  if (gtk_stack_get_visible_child (self->stack) == info->panel)
    {
      g_queue_clear_full (self->history, g_free);
      g_queue_push_tail (self->history, g_strdup ("/main"));
      gtk_stack_set_visible_child_name (self->stack, "main");
    }

  g_signal_handlers_disconnect_by_data (device, self);
  gtk_list_box_remove (self->device_list, info->row);
  gtk_stack_remove (self->stack, info->panel);

  g_hash_table_remove (self->devices, device);
}

/*
 * History
 */
static GtkStack *
find_stack (GtkWidget *widget)
{
  GtkWidget *child;

  if (GTK_IS_STACK (widget))
    return GTK_STACK (widget);

  child = gtk_widget_get_first_child (widget);

  while (child && !GTK_IS_STACK (child))
    {
      GtkStack *stack;

      if ((stack = find_stack (child)))
        return stack;

      child = gtk_widget_get_next_sibling (child);
    }

  return GTK_STACK (child);
}

static GtkStack *
find_stack_by_page_name (GtkWidget  *widget,
                         const char *name)
{
  GtkWidget *child;

  if (GTK_IS_STACK (widget) &&
      gtk_stack_get_child_by_name (GTK_STACK (widget), name))
    return GTK_STACK (widget);

  child = gtk_widget_get_first_child (widget);

  while (child)
    {
      GtkStack *stack;

      if ((stack = find_stack_by_page_name (child, name)))
        return stack;

      child = gtk_widget_get_next_sibling (child);
    }

  return GTK_STACK (child);
}

static void
valent_window_set_location (ValentWindow *self,
                            const char   *path)
{
  GtkStack *stack = NULL;
  GtkWidget *child;
  g_auto (GStrv) segments = NULL;
  const char *context = NULL;
  const char *name = NULL;
  g_autofree char *full_path = NULL;

  g_assert (VALENT_IS_WINDOW (self));
  g_assert (path != NULL);

  /* Paths can be absolute or relative */
  if (path[0] == '/')
    {
      segments = g_strsplit (path, "/", -1);
      context = segments[1];
      name = segments[2];

      if ((child = gtk_stack_get_child_by_name (self->stack, context)))
        {
          if (name != NULL && (stack = find_stack_by_page_name (child, name)))
            name = segments[2];

          else if (name == NULL && (stack = find_stack (child)))
            name = gtk_stack_get_visible_child_name (stack);

          else
            name = NULL;
        }
    }
  else
    {
      /* If @path is a top-level page name, it is the context and we need to
       * find the child stack's page name (if there is one).
       */
      if ((child = gtk_stack_get_child_by_name (self->stack, path)))
        {
          context = path;

          if ((stack = find_stack (child)))
            name = gtk_stack_get_visible_child_name (stack);
        }

      /* Otherwise it should be a child of the currently visible context */
      else if ((child = gtk_stack_get_visible_child (self->stack)))
        {
          context = gtk_stack_get_visible_child_name (self->stack);

          if ((stack = find_stack_by_page_name (child, path)))
            name = path;
        }
    }

  /* Compute the full path */
  if (name)
    full_path = g_strdup_printf ("/%s/%s", context, name);
  else
    full_path = g_strdup_printf ("/%s", context);


  /* Set the stacks */
  gtk_stack_set_visible_child_name (self->stack, context);

  if (name != NULL && stack != NULL)
    gtk_stack_set_visible_child_name (stack, name);

  /* Save new location to history */
  if (g_strcmp0 (full_path, g_queue_peek_tail (self->history)) != 0)
    g_queue_push_tail (self->history, g_steal_pointer (&full_path));

  g_debug ("[%s] %s => %s", G_STRFUNC, full_path, path);
}

/*
 * GActions
 */
static void
about_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);

  g_assert (GTK_IS_WINDOW (window));

  const char *authors[] = {
    "Andy Holmes <andrew.g.r.holmes@gmail.com>",
    NULL
  };

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
page_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentWindow *self = VALENT_WINDOW (user_data);
  const char *path;

  g_assert (VALENT_IS_WINDOW (self));

  /* Set the page */
  path = g_variant_get_string (parameter, NULL);
  valent_window_set_location (self, path);
}

static void
previous_action (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  ValentWindow *self = VALENT_WINDOW (user_data);
  g_autofree char *current = NULL;
  const char *prev;

  g_assert (VALENT_IS_WINDOW (self));

  /* Free the current state and restore the previous one */
  current = g_queue_pop_tail (self->history);
  prev = g_queue_peek_tail (self->history);
  valent_window_set_location (self, prev);
}

static gboolean
refresh_cb (gpointer data)
{
  ValentWindow *self = VALENT_WINDOW (data);

  gtk_spinner_set_spinning (self->device_list_spinner, FALSE);
  self->device_list_spinner_id = 0;

  return G_SOURCE_REMOVE;
}

static void
refresh_action (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  ValentWindow *self = VALENT_WINDOW (user_data);

  g_assert (VALENT_IS_WINDOW (self));

  if (self->device_list_spinner_id > 0)
    return;

  gtk_spinner_set_spinning (self->device_list_spinner, TRUE);
  valent_manager_identify (self->manager, NULL);

  self->device_list_spinner_id = g_timeout_add_seconds (5, refresh_cb, self);
}

static const GActionEntry actions[] = {
  {"about",    about_action,    NULL, NULL, NULL},
  {"page",     page_action,     "s",  NULL, NULL},
  {"previous", previous_action, NULL, NULL, NULL},
  {"refresh",  refresh_action,  NULL, NULL, NULL}
};

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
  devices = valent_manager_get_devices (self->manager);

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

  g_clear_handle_id (&self->device_list_spinner_id, g_source_remove);
  g_signal_handlers_disconnect_by_func (self->manager, on_device_added, self);
  g_signal_handlers_disconnect_by_func (self->manager, on_device_removed, self);

  G_OBJECT_CLASS (valent_window_parent_class)->dispose (object);
}

static void
valent_window_finalize (GObject *object)
{
  ValentWindow *self = VALENT_WINDOW (object);
  GHashTableIter iter;
  gpointer device;

  g_hash_table_iter_init (&iter, self->devices);

  while (g_hash_table_iter_next (&iter, &device, NULL))
    g_signal_handlers_disconnect_by_data (device, self);
  g_clear_pointer (&self->devices, g_hash_table_unref);

  g_clear_object (&self->settings);
  g_queue_free_full (self->history, g_free);

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
    case PROP_MANAGER:
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
    case PROP_MANAGER:
      self->manager = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
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

  /* Navigation History */
  self->history = g_queue_new ();
  g_queue_push_tail (self->history, g_strdup ("/main"));

  /* Action Group */
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  /* Devices Page */
  gtk_list_box_set_sort_func (self->device_list, device_sort_func, NULL, NULL);
  self->devices = g_hash_table_new_full (NULL, NULL, NULL, g_free);

  /* Settings Page */
  self->settings = g_settings_new ("ca.andyholmes.Valent");
  g_settings_bind (self->settings,     "name",
                   self->rename_label, "label",
                   G_SETTINGS_BIND_DEFAULT);
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

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-window.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, stack);
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, device_list);
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, device_list_spinner);

  gtk_widget_class_bind_template_child (widget_class, ValentWindow, rename_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, rename_label);
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, rename_dialog);
  gtk_widget_class_bind_template_child (widget_class, ValentWindow, rename_button);

  gtk_widget_class_bind_template_callback (widget_class, on_rename_dialog_open);
  gtk_widget_class_bind_template_callback (widget_class, on_rename_dialog_response);
  gtk_widget_class_bind_template_callback (widget_class, on_rename_entry_changed);

  /**
   * ValentWindow:manager:
   *
   * The #ValentManager that the window represents.
   */
  properties [PROP_MANAGER] =
    g_param_spec_object ("manager",
                         "Manager",
                         "The manager for this window",
                         VALENT_TYPE_MANAGER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

