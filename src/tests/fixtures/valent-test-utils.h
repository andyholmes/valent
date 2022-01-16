// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <glib.h>
#include <json-glib/json-glib.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

void             valent_test_ui_init      (int    *argcp,
                                           char ***argvp,
                                           ...);

gboolean         valent_test_mute_domain  (const char     *log_domain,
                                           GLogLevelFlags  log_level,
                                           const char     *message,
                                           gpointer        user_data);

gboolean         valent_test_mute_match   (const char     *log_domain,
                                           GLogLevelFlags  log_level,
                                           const char     *message,
                                           gpointer        user_data);

gboolean         valent_test_mute_warning (const char     *log_domain,
                                           GLogLevelFlags  log_level,
                                           const char     *message,
                                           gpointer        user_data);
gboolean         valent_test_mute_fuzzing (const char     *log_domain,
                                           GLogLevelFlags  log_level,
                                           const char     *message,
                                           gpointer        user_data);

gpointer         valent_test_event_pop    (void);
void             valent_test_event_push   (gpointer        data);
void             valent_test_event_free   (GDestroyNotify  free_func);

JsonNode       * valent_test_load_json    (const char     *path);
ValentChannel ** valent_test_channels     (JsonNode       *identity,
                                           JsonNode       *peer_identity);
gboolean         valent_test_download     (ValentChannel  *rig,
                                           JsonNode       *packet,
                                           GError        **error);
gboolean         valent_test_upload       (ValentChannel  *channel,
                                           JsonNode       *packet,
                                           GFile          *file,
                                           GError        **error);

#define valent_test_event_cmpstr(str)       \
  G_STMT_START {                            \
    char *event = valent_test_event_pop (); \
    g_assert_cmpstr (event, ==, str);        \
    g_free (event);                         \
  } G_STMT_END

/**
 * v_assert_finalize_object:
 * @object: (type GObject.Object): a #GObject
 *
 * Assert that @object is non-%NULL, then release one reference to it with
 * g_object_unref() and assert that it has been finalized (i.e. that there
 * are no more references).
 *
 * This macro is like g_assert_finalize_object(), but with the advantage of
 * printing the location and variable name of @object.
 */
#define v_assert_finalize_object(object)                                \
  G_STMT_START {                                                        \
    gpointer weak_pointer = object;                                     \
                                                                        \
    if G_UNLIKELY (!G_IS_OBJECT (weak_pointer))                         \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "'" #object "' should be a GObject");        \
                                                                        \
    g_object_add_weak_pointer ((GObject *)object, &weak_pointer);       \
    g_object_unref (weak_pointer);                                      \
                                                                        \
    if G_UNLIKELY ((weak_pointer) != NULL)                              \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "'" #object "' should be finalized");        \
  } G_STMT_END

/**
 * v_await_finalize_object:
 * @object: (type GObject.Object): a #GObject
 *
 * Assert that @object is non-%NULL, then iterate the main context until its
 * reference count reaches `1`. Then release one reference to it with
 * g_object_unref() and assert that it has been finalized.
 */
static inline void
(v_await_finalize_object) (GObject *object)
{
  gpointer weak_pointer = object;

  g_assert_true (G_IS_OBJECT (weak_pointer));

  while (g_atomic_int_get (&object->ref_count) > 1)
    g_main_context_iteration (NULL, FALSE);

  g_object_add_weak_pointer (object, &weak_pointer);
  g_object_unref (weak_pointer);
  g_assert_null (weak_pointer);
}
#define v_await_finalize_object(object) (v_await_finalize_object ((GObject *) object))

/**
 * v_assert_packet_type:
 * @p: a #JsonNode
 * @m: a member name
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_type(p, t)                                      \
  G_STMT_START {                                                        \
    const char *type = valent_packet_get_type (p);                      \
    g_assert_cmpstr (type, ==, t);                                      \
  } G_STMT_END

/**
 * v_assert_packet_field:
 * @p: a #JsonNode
 * @m: a member name
 *
 * Assert the body object of @p has the member @m.
 */
#define v_assert_packet_field(p, m)                                     \
  G_STMT_START {                                                        \
    JsonObject *body = valent_packet_get_body (p);                      \
    g_assert_true (json_object_has_member (body, m));                   \
  } G_STMT_END

/**
 * v_assert_packet_no_field:
 * @p: a #JsonNode
 * @m: a member name
 *
 * Assert the body object of @p does not have the member @m.
 */
#define v_assert_packet_no_field(p, m)                                  \
  G_STMT_START {                                                        \
    JsonObject *body = valent_packet_get_body (p);                      \
    g_assert_false (json_object_has_member (body, m));                  \
  } G_STMT_END

/**
 * v_assert_packet_true:
 * @p: a #JsonNode
 * @m: a member name
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_true(p, m)                                      \
  G_STMT_START {                                                        \
    JsonObject *body = valent_packet_get_body (p);                      \
    if G_UNLIKELY (!json_object_has_member (body, m))                   \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "missing '"#m"' member");                    \
                                                                        \
    if (!json_object_get_boolean_member (body, m))                      \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "'" #m "' should be TRUE");                  \
  } G_STMT_END

/**
 * v_assert_packet_false:
 * @p: a #JsonNode
 * @m: a member name
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_false(p, m)                                     \
  G_STMT_START {                                                        \
    JsonObject *body = valent_packet_get_body (p);                      \
    if G_UNLIKELY (!json_object_has_member (body, m))                   \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "missing '"#m"' member");                    \
                                                                        \
    if (json_object_get_boolean_member (body, m))                       \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "'" #m "' should be FALSE");                 \
  } G_STMT_END

/**
 * v_assert_packet_cmpfloat:
 * @p: a #JsonNode
 * @m: a member name
 * @cmp: comparison operator
 * @num: expected value
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_cmpfloat(p, m, cmp, num)                        \
  G_STMT_START {                                                        \
    JsonObject *body = valent_packet_get_body (p);                      \
    if G_UNLIKELY (!json_object_has_member (body, m))                   \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "missing '"#m"' member");                    \
                                                                        \
    double member = json_object_get_double_member (body, m);            \
    g_assert_cmpfloat (member, cmp, num);                               \
  } G_STMT_END

/**
 * v_assert_packet_cmpint:
 * @p: a #JsonNode
 * @m: a member name
 * @cmp: comparison operator
 * @num: expected value
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_cmpint(p, m, cmp, num)                          \
  G_STMT_START {                                                        \
    JsonObject *body = valent_packet_get_body (p);                      \
    if G_UNLIKELY (!json_object_has_member (body, m))                   \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "missing '"#m"' member");                    \
                                                                        \
    gint64 member = json_object_get_int_member (body, m);               \
    g_assert_cmpint (member, cmp, num);                                 \
  } G_STMT_END

/**
 * v_assert_packet_cmpstr:
 * @p: a #JsonNode
 * @m: a member name
 * @cmp: comparison operator
 * @str: expected value
 *
 * Check the body object of @p for the member @m with a value of @str.
 */
#define v_assert_packet_cmpstr(p, m, cmp, str)                          \
  G_STMT_START {                                                        \
    JsonObject *body = valent_packet_get_body (p);                      \
    if G_UNLIKELY (!json_object_has_member (body, m))                   \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "missing '"#m"' member");                    \
                                                                        \
    const char *member = json_object_get_string_member (body, m);       \
    g_assert_cmpstr (member, cmp, str);                                 \
  } G_STMT_END

G_END_DECLS

