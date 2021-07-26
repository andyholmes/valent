// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

typedef struct
{
  GMainLoop      *loop;
  JsonNode       *packets;
  ValentDevice   *device;
  GSettings      *settings;
  ValentChannel  *channel;
  ValentChannel  *endpoint;
  gpointer        data;
  GDestroyNotify  data_free;
} ValentTestPluginFixture;

GType          valent_test_plugin_fixture_get_type      (void) G_GNUC_CONST;

void           valent_test_plugin_fixture_init          (ValentTestPluginFixture  *fixture,
                                                         gconstpointer             user_data);
void           valent_test_plugin_fixture_clear         (ValentTestPluginFixture  *fixture,
                                                         gconstpointer             user_data);

void           valent_test_plugin_fixture_init_settings (ValentTestPluginFixture  *fixture,
                                                         const char               *name);
void           valent_test_plugin_fixture_connect       (ValentTestPluginFixture  *fixture,
                                                         gboolean                  connected);
void           valent_test_plugin_fixture_run           (ValentTestPluginFixture  *fixture);
void           valent_test_plugin_fixture_quit          (ValentTestPluginFixture  *fixture);

gpointer       valent_test_plugin_fixture_get_data      (ValentTestPluginFixture  *fixture);
void           valent_test_plugin_fixture_set_data      (ValentTestPluginFixture  *fixture,
                                                         gpointer                  data,
                                                         GDestroyNotify            data_free);
ValentDevice * valent_test_plugin_fixture_get_device    (ValentTestPluginFixture  *fixture);
GSettings    * valent_test_plugin_fixture_get_settings  (ValentTestPluginFixture  *fixture);

JsonNode     * valent_test_plugin_fixture_expect_packet (ValentTestPluginFixture  *fixture);
void           valent_test_plugin_fixture_handle_packet (ValentTestPluginFixture  *fixture,
                                                         JsonNode                 *packet);
JsonNode     * valent_test_plugin_fixture_lookup_packet (ValentTestPluginFixture  *fixture,
                                                         const char               *name);
gboolean       valent_test_plugin_fixture_download      (ValentTestPluginFixture  *fixture,
                                                         JsonNode                 *packet,
                                                         GError                  **error);
gboolean       valent_test_plugin_fixture_upload        (ValentTestPluginFixture  *fixture,
                                                         JsonNode                 *packet,
                                                         GFile                    *file,
                                                         GError                  **error);
gboolean       valent_test_plugin_fixture_schema_fuzz   (ValentTestPluginFixture  *fixture,
                                                         const char               *path);


ValentTestPluginFixture * valent_test_plugin_fixture_new             (const char     *path);
ValentTestPluginFixture * valent_test_plugin_fixture_copy            (ValentTestPluginFixture  *fixture);
void                      valent_test_plugin_fixture_free            (gpointer        data);

ValentChannel * valent_test_plugin_fixture_get_endpoint (ValentTestPluginFixture  *fixture);

G_END_DECLS
