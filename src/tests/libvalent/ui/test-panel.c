#include <libvalent-test.h>
#include <libvalent-ui.h>


static void
test_panel_basic (void)
{
  g_autoptr (ValentPanel) panel = NULL;
  const char *title;
  const char *icon_name;
  GtkWidget *widget = NULL;

  panel = g_object_ref_sink (VALENT_PANEL (valent_panel_new ()));
  g_assert_nonnull (panel);

  /* Properties */
  g_object_set (panel,
               "title", "Test Title",
               "icon-name", "dialog-information-symbolic",
               NULL);

  title = valent_panel_get_title (panel);
  g_assert_cmpstr (title, ==, "Test Title");

  icon_name = valent_panel_get_icon_name (panel);
  g_assert_cmpstr (icon_name, ==, "dialog-information-symbolic");

  /* Header */
  widget = gtk_button_new ();
  valent_panel_set_header (panel, widget);
  g_assert_nonnull (valent_panel_get_header (panel));

  valent_panel_set_header (panel, NULL);
  g_assert_null (valent_panel_get_header (panel));

  /* Body */
  widget = gtk_button_new ();
  valent_panel_append (panel, widget);
  g_assert_true (gtk_widget_is_ancestor (widget, GTK_WIDGET (panel)));

  widget = gtk_button_new ();
  valent_panel_prepend (panel, widget);
  g_assert_true (gtk_widget_is_ancestor (widget, GTK_WIDGET (panel)));

  /* Footer */
  widget = gtk_button_new ();
  valent_panel_set_footer (panel, widget);
  g_assert_nonnull (valent_panel_get_footer (panel));

  valent_panel_set_footer (panel, NULL);
  g_assert_null (valent_panel_get_footer (panel));
}

int
main (int argc,
     char *argv[])
{
  valent_test_ui_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/libvalent/ui/panel", test_panel_basic);

  return g_test_run ();
}
