/***********************************************************************/
/* gtkterm.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Main program file                                              */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 2.0 : Ported to GTK4                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : added call to add_shortcuts()                       */
/*      - 0.98 : all GUI functions moved to widgets.c                  */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "config.h"
#include "gtkterm.h"
#include "terminal.h"
#include "buffer.h"
#include "cmdline.h"
#include "interface.h"
#include "serial.h"

unsigned int gtkterm_signals[LAST_GTKTERM_SIGNAL];

//! @brief The main GtkTermWindow class.
//! MainWindow specific variables here.
struct _GtkTermWindow {
  GtkApplicationWindow parent_instance;

  GtkWidget *message;                   //! Message for the infobar
  GtkWidget *infobar;                   //! Infobar
  GtkWidget *statusbar;                 //! Statusbar
  GtkWidget *menubutton;                //! Toolbar
  GMenuModel *toolmenu;                 //! Menu
  GtkScrolledWindow *scrolled_window;   //! Make the terminal window scrolled
  GtkTermTerminal *terminal_window;     //! The terminal window
  GActionGroup *action_group;           //! Window action group

  int width;
  int height;
  bool maximized;
  bool fullscreen;
} ;

G_DEFINE_TYPE (GtkTerm, gtkterm, GTK_TYPE_APPLICATION)

G_DEFINE_TYPE (GtkTermWindow, gtkterm_window, GTK_TYPE_APPLICATION_WINDOW)

static void gtkterm_window_update (GtkTermWindow *, gpointer);

static void update_statusbar (GtkTermWindow *);
void set_window_title (GtkTermWindow *);

static void create_window (GApplication *app) {

  GtkTermWindow *window = (GtkTermWindow *)g_object_new (gtkterm_window_get_type (),
                                                          "application", 
                                                          GTKTERM_APP(app),
                                                          "show-menubar", 
                                                          TRUE,
                                                          NULL);
  
  //! Create a new terminal window and send section and keyfile as parameter
  //! GTKTERM_TERMINAL then can load the right section.
  window->terminal_window = gtkterm_terminal_new (GTKTERM_APP(app)->section, GTKTERM_APP(app), window);

  gtk_scrolled_window_set_child(window->scrolled_window, GTK_WIDGET(window->terminal_window));

  set_window_title (window);
  
  gtk_window_present (GTK_WINDOW (window));
}

static void show_action_infobar (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       data) {

  GtkTermWindow *window = data;
  char *text;
  const char *name;
  const char *value;

  name = g_action_get_name (G_ACTION (action));
  value = g_variant_get_string (parameter, NULL);

  text = g_strdup_printf ("You activated radio action: \"%s\".\n"
                          "Current value: %s", name, value);
  gtk_label_set_text (GTK_LABEL (window->message), text);
  gtk_widget_show (window->infobar);
  g_free (text);
}

static void open_response_cb (GtkNativeDialog *dialog,
                  int              response_id,
                  gpointer         user_data) {

  GtkFileChooserNative *native = user_data;
  GtkWidget *message_dialog;
  GFile *file;
  char *contents;
  GError *error = NULL;

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));

      if (g_file_load_contents (file, NULL, &contents, NULL, NULL, &error))
        {
 //         create_window (app);
 //         g_free (contents);
        }
      else
        {
          message_dialog = gtk_message_dialog_new (NULL,
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "Error loading file: \"%s\"",
                                                   error->message);
          g_signal_connect (message_dialog, "response", G_CALLBACK (gtk_window_destroy), NULL);
          gtk_widget_show (message_dialog);
          g_error_free (error);
        }
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
  g_object_unref (native);
}

static void on_gtkterm_send_raw (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data) {

  GApplication *app = user_data;
  GtkFileChooserNative *native;

  native = gtk_file_chooser_native_new ("Open File",
                                        NULL,
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Open",
                                        "_Cancel");

  g_object_set_data_full (G_OBJECT (native), "gtkterm", g_object_ref (app), g_object_unref);
  g_signal_connect (native, "response", G_CALLBACK (open_response_cb), native);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
}

static void on_gtkterm_toggle_state (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data) {
  GVariant *state;

//  show_action_dialog (action);

  state = g_action_get_state (G_ACTION (action));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
  g_variant_unref (state);
}

static void on_gtkterm_toggle_radio (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data) {

  show_action_infobar (action, parameter, user_data);

  g_action_change_state (G_ACTION (action), parameter);
}

static void on_gtkterm_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data) {

  GtkWidget *window = user_data;
  char *os_name;
  char *os_version;
  GString *s;

  const char *authors[] = {
    "Julien Schimtt", 
    "Zach Davis", 
    "Florian Euchner", 
    "Stephan Enderlein",
    "Kevin Picot",
    "Jens Georg",
    NULL};

  const char *comments =  _("GTKTerm is a simple GTK+ terminal used to communicate with the serial port.");

  s = g_string_new ("");

  os_name = g_get_os_info (G_OS_INFO_KEY_NAME);
  os_version = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);

  if (os_name && os_version)
    g_string_append_printf (s, "OS\t%s %s\n\n", os_name, os_version);
  
  g_string_append (s, "System libraries\n");
  g_string_append_printf (s, "\tGLib\t%d.%d.%d\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version);
  
  g_string_append_printf (s, "\tPango\t%s\n", pango_version_string ());
  g_string_append_printf (s, "\tGTK \t%d.%d.%d\n", gtk_get_major_version (),
                          gtk_get_minor_version (), gtk_get_micro_version ());

  GdkTexture *logo = gdk_texture_new_from_resource ("/com/github/jeija/gtkterm/gtkterm_48x48.png");

  gtk_show_about_dialog (GTK_WINDOW (window),
                         "program-name", "GTKTerm",
                         "version", PACKAGE_VERSION,
	                       "copyright", "Copyright © Julien Schimtt",
	                       "license-type", GTK_LICENSE_LGPL_3_0, 
	                       "website", "https://github.com/Jeija/gtkterm",
	                       "website-label", "https://github.com/Jeija/gtkterm",
                         "comments", "Program to demonstrate GTK functions.",
                         "authors", authors,
                         "logo-icon-name", "com.github.jeija.gtkterm",
                         "title", "GTKTerm",
                        "comments", comments,
	                       "logo", logo,
                         "system-information", s->str,
                         NULL);
}

static void on_gtkterm_quit (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data) {
 
  GtkTerm *app = GTKTERM_APP(user_data);
  GtkWidget *win;
  GList *list, *next;

  list = gtk_application_get_windows (GTK_APPLICATION(app));
  while (list)
    {
      win = list->data;
      next = list->next;

      gtk_window_destroy (GTK_WINDOW (win));

      list = next;
    }

  //! Clean up memory
  g_free (app->section);

  //! TODO: Should be part of the Gtkterm application struct
  g_option_group_unref (app->g_term_group);
  g_option_group_unref (app->g_port_group);
  g_option_group_unref (app->g_config_group); 
}

static void update_statusbar (GtkTermWindow *window) {

  char *msg;

  //! Clear any previous message, underflow is allowed
  gtk_statusbar_pop (GTK_STATUSBAR (window->statusbar), 0);

  msg = g_strdup_printf ("%s", "Test" /*get_port_string()*/);

  gtk_statusbar_push (GTK_STATUSBAR (window->statusbar), 0, msg);

  g_free (msg);
}

void set_window_title (GtkTermWindow *window) {

  char *msg;

  msg = g_strdup_printf ("GTKTerm");

  gtk_window_set_title (GTK_WINDOW(window), msg);

  g_free (msg);
}

//! Update the window title and statusbar with the new configuration
static void gtkterm_window_update (GtkTermWindow *window, gpointer user_data) {

  set_window_title (window);
  update_statusbar (window);
}

static void on_gtkterm_toggle_dark (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data) {

  GtkSettings *settings = gtk_settings_get_default ();

  g_object_set (G_OBJECT (settings),
                "gtk-application-prefer-dark-theme",
                g_variant_get_boolean (state),
                NULL);

  g_simple_action_set_state (action, state);
}

static void on_gtkterm_toggle_radio_state (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data) {

  g_simple_action_set_state (action, state);
}

static GActionEntry gtkterm_entries[] = {
  { "quit", on_gtkterm_quit, NULL, NULL, NULL },
  // { "clear_screen", on_gtkterm_clear_screen, NULL, NULL, NULL },
  // { "clear_scrollback", on_gtkterm_clear_scrollback, NULL, NULL, NULL },
  // { "send_raw_file", on_gtkterm_send_raw, NULL, NULL, NULL },  
  // { "save_raw_file", on_gtkterm_save_raw, NULL, NULL, NULL },
  // { "copy", on_gtkterm_copy, NULL, NULL, NULL },
  // { "paste", on_gtkterm_paste, NULL, NULL, NULL },  
  // { "select_all", on_gtkterm_select_all, NULL, NULL, NULL },
  // { "log_to_file", on_gtkterm_log_to_file, NULL, NULL, NULL },
  // { "log_resume", on_gtkterm_log_resume, NULL, NULL, NULL },
  // { "log_stop", on_gtkterm_log_stop, NULL, NULL, NULL },
  // { "log_clear", on_gtkterm_log_clear, NULL, NULL, NULL },
  // { "config_port", on_gtkterm_config_port, NULL, NULL, NULL },
  // { "conig_main_window", on_gtkterm_config_main_window, NULL, NULL, NULL },
  // { "local_echo", on_gtkterm_local_echo, NULL, NULL, NULL },
  // { "crlf_auto", on_gtkterm_crlf_auto, NULL, NULL, NULL },
  // { "timestamp", on_gtkterm_timestamp, NULL, NULL, NULL },
  // { "macro", on_gtkterm_macros, NULL, NULL, NULL },
  // { "load_config", on_gtkterm_load_config, NULL, NULL, NULL },
  // { "save_config", on_gtkterm_save_config, NULL, NULL, NULL },
  // { "remove_config", on_gtkterm_remove_config, NULL, NULL, NULL },
  // { "send_break", on_gtkterm_send_break, NULL, NULL, NULL },
  // { "open_port", on_gtkterm_open_port, NULL, NULL, NULL },
  // { "close_port", on_gtkterm_close_port, NULL, NULL, NULL },         
  // { "toggle_DTR", on_gtkterm_toggle_DTR, NULL, NULL, NULL },
  // { "toggle_RTS", on_gtkterm_toggle_RTS, NULL, NULL, NULL },
  // { "show_ascii", on_gtkterm_show_ascii, NULL, NULL, NULL },
  // { "show_hex", on_gtkterm_show_hex, NULL, NULL, NULL },
  // { "view_hex", on_gtkterm_view_hex, NULL, NULL, NULL },
  // { "show_index", on_gtkterm_show_index, NULL, NULL, NULL },
  // { "send_hex_data,", on_gtkterm_send_hex_data, NULL, NULL, NULL },  
  { "dark", on_gtkterm_toggle_state, NULL, "false", on_gtkterm_toggle_dark}
};

static GActionEntry win_entries[] = {
  { "about", on_gtkterm_about, NULL, NULL, NULL }
};

static void clicked_cb (GtkWidget *widget, GtkTermWindow *window) {

  gtk_widget_hide (window->infobar);
}

static void startup (GApplication *app) {

  GtkBuilder *builder;

  G_APPLICATION_CLASS (gtkterm_parent_class)->startup (app);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/com/github/jeija/gtkterm/menu.ui", NULL);

  gtk_application_set_menubar (GTK_APPLICATION (app),
                               G_MENU_MODEL (gtk_builder_get_object (builder, "gtkterm_menubar")));

  g_object_unref (builder);
}

static void activate (GApplication *app) {

  create_window (app);
}

// Do the basic initialization of the application
static void gtkterm_init (GtkTerm *app) {
  GSettings *settings;

  settings = g_settings_new ("com.github.jeija.gtkterm");

  //! Set an action group for the app entries.
  app->action_group =  G_ACTION_GROUP (g_simple_action_group_new ()); 

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   gtkterm_entries, 
                                   G_N_ELEMENTS (gtkterm_entries),
                                   app);  

  //! load the config file and set the section to [default]
  app->config = GTKTERM_CONFIGURATION (g_object_new (GTKTERM_TYPE_CONFIGURATION, NULL));
  app->section = g_strdup (DEFAULT_SECTION);

  gtkterm_add_cmdline_options (app); 

  g_object_unref (settings);
}

static void gtkterm_class_init (GtkTermClass *class) {

  GApplicationClass *app_class = G_APPLICATION_CLASS (class); 

  gtkterm_signals[SIGNAL_GTKTERM_LOAD_CONFIG] = g_signal_new ("config_load",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                0,
                                                NULL);

  gtkterm_signals[SIGNAL_GTKTERM_SAVE_CONFIG] = g_signal_new ("config_save",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                0,
                                                NULL);

  gtkterm_signals[SIGNAL_GTKTERM_PRINT_SECTION] = g_signal_new ("config_print",
                                                 GTKTERM_TYPE_CONFIGURATION,
                                                 G_SIGNAL_RUN_FIRST,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 G_TYPE_NONE,
                                                 1,
                                                 G_TYPE_POINTER,
                                                 NULL);                                      

  gtkterm_signals[SIGNAL_GTKTERM_REMOVE_SECTION] = g_signal_new ("config_remove",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                0,
                                                NULL);

    gtkterm_signals[SIGNAL_GTKTERM_CONFIG_TERMINAL] = g_signal_new ("config_terminal",
                                                				GTKTERM_TYPE_CONFIGURATION,
                                                				G_SIGNAL_RUN_FIRST,
                                                				0,
                                               					NULL,
                                                				NULL,
                                                				NULL,
                                                				G_TYPE_POINTER,
                                               					1,
																                        G_TYPE_POINTER,
                                               					NULL);

    gtkterm_signals[SIGNAL_GTKTERM_CONFIG_SERIAL] = g_signal_new ("config_serial",
                                                				GTKTERM_TYPE_CONFIGURATION,
                                                				G_SIGNAL_RUN_FIRST,
                                                				0,
                                               					NULL,
                                                				NULL,
                                                				NULL,
                                                				G_TYPE_POINTER,
                                               					1,
																                        G_TYPE_POINTER,
                                               					NULL);

  app_class->startup = startup;
  app_class->activate = activate;
}

static void gtkterm_window_store_state (GtkTermWindow *win) {

  GSettings *settings;

  settings = g_settings_new ("com.github.jeija.gtkterm");
  g_settings_set (settings, "window-size", "(ii)", win->width, win->height);
  g_settings_set_boolean (settings, "maximized", win->maximized);
  g_settings_set_boolean (settings, "fullscreen", win->fullscreen);
  g_object_unref (settings);
}

static void gtkterm_window_load_state (GtkTermWindow *win) {
  GSettings *settings;

  settings = g_settings_new ("com.github.jeija.gtkterm");
  g_settings_get (settings, "window-size", "(ii)", &win->width, &win->height);
  win->maximized = g_settings_get_boolean (settings, "maximized");
  win->fullscreen = g_settings_get_boolean (settings, "fullscreen");
  g_object_unref (settings);
}

static void gtkterm_window_init (GtkTermWindow *window) {
  
  GtkWidget *popover;

  window->width = -1;
  window->height = -1;
  window->maximized = FALSE;
  window->fullscreen = FALSE;

  gtk_widget_init_template (GTK_WIDGET (window));

  popover = gtk_popover_menu_new_from_model (window->toolmenu);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (window->menubutton), popover);

  window->action_group = G_ACTION_GROUP(g_simple_action_group_new ());                                 

  //! TODO: Rename it.
  g_action_map_add_action_entries (G_ACTION_MAP (window->action_group),
                                   win_entries, 
                                   G_N_ELEMENTS (win_entries),
                                   window);
  gtk_widget_insert_action_group (GTK_WIDGET (window), "gtkterm_window", window->action_group);                                    
}

static void gtkterm_window_constructed (GObject *object) {
  GtkTermWindow *window = (GtkTermWindow *)object;

  gtkterm_window_load_state (window);

  gtk_window_set_default_size (GTK_WINDOW (window), window->width, window->height); 

  //! Connect to the terminal_changed so we can update the statusbar and window title
  g_signal_connect (window, "terminal_changed", G_CALLBACK(gtkterm_window_update), NULL);	 

  if (window->maximized)
    gtk_window_maximize (GTK_WINDOW (window));

  if (window->fullscreen)
    gtk_window_fullscreen (GTK_WINDOW (window));

  G_OBJECT_CLASS (gtkterm_window_parent_class)->constructed (object);
}

static void gtkterm_window_size_allocate (GtkWidget *widget,
                                       int width,
                                       int height,
                                       int baseline) {
                                        
  GtkTermWindow *window = (GtkTermWindow *)widget; 

  GTK_WIDGET_CLASS (gtkterm_window_parent_class)->size_allocate (widget,
                                                                          width,
                                                                          height,
                                                                          baseline);

  if (!window->maximized && !window->fullscreen)
    gtk_window_get_default_size (GTK_WINDOW (window), &window->width, &window->height);
}

static void surface_state_changed (GtkWidget *widget) {
  GtkTermWindow *window = (GtkTermWindow *)widget;
  GdkToplevelState new_state;

  new_state = gdk_toplevel_get_state (GDK_TOPLEVEL (gtk_native_get_surface (GTK_NATIVE (widget))));
  window->maximized = (new_state & GDK_TOPLEVEL_STATE_MAXIMIZED) != 0;
  window->fullscreen = (new_state & GDK_TOPLEVEL_STATE_FULLSCREEN) != 0;
}

static void gtkterm_window_realize (GtkWidget *widget) {
  GTK_WIDGET_CLASS (gtkterm_window_parent_class)->realize (widget);

  g_signal_connect_swapped (gtk_native_get_surface (GTK_NATIVE (widget)), "notify::state",
                            G_CALLBACK (surface_state_changed), widget);
}

static void gtkterm_window_unrealize (GtkWidget *widget) {
  g_signal_handlers_disconnect_by_func (gtk_native_get_surface (GTK_NATIVE (widget)),
                                        surface_state_changed, widget);

  GTK_WIDGET_CLASS (gtkterm_window_parent_class)->unrealize (widget);
}

static void gtkterm_window_dispose (GObject *object) {
  GtkTermWindow *window = (GtkTermWindow *)object;

  gtkterm_window_store_state (window);

  G_OBJECT_CLASS (gtkterm_window_parent_class)->dispose (object);
}

static void gtkterm_window_class_init (GtkTermWindowClass *class) {
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructed = gtkterm_window_constructed;
  object_class->dispose = gtkterm_window_dispose;

  widget_class->size_allocate = gtkterm_window_size_allocate;
  widget_class->realize = gtkterm_window_realize;
  widget_class->unrealize = gtkterm_window_unrealize;

  gtkterm_signals[SIGNAL_GTKTERM_TERMINAL_CHANGED] = g_signal_new ("terminal_changed",
                                            GTKTERM_TYPE_GTKTERM_WINDOW,
                                            G_SIGNAL_RUN_FIRST,
                                            0,
                                            NULL,
                                            NULL,
                                            NULL,
                                            G_TYPE_NONE,
                                            0,
                                            NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/github/jeija/gtkterm/gtkterm_main.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, message);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, infobar);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, statusbar);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, scrolled_window);   
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, menubutton);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, toolmenu);
  gtk_widget_class_bind_template_callback (widget_class, clicked_cb);
}

int main (int argc, char *argv[]) {
	GtkApplication *app;

	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	app = GTK_APPLICATION (g_object_new (GTKTERM_TYPE_APP,
                                       "application-id", 
                                       "com.github.jeija.gtkterm",
                                       "flags", 
                                       G_APPLICATION_HANDLES_OPEN,
                                       NULL));                                    

	return g_application_run (G_APPLICATION (app), argc, argv);
}
