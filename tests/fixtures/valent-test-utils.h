// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <valent.h>

G_BEGIN_DECLS

void             valent_test_init          (int              *argcp,
                                            char           ***argvp,
                                                              ...);
void             valent_test_ui_init       (int              *argcp,
                                            char           ***argvp,
                                                              ...);

gboolean         valent_test_mute_fuzzing  (const char       *log_domain,
                                            GLogLevelFlags    log_level,
                                            const char       *message,
                                            gpointer          user_data);

gpointer         valent_test_event_pop     (void);
void             valent_test_event_push    (gpointer          data);
void             valent_test_event_free    (GDestroyNotify    free_func);

gpointer         valent_test_await_adapter (gpointer          component);
void             valent_test_await_pending (void);
void             valent_test_wait          (unsigned int      duration);
JsonNode       * valent_test_load_json     (const char       *path);
GSettings      * valent_test_mock_settings (const char       *domain);
ValentChannel ** valent_test_channel_pair  (JsonNode         *identity,
                                            JsonNode         *peer_identity);
gboolean         valent_test_download      (ValentChannel    *channel,
                                            JsonNode         *packet,
                                            GError          **error);
gboolean         valent_test_upload        (ValentChannel    *channel,
                                            JsonNode         *packet,
                                            GFile            *file,
                                            GError          **error);

#define valent_test_event_cmpstr(str)       \
  G_STMT_START {                            \
    char *event = valent_test_event_pop (); \
    g_assert_cmpstr (event, ==, str);        \
    g_free (event);                         \
  } G_STMT_END


/**
 * VALENT_NO_ASAN:
 *
 * A function attribute that disables AddressSanitizer.
 */
#define VALENT_NO_ASAN
#if defined(__has_attribute)
  #if __has_attribute(no_sanitize)
    #undef VALENT_NO_ASAN
    #define VALENT_NO_ASAN __attribute__((no_sanitize("address")))
  #endif
#endif

/**
 * VALENT_HAVE_ASAN:
 *
 * A function attribute that disables AddressSanitizer.
 */
#define VALENT_HAVE_ASAN 0
#if defined(__SANITIZE_ADDRESS__)
 #undef VALENT_HAVE_ASAN
 #define VALENT_HAVE_ASAN 1
#elif defined(__has_feature)
 #if __has_feature(address_sanitizer)
  #undef VALENT_HAVE_ASAN
  #define VALENT_HAVE_ASAN 1
 #endif
#endif

/**
 * VALENT_NO_TSAN:
 *
 * A function attribute that disables ThreadSanitizer.
 */
#define VALENT_NO_TSAN
#if defined(__has_attribute)
  #if __has_attribute(no_sanitize)
    #undef VALENT_NO_TSAN
    #define VALENT_NO_TSAN __attribute__((no_sanitize("thread")))
  #endif
#endif

/**
 * VALENT_HAVE_TSAN:
 *
 * Whether ThreadSanitizer is in use.
 */
#define VALENT_HAVE_TSAN 0
#if defined(__SANITIZE_THREAD__)
 #undef VALENT_HAVE_TSAN
 #define VALENT_HAVE_TSAN 1
#elif defined(__has_feature)
 #if __has_feature(thread_sanitizer)
  #undef VALENT_HAVE_TSAN
  #define VALENT_HAVE_TSAN 1
 #endif
#endif

/**
 * VALENT_NO_UBSAN:
 *
 * A function attribute that disables UndefinedBehaviourSanitizer.
 *
 * This macro only works on Clang.
 */
#define VALENT_NO_UBSAN
#if defined(__has_feature)
 #if __has_feature(undefined_sanitizer)
  #undef VALENT_NO_UBSAN
  #define VALENT_NO_UBSAN __attribute__((no_sanitize("undefined")))
 #endif
#endif

/**
 * VALENT_HAVE_UBSAN:
 *
 * Whether UndefinedBehaviourSanitizer is in use.
 *
 * This macro only works on Clang.
 */
#define VALENT_HAVE_UBSAN 0
#if defined(__has_feature)
 #if __has_feature(undefined_sanitizer)
  #undef VALENT_HAVE_UBSAN
  #define VALENT_HAVE_UBSAN 1
 #endif
#endif

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
 * @p: a #JsonNode
 * @m: a member name
 *
 * Assert the body object of @p has the member @m.
 */
#define v_assert_packet_field(p, m)                                            \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_LIKELY (json_object_has_member (__body, m)) ; else                    \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
  } G_STMT_END

/**
 * v_assert_packet_no_field:
 * @p: a #JsonNode
 * @m: a member name
 *
 * Assert the body object of @p does not have the member @m.
 */
#define v_assert_packet_no_field(p, m)                                         \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_LIKELY (!json_object_has_member (__body, m)) ; else                   \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should not have " #m " member");       \
  } G_STMT_END

/**
 * v_assert_packet_true:
 * @p: a #JsonNode
 * @m: a member name
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_true(p, m)                                             \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_LIKELY (json_object_has_member (__body, m)) ; else                    \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
    if G_LIKELY (json_object_get_boolean_member (__body, m)) ; else            \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           #m " should be TRUE");                              \
  } G_STMT_END

/**
 * v_assert_packet_false:
 * @p: a #JsonNode
 * @m: a member name
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_false(p, m)                                            \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_LIKELY (json_object_has_member (__body, m)) ; else                    \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
    if G_LIKELY (!json_object_get_boolean_member (__body, m)) ; else           \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           #m " should be FALSE");                             \
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
#define v_assert_packet_cmpfloat(p, m, cmp, num)                               \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_LIKELY (json_object_has_member (__body, m)) ; else                    \
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
 * @p: a #JsonNode
 * @m: a member name
 * @cmp: comparison operator
 * @num: expected value
 *
 * Check the body object of @p for the member @m.
 */
#define v_assert_packet_cmpint(p, m, cmp, num)                                 \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_LIKELY (json_object_has_member (__body, m)) ; else                    \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
    gint64 __n1 = json_object_get_double_member (__body, m);                   \
    gint64 __n2 = (gint64)(num);                                               \
    if (__n1 cmp __n2) ; else                                                  \
      g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
        #m " " #cmp " " #num, (gint64)__n1, #cmp, (gint64)__n2, 'i');          \
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
#define v_assert_packet_cmpstr(p, m, cmp, str)                                 \
  G_STMT_START {                                                               \
    JsonObject *__body = valent_packet_get_body (p);                           \
    if G_LIKELY (json_object_has_member (__body, m)) ; else                    \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,        \
                           "packet body should have " #m " member");           \
    const char *__s1 = json_object_get_string_member (__body, m);              \
    const char *__s2 = (str);                                                  \
    if (g_strcmp0 (__s1, __s2) cmp 0) ; else                                   \
      g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
        #m " " #cmp " " #str, __s1, #cmp, __s2);                               \
  } G_STMT_END

G_END_DECLS

