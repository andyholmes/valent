// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-lan-dnssd.h"

#define DNSSD_SERVICE_TYPE "_kdeconnect-test._udp"


typedef struct
{
  JsonNode   *packets;
  GListModel *dnssd;

  GPtrArray  *data;
  int         state;
} LanDNSSDFixture;

static void
lan_dnssd_fixture_set_up (LanDNSSDFixture *fixture,
                          gconstpointer    user_data)
{
  JsonNode *identity;

  fixture->packets = valent_test_load_json ("plugin-lan.json");

  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");
  fixture->dnssd = g_object_new (VALENT_TYPE_LAN_DNSSD,
                                 "identity",     identity,
                                 "service-type", DNSSD_SERVICE_TYPE,
                                 NULL);
  fixture->data = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
lan_dnssd_fixture_tear_down (LanDNSSDFixture *fixture,
                             gconstpointer    user_data)
{
  v_await_finalize_object (fixture->dnssd);

  g_clear_pointer (&fixture->packets, json_node_unref);
  g_clear_pointer (&fixture->data, g_ptr_array_unref);
}

static void
on_items_changed (GListModel      *list,
                  unsigned int     position,
                  unsigned int     removed,
                  unsigned int     added,
                  LanDNSSDFixture *fixture)
{
  if (position == 0 && removed == 1)
    {
      g_assert_cmpuint (fixture->data->len, >=, removed);
      g_assert_nonnull (g_ptr_array_remove_index (fixture->data, position));
    }

  if (position == 0 && added == 1)
    {
      GSocketAddress *item = g_list_model_get_item (list, position);

      g_assert_true (G_IS_SOCKET_ADDRESS (item));
      g_ptr_array_insert (fixture->data, position, item);
    }

  fixture->state = TRUE;
}

static void
test_lan_dnssd_basic (LanDNSSDFixture *fixture,
                      gconstpointer    user_data)
{
  g_autoptr (JsonNode) identity_out = NULL;
  g_autofree char *service_type = NULL;
  JsonNode *identity = NULL;

  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->dnssd,
                "identity",     &identity_out,
                "service-type", &service_type,
                NULL);
  g_assert_cmpstr (DNSSD_SERVICE_TYPE, ==, service_type);
  g_assert_true (json_node_equal (identity, identity_out));

  VALENT_TEST_CHECK ("DNS-SD adapter registers the service");
  g_signal_connect (fixture->dnssd,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    fixture);
  valent_lan_dnssd_attach (VALENT_LAN_DNSSD (fixture->dnssd), NULL);
  valent_test_await_boolean (&fixture->state);

  VALENT_TEST_CHECK ("DNS-SD adapter updates the service TXT record");
  g_object_set (fixture->dnssd,
                "identity", identity,
                NULL);
  valent_test_await_timeout (1);

  VALENT_TEST_CHECK ("DNS-SD adapter unregisters the service");
  g_object_set (fixture->dnssd,
                "identity", NULL,
                NULL);
  valent_test_await_boolean (&fixture->state);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_type_ensure (VALENT_TYPE_LAN_DNSSD);

  g_test_add ("/plugins/lan/dnssd",
              LanDNSSDFixture, NULL,
              lan_dnssd_fixture_set_up,
              test_lan_dnssd_basic,
              lan_dnssd_fixture_tear_down);

  return g_test_run ();
}
