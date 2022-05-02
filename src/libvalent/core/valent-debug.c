// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Christian Hergert <chergert@redhat.com>
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include "valent-debug.h"

#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef VALENT_ENABLE_PROFILING
# include <sched.h>
# include <signal.h>
# include <sysprof-capture.h>
#endif


#ifdef VALENT_ENABLE_PROFILING
G_LOCK_DEFINE_STATIC (sysprof_mutex);

static SysprofCaptureWriter *sysprof = NULL;

static inline int
current_cpu (void)
{
#ifdef HAVE_SCHED_GETCPU
  return sched_getcpu ();
#else
  return 0;
#endif
}

static void
valent_trace_log (const char     *domain,
                  GLogLevelFlags  level,
                  const char     *message)
{
  G_LOCK (sysprof_mutex);
  if G_LIKELY (sysprof)
    {
      sysprof_capture_writer_add_log (sysprof,
                                      SYSPROF_CAPTURE_CURRENT_TIME,
                                      current_cpu (),
                                      getpid (),
                                      level,
                                      domain,
                                      message);
    }
  G_UNLOCK (sysprof_mutex);
}

void
valent_trace_mark (const char *strfunc,
                   gint64      begin_time_usec,
                   gint64      end_time_usec)
{
  G_LOCK (sysprof_mutex);
  if G_LIKELY (sysprof)
    {
      /* In case our clock is not reliable */
      if (end_time_usec < begin_time_usec)
        end_time_usec = begin_time_usec;

      sysprof_capture_writer_add_mark (sysprof,
                                       begin_time_usec * 1000L,
                                       current_cpu (),
                                       getpid (),
                                       (end_time_usec - begin_time_usec) * 1000L,
                                       "tracing",
                                       "function",
                                       strfunc);
    }
  G_UNLOCK (sysprof_mutex);
}
#endif


G_LOCK_DEFINE_STATIC (log_mutex);

GIOChannel *log_channel = NULL;

static const char* ignored_domains[] =
{
  "GLib-Net",
  "GLib",
  "Gvc",
  NULL
};

static const char *
valent_log_level_str (GLogLevelFlags log_level)
{
  switch (((gulong)log_level & G_LOG_LEVEL_MASK))
    {
    case G_LOG_LEVEL_ERROR:      return "   \033[1;31mERROR\033[0m";
    case G_LOG_LEVEL_CRITICAL:   return "\033[1;35mCRITICAL\033[0m";
    case G_LOG_LEVEL_WARNING:    return " \033[1;33mWARNING\033[0m";
    case G_LOG_LEVEL_MESSAGE:    return " \033[1;34mMESSAGE\033[0m";
    case G_LOG_LEVEL_INFO:       return "    \033[1;32mINFO\033[0m";
    case G_LOG_LEVEL_DEBUG:      return "   \033[1;32mDEBUG\033[0m";
    case VALENT_LOG_LEVEL_TRACE: return "   \033[1;36mTRACE\033[0m";

    default:
      return " UNKNOWN";
    }
}

static void
valent_log_handler (const char     *domain,
                    GLogLevelFlags  level,
                    const char     *message,
                    gpointer        user_data)
{
  gint64 now;
  struct tm tt;
  time_t t;
  char ftime[32];
  char *buffer;

#ifdef VALENT_ENABLE_PROFILING
  if (level == VALENT_LOG_LEVEL_TRACE)
    valent_trace_log (domain, level, message);
#endif

  /* Ignore noisy log domains */
  if (domain && g_strv_contains (ignored_domains, domain))
    return;

  /* Prepare log message */
  now = g_get_real_time ();
  t = now / G_USEC_PER_SEC;

#ifdef HAVE_LOCALTIME_R
  localtime_r (&t, &tt);
#else
  tt = *localtime (&t);
#endif

  strftime (ftime, sizeof (ftime), "%H:%M:%S", &tt);
  buffer = g_strdup_printf ("%s.%04d %30s: %s: %s\n",
                            ftime,
                            (int)((now % G_USEC_PER_SEC) / 100L),
                            domain,
                            valent_log_level_str (level),
                            message);

  G_LOCK (log_mutex);
  g_io_channel_write_chars (log_channel, buffer, -1, NULL, NULL);
  g_io_channel_flush (log_channel, NULL);
  G_UNLOCK (log_mutex);

  g_free (buffer);
}

/**
 * valent_debug_init:
 *
 * Initializes logging for Valent.
 *
 * This should be called before the application starts, which is typically when
 * [method@Gio.Application.run] is invoked.
 *
 * If %VALENT_ENABLE_PROFILING is defined, trace markers and messages at level
 * %VALENT_LOG_LEVEL_TRACE will be passed to sysprof.
 *
 * Since: 1.0
 */
void
valent_debug_init (void)
{
  G_LOCK (log_mutex);
  if (log_channel == NULL)
    {
      log_channel = g_io_channel_unix_new (STDOUT_FILENO);
      g_log_set_default_handler (valent_log_handler, NULL);
    }
  G_UNLOCK (log_mutex);

#ifdef VALENT_ENABLE_PROFILING
  G_LOCK (sysprof_mutex);
  if (sysprof == NULL)
    {
      signal (SIGPIPE, SIG_IGN);
      sysprof_clock_init ();
      sysprof = sysprof_capture_writer_new_from_env (0);
    }
  G_UNLOCK (sysprof_mutex);
#endif
}

/**
 * valent_debug_clear:
 *
 * Shutdown logging for Valent.
 *
 * This should be called after the application stops, which is typically when
 * the call to [method@Gio.Application.run] returns.
 *
 * Since: 1.0
 */
void
valent_debug_clear (void)
{
  G_LOCK (log_mutex);
  if (log_channel)
    {
      g_clear_pointer (&log_channel, g_io_channel_unref);
      g_log_set_default_handler (valent_log_handler, NULL);
    }
  G_UNLOCK (log_mutex);

#ifdef VALENT_ENABLE_PROFILING
  G_LOCK (sysprof_mutex);
  if (sysprof)
    {
      sysprof_capture_writer_flush (sysprof);
      g_clear_pointer (&sysprof, sysprof_capture_writer_unref);
    }
  G_UNLOCK (sysprof_mutex);
#endif
}
