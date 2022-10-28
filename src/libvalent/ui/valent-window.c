// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-window"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-application-credits.h"
#include "valent-device-page.h"
#include "valent-preferences-window.h"
#include "valent-window.h"


struct _ValentWindow
{
  AdwApplicationWindow  parent_instance;
  ValentDeviceManager  *manager;

  /* template */
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
 * ValentDevice Callbacks
 */
static void
on_device_changed (ValentDevice *device,
                   GParamSpec   *pspec,
                   GtkWidget    *status)
{
  ValentDeviceState state;
  GtkStyleContext *style;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (GTK_IS_LABEL (status));

  state = valent_device_get_state (device);
  style = gtk_widget_get_style_context (GTK_WIDGET (status));

  if ((state & VALENT_DEVICE_STATE_PAIRED) == 0)
    {
      gtk_label_set_label (GTK_LABEL (status), _("Unpaired"));
      gtk_style_context_remove_class (style, "dim-label");
    }
  else if ((state & VALENT_DEVICE_STATE_CONNECTED) == 0)
    {
      gtk_label_set_label (GTK_LABEL (status), _("Disconnected"));
      gtk_style_context_add_class (style, "dim-label");
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (status), _("Connected"));
      gtk_style_context_remove_class (style, "dim-label");
    }
}

static void
on_row_destroy (GtkWidget *row,
                GtkWidget *panel)
{
  GtkWidget *stack = NULL;

  g_assert (GTK_IS_WIDGET (row));
  g_assert (GTK_IS_WIDGET (panel));

  if ((stack = gtk_widget_get_ancestor (panel, GTK_TYPE_STACK)) != NULL)
    gtk_stack_remove (GTK_STACK (stack), panel);
}

static GtkWidget *
valent_window_create_row_func (gpointer item,
                               gpointer user_data)
{
  ValentWindow *self = VALENT_WINDOW (user_data);
  ValentDevice *device = VALENT_DEVICE (item);
  const char *device_id;
  const char *name;
  GtkStackPage *page;
  GtkWidget *panel;
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *status;
  GtkWidget *arrow;

  g_assert (VALENT_IS_WINDOW (self));
  g_assert (VALENT_IS_DEVICE (item));

  device_id = valent_device_get_id (device);
  name = valent_device_get_name (device);

  /* Row */
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "action-name",   "win.page",
                      "action-target", g_variant_new_string (device_id),
                      "activatable",   TRUE,
                      "selectable",    FALSE,
                      NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), box);

  status = g_object_new (GTK_TYPE_LABEL, NULL);
  gtk_box_append (GTK_BOX (box), status);

  arrow = gtk_image_new_from_icon_name ("go-next-symbolic");
  gtk_style_context_add_class (gtk_widget_get_style_context (arrow), "dim-label");
  gtk_box_append (GTK_BOX (box), arrow);

  /* Bind to device */
  g_object_bind_property (device, "name",
                          row,    "title",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (device, "icon-name",
                          row,    "icon-name",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (device,
                           "notify::state",
                           G_CALLBACK (on_device_changed),
                           status, 0);
  on_device_changed (device, NULL, status);

  /* Panel */
  panel = g_object_new (VALENT_TYPE_DEVICE_PAGE,
                        "device", device,
                        NULL);
  page = gtk_stack_add_titled (self->stack, panel, device_id, name);
  g_object_bind_property (device, "name",
                          page,   "title",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (device, "icon-name",
                          page,   "icon-name",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_object (row,
                           "destroy",
                           G_CALLBACK (on_row_destroy),
                           panel, 0);

  return g_steal_pointer (&row);
}

static void
valent_window_close_preferences (ValentWindow *self)
{
  GtkWidget *visible_child;

  g_assert (VALENT_IS_WINDOW (self));

  visible_child = gtk_stack_get_visible_child (self->stack);

  if (VALENT_IS_DEVICE_PAGE (visible_child))
    valent_device_page_close_preferences (VALENT_DEVICE_PAGE (visible_child));

  g_clear_pointer (&self->preferences, gtk_window_destroy);
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
  GtkWindow *dialog = NULL;
  static const char *version = NULL;

  g_assert (GTK_IS_WINDOW (window));

  if (version == NULL)
    {
      if (g_strcmp0 (PROFILE_NAME, "devel") == 0)
        version = PACKAGE_VERSION"+"VALENT_VCS_VERSION;
      else
        version = PACKAGE_VERSION;
    }

  dialog = g_object_new (ADW_TYPE_ABOUT_WINDOW,
                         "application-icon",  APPLICATION_ID,
                         "application-name",   _("Valent"),
                         "copyright",          "© 2022 Andy Holmes",
                         /* TODO: "issue-url",          PACKAGE_BUGREPORT, */
                         "license-type",       GTK_LICENSE_GPL_3_0,
                         "developers",         valent_application_credits_developers,
                         "documenters",        valent_application_credits_documenters,
                         "transient-for",      window,
                         "translator-credits", _("translator-credits"),
                         "version",            version,
                         "website",            PACKAGE_URL,
                         NULL);
  adw_about_window_add_acknowledgement_section (ADW_ABOUT_WINDOW (dialog),
                                                _("Sponsors"),
                                                valent_application_credits_sponsors);

  gtk_window_present (dialog);
}

static void
page_action (GtkWidget  *widget,
             const char *action_name,
             GVariant   *parameter)
{
  ValentWindow *self = VALENT_WINDOW (widget);
  const char *name;
  GtkWidget *page;

  g_assert (VALENT_IS_WINDOW (self));

  name = g_variant_get_string (parameter, NULL);
  page = gtk_stack_get_child_by_name (self->stack, name);

  if (page == NULL)
    page = gtk_stack_get_child_by_name (self->stack, "main");

  valent_window_close_preferences (self);
  gtk_stack_set_visible_child (self->stack, page);
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

  valent_device_manager_refresh (self->manager);
  self->refresh_id = g_timeout_add_seconds (5, refresh_cb, self);
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

  g_clear_pointer (&self->preferences, gtk_window_destroy);
  g_clear_handle_id (&self->refresh_id, g_source_remove);

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
  gtk_widget_class_install_action (widget_class, "win.page", "s", page_action);
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
}

