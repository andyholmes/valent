// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

static void
test_resource_basic (void)
{
  g_autoptr (ValentResource) resource = NULL;

  g_auto (GStrv) contributor = NULL;
  g_autofree char *coverage = g_uuid_string_random ();
  g_autofree char *creator = g_uuid_string_random ();
  g_autoptr (GDateTime) date = g_date_time_new_now_local ();
  g_autofree char *description = g_uuid_string_random ();
  g_autofree char *format = g_uuid_string_random ();
  g_autofree char *identifier = g_uuid_string_random ();
  g_autofree char *language = g_uuid_string_random ();
  g_autofree char *publisher = g_uuid_string_random ();
  g_auto (GStrv) relation = NULL;
  g_autofree char *rights = g_uuid_string_random ();
  g_autoptr (ValentResource) source = g_object_new (VALENT_TYPE_RESOURCE, NULL);
  g_autofree char *subject = g_uuid_string_random ();
  g_autofree char *title = g_uuid_string_random ();
  g_autofree char *type_hint = g_uuid_string_random ();

  g_auto (GStrv) contributor_out = NULL;
  g_autofree char *coverage_out = NULL;
  g_autofree char *creator_out = NULL;
  g_autoptr (GDateTime) date_out = NULL;
  g_autofree char *description_out = NULL;
  g_autofree char *format_out = NULL;
  g_autofree char *identifier_out = NULL;
  g_autofree char *language_out = NULL;
  g_autofree char *publisher_out = NULL;
  g_auto (GStrv) relation_out = NULL;
  g_autofree char *rights_out = NULL;
  g_autoptr (ValentResource) source_out = NULL;
  g_autofree char *subject_out = NULL;
  g_autofree char *title_out = NULL;
  g_autofree char *type_hint_out = NULL;

  VALENT_TEST_CHECK ("Object can be constructed");
  resource = g_object_new (VALENT_TYPE_RESOURCE,
                           "contributor", contributor,
                           "coverage",    coverage,
                           "creator",     creator,
                           "date",        date,
                           "description", description,
                           "format",      format,
                           "identifier",  identifier,
                           "language",    language,
                           "publisher",   publisher,
                           "relation",    relation,
                           "rights",      rights,
                           "source",      source,
                           "subject",     subject,
                           "title",       title,
                           "type-hint",   type_hint,
                           NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (resource,
                "contributor", &contributor_out,
                "coverage",    &coverage_out,
                "creator",     &creator_out,
                "date",        &date_out,
                "description", &description_out,
                "format",      &format_out,
                "identifier",  &identifier_out,
                "language",    &language_out,
                "publisher",   &publisher_out,
                "relation",    &relation_out,
                "rights",      &rights_out,
                "source",      &source_out,
                "subject",     &subject_out,
                "title",       &title_out,
                "type-hint",   &type_hint_out,
                NULL);

  g_assert_true (contributor == contributor_out ||
                 g_strv_equal ((const char * const *)contributor,
                               (const char * const *)contributor_out));
  g_assert_cmpstr (coverage, ==, coverage_out);
  g_assert_cmpstr (creator, ==, creator_out);
  g_assert_true (g_date_time_equal (date, date_out));
  g_assert_cmpstr (description, ==, description_out);
  g_assert_cmpstr (format, ==, format_out);
  g_assert_cmpstr (identifier, ==, identifier_out);
  g_assert_cmpstr (language, ==, language_out);
  g_assert_cmpstr (publisher, ==, publisher_out);
  g_assert_true (relation == relation_out ||
                 g_strv_equal ((const char * const *)relation,
                               (const char * const *)relation_out));
  g_assert_cmpstr (rights, ==, rights_out);
  g_assert_true (source == source_out);
  g_assert_cmpstr (subject, ==, subject_out);
  g_assert_cmpstr (title, ==, title_out);
  g_assert_cmpstr (type_hint, ==, type_hint_out);

  g_assert_true (valent_resource_get_contributor (resource) == contributor_out ||
                 g_strv_equal ((const char * const *)valent_resource_get_contributor (resource),
                               (const char * const *)contributor_out));
  g_assert_cmpstr (valent_resource_get_coverage (resource), ==, coverage_out);
  g_assert_cmpstr (valent_resource_get_creator (resource), ==, creator_out);
  g_assert_true (g_date_time_equal (valent_resource_get_date (resource), date_out));
  g_assert_cmpstr (valent_resource_get_description (resource), ==, description_out);
  g_assert_cmpstr (valent_resource_get_format (resource), ==, format_out);
  g_assert_cmpstr (valent_resource_get_identifier (resource), ==, identifier_out);
  g_assert_cmpstr (valent_resource_get_language (resource), ==, language_out);
  g_assert_cmpstr (valent_resource_get_publisher (resource), ==, publisher_out);
  g_assert_true (valent_resource_get_relation (resource) == relation_out ||
                 g_strv_equal ((const char * const *)valent_resource_get_relation (resource),
                               (const char * const *)relation_out));
  g_assert_cmpstr (valent_resource_get_rights (resource), ==, rights_out);
  g_assert_true (valent_resource_get_source (resource) == source_out);
  g_assert_cmpstr (valent_resource_get_subject (resource), ==, subject_out);
  g_assert_cmpstr (valent_resource_get_title (resource), ==, title_out);
  g_assert_cmpstr (valent_resource_get_type_hint (resource), ==, type_hint_out);
}

static void
test_resource_update (void)
{
  g_autoptr (ValentResource) resource = NULL;
  g_autoptr (ValentResource) source = NULL;

  resource = g_object_new (VALENT_TYPE_RESOURCE, NULL);
  source = g_object_new (VALENT_TYPE_RESOURCE, NULL);

  valent_resource_update (resource, source);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/core/resource/basic",
                   test_resource_basic);

  g_test_add_func ("/libvalent/core/resource/update",
                   test_resource_update);

  g_test_run ();
}

