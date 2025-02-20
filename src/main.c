/* Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "phoc"

#include "phoc-config.h"

#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib/gi18n.h>

#include <locale.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/config.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include "settings.h"
#include "server.h"


static GLogLevelFlags wlroots_info_log_level = G_LOG_LEVEL_INFO;


G_NORETURN static void
print_version (void)
{
  printf ("Phoc %s - Phone compositor\n", PHOC_VERSION);
  exit (0);
}

static void
setup_signals (void)
{
  sigset_t mask;

  /* wlroots uses this to talk to xwayland, block it before
     we spawn other threads */
  sigemptyset (&mask);
  sigaddset (&mask, SIGUSR1);
  sigprocmask (SIG_BLOCK, &mask, NULL);
}

static void
log_glib (enum wlr_log_importance verbosity, const char *fmt, va_list args)
{
  int level;

  switch (verbosity) {
  case WLR_ERROR:
    level = G_LOG_LEVEL_CRITICAL;
    break;
  case WLR_INFO:
    level = wlroots_info_log_level;
    break;
  case WLR_DEBUG:
    level = G_LOG_LEVEL_DEBUG;
    break;
  default:
    g_assert_not_reached ();
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  g_logv ("phoc-wlroots", level, fmt, args);
#pragma GCC diagnostic pop
}


static GDebugKey debug_keys[] =
{
  { .key = "auto-maximize",
    .value = PHOC_SERVER_DEBUG_FLAG_AUTO_MAXIMIZE,},
  { .key = "damage-tracking",
    .value = PHOC_SERVER_DEBUG_FLAG_DAMAGE_TRACKING,},
  { .key = "no-quit",
    .value = PHOC_SERVER_DEBUG_FLAG_NO_QUIT,},
  { .key = "touch-points",
    .value = PHOC_SERVER_DEBUG_FLAG_TOUCH_POINTS,},
  { .key = "layer-shell",
    .value = PHOC_SERVER_DEBUG_FLAG_LAYER_SHELL,},
  { .key = "cutouts",
    .value = PHOC_SERVER_DEBUG_FLAG_CUTOUTS,},
  { .key = "disable-animations",
    .value = PHOC_SERVER_DEBUG_FLAG_DISABLE_ANIMATIONS,},
  { .key = "force-shell-reveal",
    .value = PHOC_SERVER_DEBUG_FLAG_FORCE_SHELL_REVEAL,},
};


static PhocServerDebugFlags
parse_debug_env (void)
{
  const char *debugenv;
  PhocServerDebugFlags flags = PHOC_SERVER_DEBUG_FLAG_NONE;

  debugenv = g_getenv ("PHOC_DEBUG");
  if (!debugenv)
    return flags;

  return g_parse_debug_string (debugenv,
                               debug_keys,
                               G_N_ELEMENTS (debug_keys));
}


int
main (int argc, char **argv)
{
  g_autoptr (GOptionContext) opt_context = NULL;
  g_autoptr (GError) err = NULL;
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (PhocServer) server = NULL;
  g_autofree gchar *config_path = NULL;
  g_autofree gchar *exec = NULL;
  PhocServerFlags flags = PHOC_SERVER_FLAG_NONE;
  PhocServerDebugFlags debug_flags = PHOC_SERVER_DEBUG_FLAG_NONE;
  gboolean version = FALSE, shell_mode = FALSE, xwayland = FALSE, verbose = FALSE;
  PhocConfig *config;

  setup_signals ();

  const GOptionEntry options [] = {
    {"config", 'C', 0, G_OPTION_ARG_STRING, &config_path,
     "Path to the configuration file. (default: phoc.ini).", NULL},
    {"exec", 'E', 0, G_OPTION_ARG_STRING, &exec,
     "Command (session) that will be ran at startup", NULL},
    {"shell", 'S', 0, G_OPTION_ARG_NONE, &shell_mode,
     "Whether to expect a shell to attach", NULL},
    {"xwayland", 'X', 0, G_OPTION_ARG_NONE, &xwayland,
     "Whether to start XWayland", NULL},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
     "Whether to provide more verbose output", NULL},
    {"version", 0, 0, G_OPTION_ARG_NONE, &version,
     "Show version information", NULL},
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
  };

  opt_context = g_option_context_new ("- A phone compositor");
  g_option_context_add_main_entries (opt_context, options, NULL);
  if (!g_option_context_parse (opt_context, &argc, &argv, &err)) {
    g_warning ("%s", err->message);
    g_clear_error (&err);
    return 1;
  }

  if (version)
    print_version ();

  if (verbose)
    wlroots_info_log_level = G_LOG_LEVEL_MESSAGE;

  setlocale (LC_MESSAGES, "");
  textdomain (GETTEXT_PACKAGE);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);

  debug_flags = parse_debug_env ();
  wlr_log_init (WLR_DEBUG, log_glib);
  server = phoc_server_get_default ();
  if (server == NULL) {
    /* phoc_server_get_default already printed an error */
    return 1;
  }
  phoc_server_set_debug_flags (server, debug_flags);

  if (shell_mode)
    flags |= PHOC_SERVER_FLAG_SHELL_MODE;

  config = phoc_config_new_from_file (config_path);
  if (config == NULL)
    return EXIT_FAILURE;
  if (xwayland)
    config->xwayland = TRUE;

  loop = g_main_loop_new (NULL, FALSE);
  if (!phoc_server_setup (server, config, exec, loop, flags))
    return EXIT_FAILURE;

  g_main_loop_run (loop);

  return phoc_server_get_session_exit_status (server);
}
