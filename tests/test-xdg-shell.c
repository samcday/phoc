/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#include "testlib.h"

#include "xdg-shell-client-protocol.h"


typedef struct {
  gboolean auto_maximize;
  const char *screenshot_maximized;

} PhocTestXdgShellTestData;


static gboolean
test_client_xdg_shell_normal (PhocTestClientGlobals *globals, gpointer data)
{
  PhocTestXdgToplevelSurface *ls_green;

  ls_green = phoc_test_xdg_toplevel_new_with_buffer (globals, 0, 0, NULL, 0xFF00FF00);
  g_assert_nonnull (ls_green);
  phoc_assert_screenshot (globals, "test-xdg-shell-normal-1.png");

  phoc_test_xdg_toplevel_free (ls_green);
  phoc_assert_screenshot (globals, "empty.png");

  return TRUE;
}


static gboolean
test_client_xdg_shell_auto_maximized (PhocTestClientGlobals *globals, gpointer data)
{
  PhocTestXdgToplevelSurface *ls_green;

  ls_green = phoc_test_xdg_toplevel_new_with_buffer (globals, 0, 0, NULL, 0xFF00FF00);
  g_assert_nonnull (ls_green);
  phoc_assert_screenshot (globals, "test-xdg-shell-maximized-1.png");

  phoc_test_xdg_toplevel_free (ls_green);
  phoc_assert_screenshot (globals, "empty.png");

  return TRUE;
}


static gboolean
test_client_xdg_shell_toplevel_maximized (PhocTestClientGlobals *globals, gpointer data)
{
  PhocTestXdgToplevelSurface *xs;
  guint32 color = 0xFF00FF00;

  xs = phoc_test_xdg_toplevel_new_with_buffer (globals, 0, 0, "to-max", color);
  g_assert_nonnull (xs);

  /* Maximize */
  zwlr_foreign_toplevel_handle_v1_set_maximized (xs->foreign_toplevel->handle);
  wl_surface_commit (xs->wl_surface);
  wl_display_dispatch (globals->display);
  /* We got the maximized size, update the buffer */
  phoc_test_xdg_update_buffer (globals, xs, color);
  phoc_assert_screenshot (globals, "test-xdg-shell-maximized-1.png");

  /* Back to normal state */
  zwlr_foreign_toplevel_handle_v1_unset_maximized (xs->foreign_toplevel->handle);
  wl_display_dispatch (globals->display);
  phoc_test_xdg_update_buffer (globals, xs, color);
  phoc_assert_screenshot (globals, "test-xdg-shell-normal-1.png");

  phoc_test_xdg_toplevel_free (xs);
  phoc_assert_screenshot (globals, "empty.png");

  return TRUE;
}


static gboolean
test_client_xdg_shell_toplevel_maximized_scale (PhocTestClientGlobals *globals, gpointer data)
{
  PhocTestXdgToplevelSurface *xs;
  guint32 color = 0xFF00FF00;

  xs = phoc_test_xdg_toplevel_new_with_buffer (globals, 0, 0, "to-max", color);
  g_assert_nonnull (xs);

  /* Maximize */
  zwlr_foreign_toplevel_handle_v1_set_maximized (xs->foreign_toplevel->handle);
  wl_surface_commit (xs->wl_surface);
  wl_display_dispatch (globals->display);
  /* We got the maximized size, update the buffer */
  phoc_test_xdg_update_buffer (globals, xs, color);
  phoc_assert_screenshot (globals, "test-xdg-shell-maximized-2.5.png");

  /* Back to normal state */
  zwlr_foreign_toplevel_handle_v1_unset_maximized (xs->foreign_toplevel->handle);
  wl_display_dispatch (globals->display);
  phoc_test_xdg_update_buffer (globals, xs, color);
  phoc_assert_screenshot (globals, "test-xdg-shell-normal-2.5.png");

  phoc_test_xdg_toplevel_free (xs);
  phoc_assert_screenshot (globals, "empty-2.5.png");

  return TRUE;
}


static gboolean
test_client_xdg_shell_server_prepare (PhocServer *server, gpointer data)
{
  PhocDesktop *desktop = phoc_server_get_desktop (server);
  gboolean maximize = GPOINTER_TO_INT (data);

  g_assert_nonnull (desktop);
  phoc_desktop_set_auto_maximize (desktop, maximize);
  return TRUE;
}


static void
test_xdg_shell_normal (void)
{
  PhocTestClientIface iface = {
    .server_prepare = test_client_xdg_shell_server_prepare,
    .client_run     = test_client_xdg_shell_normal,
    .debug_flags    = PHOC_SERVER_DEBUG_FLAG_DISABLE_ANIMATIONS,
  };

  phoc_test_client_run (TEST_PHOC_CLIENT_TIMEOUT, &iface, GINT_TO_POINTER (FALSE));
}


static void
test_xdg_shell_auto_maximized (void)
{
  PhocTestClientIface iface = {
    .server_prepare = test_client_xdg_shell_server_prepare,
    .client_run     = test_client_xdg_shell_auto_maximized,
    .debug_flags    = PHOC_SERVER_DEBUG_FLAG_DISABLE_ANIMATIONS,
  };

  phoc_test_client_run (TEST_PHOC_CLIENT_TIMEOUT, &iface, GINT_TO_POINTER (TRUE));
}


static void
test_xdg_shell_toplevel_maximized (void)
{
  PhocTestClientIface iface = {
    .server_prepare = test_client_xdg_shell_server_prepare,
    .client_run     = test_client_xdg_shell_toplevel_maximized,
    .debug_flags    = PHOC_SERVER_DEBUG_FLAG_DISABLE_ANIMATIONS,
  };

  phoc_test_client_run (TEST_PHOC_CLIENT_TIMEOUT, &iface, GINT_TO_POINTER (FALSE));
}


static void
test_xdg_shell_toplevel_maximized_scale (void)
{
  PhocTestClientIface iface = {
    .server_prepare = test_client_xdg_shell_server_prepare,
    .client_run     = test_client_xdg_shell_toplevel_maximized_scale,
    .debug_flags    = PHOC_SERVER_DEBUG_FLAG_DISABLE_ANIMATIONS,
    .output_config  = (PhocTestOutputConfig){
      .width = 360,
      .height = 720,
      .scale = 2.5,
      .transform = WL_OUTPUT_TRANSFORM_270,
    },
  };

  phoc_test_client_run (TEST_PHOC_CLIENT_TIMEOUT, &iface, GINT_TO_POINTER (FALSE));
}


gint
main (gint argc, gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  PHOC_TEST_ADD ("/phoc/xdg-shell/simple/normal", test_xdg_shell_normal);
  PHOC_TEST_ADD ("/phoc/xdg-shell/auto-maximize/normal", test_xdg_shell_auto_maximized);
  PHOC_TEST_ADD ("/phoc/xdg-shell/toplevel/maximize/normal", test_xdg_shell_toplevel_maximized);
  PHOC_TEST_ADD ("/phoc/xdg-shell/toplevel/maximize/scale",
                 test_xdg_shell_toplevel_maximized_scale);

  return g_test_run ();
}
