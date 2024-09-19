// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui-utils"

#include "config.h"

#include <gio/gio.h>

#include "valent-ui-utils-private.h"


// https://html.spec.whatwg.org/multipage/input.html#valid-e-mail-address
#define EMAIL_PATTERN "[a-zA-Z0-9.!#$%&'*+\\/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*"
// https://mathiasbynens.be/demo/url-regex, @stephenhay, relaxed scheme
#define URI_PATTERN   "\\b([a-zA-Z0-9-]+:[\\/]{1,3}|www[.])[^\\s>]*"
#define COMPILE_FLAGS (G_REGEX_CASELESS | G_REGEX_MULTILINE | G_REGEX_NO_AUTO_CAPTURE | G_REGEX_OPTIMIZE)

static GRegex *email_regex = NULL;
static GRegex *uri_regex = NULL;

static gboolean
valent_ui_replace_eval_uri (const GMatchInfo *info,
                            GString          *result,
                            gpointer          user_data)
{
  g_autofree char *uri = NULL;

  uri = g_match_info_fetch (info, 0);

  if (g_uri_is_valid (uri, G_URI_FLAGS_NONE, NULL))
    g_string_append_printf (result, "<a href=\"%s\">%s</a>", uri, uri);
  else if (g_regex_match (email_regex, uri, 0, NULL))
    g_string_append_printf (result, "<a href=\"mailto:%s\">%s</a>", uri, uri);
  else
    g_string_append_printf (result, "<a href=\"https://%s\">%s</a>", uri, uri);

  return FALSE;
}

/**
 * valent_string_to_markup:
 * @text: (nullable): input text
 *
 * Add markup to text for recognized elements.
 *
 * This function currently scans for URLs and e-mail addresses, then amends each
 * element with anchor tags (`<a>`).
 *
 * If @text is %NULL, this function will return %NULL.
 *
 * Returns: (transfer full) (nullable): a string of markup
 *
 * Since: 1.0
 */
char *
valent_string_to_markup (const char *text)
{
  g_autofree char *escaped = NULL;
  g_autofree char *markup = NULL;
  g_autoptr (GError) error = NULL;

  if G_UNLIKELY (text == NULL)
    return NULL;

  if G_UNLIKELY (uri_regex == NULL)
    {
      email_regex = g_regex_new (EMAIL_PATTERN, COMPILE_FLAGS, 0, NULL);
      uri_regex = g_regex_new (URI_PATTERN"|"EMAIL_PATTERN, COMPILE_FLAGS, 0, NULL);
    }

  escaped = g_markup_escape_text (text, -1);
  markup = g_regex_replace_eval (uri_regex,
                                 escaped,
                                 strlen (escaped),
                                 0,
                                 0,
                                 valent_ui_replace_eval_uri,
                                 NULL,
                                 &error);

  return g_steal_pointer (&markup);
}
