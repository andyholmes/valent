// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui-utils"

#include "config.h"

#include <pango/pango.h>
#include <libvalent-core.h>

#include "valent-ui-utils.h"


// Andy Holmes
#define EMAIL_PATTERN "\\b(?:.+@.+\\.[a-z]{2,4}\\b)"

// https://gist.github.com/gruber/8891611 (changed tld list to `[a-z]{2,4}`)
#define URL_PATTERN "\\b((?:https?:(?:/{1,3}|[a-z0-9%])|[a-z0-9.\\-]+[.](?:[a-z]{2,4})/)(?:[^\\s()<>{}\\[\\]]+|\\([^\\s()]*?\\([^\\s()]+\\)[^\\s()]*?\\)|\\([^\\s]+?\\))+(?:\\([^\\s()]*?\\([^\\s()]+\\)[^\\s()]*?\\)|\\([^\\s]+?\\)|[^\\s`!()\\[\\]{};:'\".,<>?«»“”‘’])|(?:(?<!@)[a-z0-9]+(?:[.\\-][a-z0-9]+)*[.](?:[a-z]{2,4})\\b/?(?!@)))"

#define URI_FLAGS   (G_REGEX_CASELESS | G_REGEX_MULTILINE | G_REGEX_NO_AUTO_CAPTURE | G_REGEX_OPTIMIZE)

static GRegex *email_regex = NULL;
static GRegex *uri_regex = NULL;

static gboolean
valent_ui_replace_eval_uri (const GMatchInfo *info,
                            GString          *result,
                            gpointer          user_data)
{
  g_autofree char *text = NULL;
  g_autofree char *escaped = NULL;

  text = g_match_info_fetch (info, 0);

  if (g_uri_is_valid (text, G_URI_FLAGS_NONE, NULL))
    {
      escaped = g_markup_printf_escaped ("<a href=\"%s\">%s</a>",
                                         text, text);
    }
  else if (g_regex_match (email_regex, text, 0, NULL))
    {
      escaped = g_markup_printf_escaped ("<a href=\"mailto:%s\">%s</a>",
                                         text, text);
    }
  else
    {
      escaped = g_markup_printf_escaped ("<a href=\"https://%s\">%s</a>",
                                         text, text);
    }

  g_string_append (result, escaped);

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
 * Returns: (transfer full) (nullable): a string of Pango markup
 *
 * Since: 1.0
 */
char *
valent_string_to_markup (const char *text)
{
  g_autofree char *result = NULL;
  int text_len = 0;
  g_autoptr (GError) error = NULL;

  if G_UNLIKELY (text == NULL)
    return NULL;

  if G_UNLIKELY (uri_regex == NULL)
    {
      email_regex = g_regex_new (EMAIL_PATTERN, URI_FLAGS, 0, NULL);
      uri_regex = g_regex_new (URL_PATTERN"|"EMAIL_PATTERN, URI_FLAGS, 0, NULL);
    }

  text_len = strlen (text);
  result = g_regex_replace_eval (uri_regex,
                                 text,
                                 text_len,
                                 0,
                                 0,
                                 valent_ui_replace_eval_uri,
                                 NULL,
                                 &error);

  if (result == NULL)
    {
      g_warning ("%s: %s: %s", G_STRFUNC, error->message, text);
      return g_markup_escape_text (text, text_len);
    }

  if (!pango_parse_markup (result, -1, 0, NULL, NULL, NULL, &error) &&
      !g_error_matches (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT))
    {
      g_warning ("%s: %s: %s", G_STRFUNC, error->message, result);
      return g_markup_escape_text (text, text_len);
    }

  return g_steal_pointer (&result);
}

