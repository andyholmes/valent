// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-lan-dnssd.h"


typedef struct
{
  GMainLoop  *loop;
  JsonNode   *packets;
  GListModel *dnssd;

  gpointer    data;
} LanDNSSDFixture;

static void
lan_dnssd_fixture_set_up (LanDNSSDFixture *fixture,
                          gconstpointer    user_data)
{
  JsonNode *identity;

  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->packets = valent_test_load_json ("plugin-lan.json");

  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");
  fixture->dnssd = valent_lan_dnssd_new (identity);
}

static void
lan_dnssd_fixture_tear_down (LanDNSSDFixture *fixture,
                             gconstpointer    user_data)
{
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->packets, json_node_unref);

  v_await_finalize_object (fixture->dnssd);
}

static void
on_items_changed (GListModel   *list,
                  unsigned int  position,
                  unsigned int  removed,
                  unsigned int  added,
                  gboolean     *done)
{
  g_autoptr (GObject) item = NULL;

  // position 0 is the local service
  if (position == 0 && removed == 1)
    {
      item = g_list_model_get_item (list, position);
      g_assert_false (G_IS_SOCKET_ADDRESS (item));
    }

  if (position == 0 && added == 1)
    {
      item = g_list_model_get_item (list, position);
      g_assert_true (G_IS_SOCKET_ADDRESS (item));
    }

  if (done != NULL)
    *done = TRUE;
}

static void
test_lan_dnssd_basic (LanDNSSDFixture *fixture,
                      gconstpointer    user_data)
{
  JsonNode *identity = NULL;
  gboolean done = FALSE;

  VALENT_TEST_CHECK ("DNS-SD adapter registers the service");
  g_signal_connect (fixture->dnssd,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    &done);
  valent_lan_dnssd_attach (VALENT_LAN_DNSSD (fixture->dnssd), NULL);
  valent_test_await_boolean (&done);

  VALENT_TEST_CHECK ("DNS-SD adapter updates the service TXT record");
  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");
  g_object_set (fixture->dnssd,
                "identity", identity,
                NULL);

  valent_test_await_timeout (1);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_type_ensure (VALENT_TYPE_LAN_DNSSD);

  g_test_add ("/plugins/lan/channel",
              LanDNSSDFixture, NULL,
              lan_dnssd_fixture_set_up,
              test_lan_dnssd_basic,
              lan_dnssd_fixture_tear_down);

  return g_test_run ();
}
