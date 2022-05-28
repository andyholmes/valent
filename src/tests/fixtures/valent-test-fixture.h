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
} ValentTestFixture;


GType               valent_test_fixture_get_type      (void) G_GNUC_CONST;

ValentTestFixture * valent_test_fixture_new           (const char         *path);
ValentTestFixture * valent_test_fixture_ref           (ValentTestFixture  *fixture);
void                valent_test_fixture_unref         (ValentTestFixture  *fixture);

void                valent_test_fixture_init          (ValentTestFixture  *fixture,
                                                       gconstpointer       user_data);
void                valent_test_fixture_init_settings (ValentTestFixture  *fixture,
                                                       const char         *name);
void                valent_test_fixture_clear         (ValentTestFixture  *fixture,
                                                       gconstpointer      user_data);
void                valent_test_fixture_connect       (ValentTestFixture  *fixture,
                                                       gboolean            connected);
void                valent_test_fixture_run           (ValentTestFixture  *fixture);
void                valent_test_fixture_quit          (ValentTestFixture  *fixture);
void                valent_test_fixture_wait          (ValentTestFixture  *fixture,
                                                       unsigned int        interval);
gpointer            valent_test_fixture_get_data      (ValentTestFixture  *fixture);
void                valent_test_fixture_set_data      (ValentTestFixture  *fixture,
                                                       gpointer            data,
                                                       GDestroyNotify      data_free);
ValentDevice      * valent_test_fixture_get_device    (ValentTestFixture  *fixture);
GSettings         * valent_test_fixture_get_settings  (ValentTestFixture  *fixture);

JsonNode          * valent_test_fixture_expect_packet (ValentTestFixture  *fixture);
void                valent_test_fixture_handle_packet (ValentTestFixture  *fixture,
                                                       JsonNode           *packet);
JsonNode          * valent_test_fixture_lookup_packet (ValentTestFixture  *fixture,
                                                       const char         *name);
gboolean            valent_test_fixture_download      (ValentTestFixture  *fixture,
                                                       JsonNode           *packet,
                                                       GError            **error);
gboolean            valent_test_fixture_upload        (ValentTestFixture  *fixture,
                                                       JsonNode           *packet,
                                                       GFile              *file,
                                                       GError            **error);
void                valent_test_fixture_schema_fuzz   (ValentTestFixture  *fixture,
                                                       const char         *path);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ValentTestFixture, valent_test_fixture_unref)

G_END_DECLS
