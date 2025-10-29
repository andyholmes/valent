// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <valent.h>

G_BEGIN_DECLS

void             valent_test_init            (int                   *argcp,
                                              char                ***argvp,
                                                                     ...);
void             valent_test_run_loop        (void);
gboolean         valent_test_quit_loop       (void);

gboolean         valent_test_mute_fuzzing    (const char            *log_domain,
                                              GLogLevelFlags         log_level,
                                              const char            *message,
                                              gpointer               user_data);

gpointer         valent_test_await_adapter   (gpointer               component);
void             valent_test_await_boolean   (gboolean              *done);
void             valent_test_await_pending   (void);
void             valent_test_await_pointer   (gpointer              *result);
void             valent_test_await_nullptr   (gpointer              *result);
void             valent_test_await_signal    (gpointer               object,
                                              const char            *signal_name);
void             valent_test_watch_signal    (gpointer               object,
                                              const char            *signal_name,
                                              gboolean              *watch);
void             valent_test_watch_clear     (gpointer               object,
                                              gboolean              *watch);
void             valent_test_await_timeout   (unsigned int           duration);
JsonNode       * valent_test_load_json       (const char            *path);
GSettings      * valent_test_mock_settings   (const char            *domain);
void             valent_test_channel_pair    (JsonNode              *identity,
                                              JsonNode              *peer_identity,
                                              ValentChannel        **channel_out,
                                              ValentChannel        **peer_channel_out);
void             valent_test_download        (ValentChannel         *channel,
                                              JsonNode              *packet,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data);
gboolean         valent_test_download_finish (ValentChannel         *channel,
                                              GAsyncResult          *result,
                                              GError               **error);
void             valent_test_upload          (ValentChannel         *channel,
                                              JsonNode              *packet,
                                              GFile                 *file,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data);
gboolean         valent_test_upload_finish   (ValentChannel         *channel,
                                              GAsyncResult          *result,
                                              GError               **error);

#define valent_test_await_boolean(ptr)                     \
  G_STMT_START {                                           \
    G_STATIC_ASSERT (sizeof (ptr) == sizeof (gboolean *)); \
    valent_test_await_boolean ((gboolean *)ptr);           \
  } G_STMT_END

#define valent_test_await_pending()                \
  G_STMT_START {                                   \
    while (g_main_context_iteration (NULL, FALSE)) \
      continue;                                    \
  } G_STMT_END

#define valent_test_await_pointer(ptr)                     \
  G_STMT_START {                                           \
    G_STATIC_ASSERT (sizeof (ptr) == sizeof (gpointer *)); \
    valent_test_await_pointer ((gpointer *)ptr);           \
  } G_STMT_END

#define valent_test_await_nullptr(ptr)                     \
  G_STMT_START {                                           \
    G_STATIC_ASSERT (sizeof (ptr) == sizeof (gpointer *)); \
    valent_test_await_nullptr ((gpointer *)ptr);           \
  } G_STMT_END

/*
 * VALENT_TEST_CHECK: (skip)
 * @message: format string
 * @...: parameters to insert into the format string
 *
 * Print a message for the current test.
 */
#define VALENT_TEST_CHECK(message, ...)       \
        (g_test_message ("\n[%s]: "message,  \
                         g_test_get_path (), \
                         ##__VA_ARGS__))     \

/**
 * v_await_finalize_object:
 * @object: (type GObject.Object): a `GObject`
 *
 * Assert that @object is non-%NULL, then iterate the main context until its
 * reference count reaches `1`. Then release one reference to it with
 * g_object_unref() and assert that it has been finalized.
 *
 * This macro is similar to g_assert_finalize_object(), but with the advantage
 * supporting objects that need operations to resolve (ie. GTasks) before being
 * finalized.
 */
#define v_await_finalize_object(object)                                 \
  G_STMT_START {                                                        \
    gpointer weak_pointer = object;                                     \
                                                                        \
    if G_UNLIKELY (!G_IS_OBJECT (weak_pointer))                         \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "'" #object "' should be a GObject");        \
                                                                        \
    while (g_atomic_int_get (&((GObject *)object)->ref_count) > 1)      \
      g_main_context_iteration (NULL, FALSE);                           \
                                                                        \
    g_object_add_weak_pointer ((GObject *)object, &weak_pointer);       \
    g_object_unref (weak_pointer);                                      \
                                                                        \
    if G_UNLIKELY ((weak_pointer) != NULL)                              \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                           "'" #object "' should be finalized");        \
  } G_STMT_END

/**
 * v_assert_packet_type:
 * @p: a `JsonNode`
 * @t: a KDE Connect packet type
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_type(p, t)                                             \
  G_STMT_START {                                                               \
    const char *__s1 = valent_packet_get_type (p);                             \
    const char *__s2 = (t);                                                    \
    if (g_strcmp0 (__s1, __s2) == 0) ; else                                    \
      g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
        "type == " #t, __s1, "==", __s2);                                      \
  } G_STMT_END

/**
 * v_assert_packet_field:
 * @p: a `JsonNode`
 * @m: a member name
 *
 * Assert the body object of @p has the member @m.
 */
#define v_assert_packet_field(p, m)                                            \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_UNLIKELY (!json_object_has_member (__body, m))                        \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
  } G_STMT_END

/**
 * v_assert_packet_no_field:
 * @p: a `JsonNode`
 * @m: a member name
 *
 * Assert the body object of @p does not have the member @m.
 */
#define v_assert_packet_no_field(p, m)                                         \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_UNLIKELY (json_object_has_member (__body, m))                         \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should not have " #m " member");       \
  } G_STMT_END

/**
 * v_assert_packet_true:
 * @p: a `JsonNode`
 * @m: a member name
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_true(p, m)                                             \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_UNLIKELY (!json_object_has_member (__body, m))                        \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
    if G_UNLIKELY (!json_object_get_boolean_member (__body, m))                \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           #m " should be TRUE");                              \
  } G_STMT_END

/**
 * v_assert_packet_false:
 * @p: a `JsonNode`
 * @m: a member name
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_false(p, m)                                            \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_UNLIKELY (!json_object_has_member (__body, m))                        \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
    if G_UNLIKELY (json_object_get_boolean_member (__body, m))                 \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           #m " should be FALSE");                             \
  } G_STMT_END

/**
 * v_assert_packet_cmpfloat:
 * @p: a `JsonNode`
 * @m: a member name
 * @cmp: comparison operator
 * @num: expected value
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_cmpfloat(p, m, cmp, num)                               \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_UNLIKELY (!json_object_has_member (__body, m))                        \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
    double __n1 = json_object_get_double_member (__body, m);                   \
    double __n2 = (double)(num);                                               \
    if (__n1 cmp __n2) ; else                                                  \
      g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
        #m " " #cmp " " #num, (long double)__n1, #cmp, (double)__n2, 'f');     \
  } G_STMT_END

/**
 * v_assert_packet_cmpint:
 * @p: a `JsonNode`
 * @m: a member name
 * @cmp: comparison operator
 * @num: expected value
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_cmpint(p, m, cmp, num)                                 \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_UNLIKELY (!json_object_has_member (__body, m))                        \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
    int64_t __n1 = json_object_get_int_member (__body, m);                  \
    int64_t __n2 = (int64_t)(num);                                             \
    if (__n1 cmp __n2) ; else                                                  \
      g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
        #m " " #cmp " " #num, (int64_t)__n1, #cmp, (int64_t)__n2, 'i');        \
  } G_STMT_END

/**
 * v_assert_packet_cmpstr:
 * @p: a `JsonNode`
 * @m: a member name
 * @cmp: comparison operator
 * @str: expected value
 *
 * Check the body object of @p for the member @m with a value of @str.
 */
#define v_assert_packet_cmpstr(p, m, cmp, str)                                 \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_UNLIKELY (!json_object_has_member (__body, m))                        \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
    const char *__s1 = json_object_get_string_member (__body, m);              \
    const char *__s2 = (str);                                                  \
    if (g_strcmp0 (__s1, __s2) cmp 0) ; else                                   \
      g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
        #m " " #cmp " " #str, __s1, #cmp, __s2);                               \
  } G_STMT_END

G_END_DECLS

