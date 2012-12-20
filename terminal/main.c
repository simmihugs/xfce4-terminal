/*-
 * Copyright (c) 2004-2007 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <stdlib.h>

#include <terminal/terminal-app.h>
#include <terminal/terminal-private.h>

#include <terminal/terminal-gdbus.h>



static void
usage (void)
{
  gchar *name = g_get_prgname ();

  /* set locale for the translations below */
  gtk_set_locale ();

  g_print ("%s\n"
           "  %s [%s...]\n\n",
           _("Usage:"), name, _("OPTION"));

  g_print ("%s:\n"
           "  -h, --help; -V, --version; --disable-server;\n"
           "  --default-display=%s; --default-working-directory=%s\n\n",
           _("General Options"),
           /* parameter of --default-display */
           _("display"),
           /* parameter of --default-working-directory */
           _("directory"));

  g_print ("%s:\n"
           "  --tab; --window\n\n",
           _("Window or Tab Separators"));

  g_print ("%s:\n"
           "  -x, --execute; -e, --command=%s; -T, --title=%s;\n"
           "  --working-directory=%s; -H, --hold\n\n",
           _("Tab Options"),
           /* parameter of --command */
           _("command"),
           /* parameter of --title */
           _("title"),
           /* parameter of --working-directory */
           _("directory"));

  g_print ("%s:\n"
           "  --display=%s; --geometry=%s; --role=%s;\n"
           "  --startup-id=%s; -I, --icon=%s; --fullscreen; --maximize;\n"
           "  --show-menubar, --hide-menubar; --show-borders, --hide-borders;\n"
           "  --show-toolbars, --hide-toolbars\n\n",
           _("Window Options"),
           /* parameter of --display */
           _("display"),
           /* parameter of --geometry */
           _("geometry"),
           /* parameter of --role */
           _("role"),
           /* parameter of --startup-id */
           _("string"),
           /* parameter of --icon */
           _("icon"));

  g_print (_("See the %s man page for full explanation of the options above."),
           name);

  g_print ("\n\n");

  g_free (name);
}



int
main (int argc, char **argv)
{
  gboolean         show_help = FALSE;
  gboolean         show_version = FALSE;
  gboolean         disable_server = FALSE;
  GdkModifierType  modifiers;
  TerminalApp     *app;
  const gchar     *startup_id;
  const gchar     *display;
  GError          *error = NULL;
  gchar          **nargv;
  gint             nargc;
  gint             n;
  gchar           *name;
  const gchar     *msg;

  /* install required signal handlers */
  signal (SIGPIPE, SIG_IGN);

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
  g_set_application_name (_("Xfce Terminal"));

#ifndef NDEBUG
  /* Do NOT remove this line for now, If something doesn't work,
   * fix your code instead!
   */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

  /* parse some options we need in main, not the windows attrs */
  terminal_options_parse (argc, argv, &show_help, &show_version, &disable_server);

  if (G_UNLIKELY (show_version))
    {
      /* set locale for the translations below */
      gtk_set_locale ();

      name = g_get_prgname ();
      g_print ("%s %s (Xfce %s)\n\n", name, PACKAGE_VERSION, xfce_version_string ());
      g_free (name);
      g_print ("%s\n", "Copyright (c) 2003-2012");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print ("%s\n", _("Written by Benedikt Meurer <benny@xfce.org>"));
      g_print ("%s\n\n", _("and Nick Schermer <nick@xfce.org>."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");
      return EXIT_SUCCESS;
    }
  else if (G_UNLIKELY (show_help))
    {
      usage ();
      return EXIT_SUCCESS;
    }

  /* create a copy of the standard arguments with our additional stuff */
  nargv = g_new (gchar*, argc + 5); nargc = 0;
  nargv[nargc++] = g_strdup (argv[0]);
  nargv[nargc++] = g_strdup ("--default-working-directory");
  nargv[nargc++] = g_get_current_dir ();

  /* append startup if given */
  startup_id = g_getenv ("DESKTOP_STARTUP_ID");
  if (G_LIKELY (startup_id != NULL))
    {
      nargv[nargc++] = g_strdup_printf ("--startup-id=%s", startup_id);
      g_unsetenv ("DESKTOP_STARTUP_ID");
    }

  /* append default display if given */
  display = g_getenv ("DISPLAY");
  if (G_LIKELY (display != NULL))
    nargv[nargc++] = g_strdup_printf ("--default-display=%s", display);

  /* append all given arguments */
  for (n = 1; n < argc; ++n)
    nargv[nargc++] = g_strdup (argv[n]);
  nargv[nargc] = NULL;

  /* for GDBus */
  g_type_init ();

  if (!disable_server)
    {
      /* try to connect to an existing Terminal service */
      if (terminal_gdbus_invoke_launch (nargc, nargv, &error))
        {
          return EXIT_SUCCESS;
        }
      else
        {
          if (g_error_matches (error, TERMINAL_ERROR, TERMINAL_ERROR_USER_MISMATCH)
              || g_error_matches (error, TERMINAL_ERROR, TERMINAL_ERROR_DISPLAY_MISMATCH))
            {
              /* don't try to establish another service here */
              disable_server = TRUE;

#ifndef NDEBUG
              g_debug ("%s mismatch when invoking remote terminal: %s",
                       error->code == TERMINAL_ERROR_USER_MISMATCH ? "User" : "Display",
                       error->message);
#endif
            }
          else if (g_error_matches (error, TERMINAL_ERROR, TERMINAL_ERROR_OPTIONS))
            {
              /* skip the GDBus prefix */
              msg = strchr (error->message, ' ');
              if (G_LIKELY (msg != NULL))
                msg++;
              else
                msg = error->message;

              /* options were not parsed succesfully, don't try that again below */
              g_printerr ("%s: %s\n", g_get_prgname (), msg);
              g_error_free (error);
              g_strfreev (nargv);
              return EXIT_FAILURE;
            }
#ifndef NDEBUG
          else if (error != NULL)
            {
              g_debug ("D-Bus reply error: %s (%s: %d)", error->message,
                       g_quark_to_string (error->domain), error->code);
            }
#endif

          g_clear_error (&error);
        }
    }

  /* initialize Gtk+ */
  gtk_init (&argc, &argv);

  /* Make GtkAccelGroup accept Mod5 (Windows Key) as modifier */
  modifiers = gtk_accelerator_get_default_mod_mask ();
  gtk_accelerator_set_default_mod_mask (modifiers | GDK_MOD4_MASK);

  /* set default window icon */
  gtk_window_set_default_icon_name ("utilities-terminal");

  app = g_object_new (TERMINAL_TYPE_APP, NULL);

  if (!disable_server)
    {
      if (!terminal_gdbus_register_service (app, &error))
        {
          g_printerr (_("Unable to register terminal service: %s\n"), error->message);
          g_clear_error (&error);
        }
    }

  if (!terminal_app_process (app, nargv, nargc, &error))
    {
      /* parsing one of the arguments failed */
      g_printerr ("%s: %s\n", g_get_prgname (), error->message);
      g_error_free (error);
      g_object_unref (G_OBJECT (app));
      g_strfreev (nargv);
      return EXIT_FAILURE;
    }

  /* free temporary arguments */
  g_strfreev (nargv);

  gtk_main ();

  g_object_unref (G_OBJECT (app));

  return EXIT_SUCCESS;
}
