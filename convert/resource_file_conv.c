/***********************************************************************
 * resource_config.c                                 
 * -----------------                                    
 *           GTKTerm Software                              
 *                      (c) Julien Schmitt  
 *                                              
 * ------------------------------------------------------------------- 
 *                                           
 *   \brief Purpose                               
 *      Save and load configuration from resource file          
 *                                              
 *   ChangeLog                                
 *      - 2.0 : Remove parsecfg. Switch to GKeyFile  
 *              Migration done by Jens Georg (phako)       
 *                                                                
 ***********************************************************************/

#include <stdio.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <pango/pango-font.h>

#include <config.h>

#include "i18n.h"
#include "serial.h"
#include "term_config.h"
#include "resource_file_conv.h"
#include "i18n.h"
#include "interface.h"
#include "macros.h"

//! Default configuration filename
#define CONFIGURATION_FILENAME ".gtktermrc"
#define CONFIGURATION_FILENAME_V1 ".gtktermrc.v1"
//! The key file
GFile *config_file;

//! Define all configuration items which are used
//! in the resource file. it is an index to ConfigurationItem.
enum {
		CONF_ITEM_SERIAL_PORT,
		CONF_ITEM_SERIAL_BAUDRATE,
		CONF_ITEM_SERIAL_BITS,
		CONF_ITEM_SERIAL_STOPBITS,
		CONF_ITEM_SERIAL_PARITY,
		CONF_ITEM_SERIAL_FLOW_CONTROL,
		CONF_ITEM_TERM_WAIT_DELAY,
		CONF_ITEM_TERM_WAIT_CHAR,
		CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX,
		CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX,
		CONF_ITEM_TERM_MACROS,
		CONF_ITEM_TERM_RAW_FILENAME,
		CONF_ITEM_TERM_ECHO,
		CONF_ITEM_TERM_CRLF_AUTO,
		CONF_ITEM_SERIAL_DISABLE_PORT_LOCK,
		CONF_ITEM_TERM_FONT,
		CONF_ITEM_TERM_TIMESTAMP,		
		CONF_ITEM_TERM_BLOCK_CURSOR,		
		CONF_ITEM_TERM_SHOW_CURSOR,
		CONF_ITEM_TERM_ROWS,
		CONF_ITEM_TERM_COLS,
		CONF_ITEM_TERM_SCROLLBACK,
		CONF_ITEM_TERM_VISUAL_BELL,		
		CONF_ITEM_TERM_FOREGROUND_RED,
		CONF_ITEM_TERM_FOREGROUND_GREEN,
		CONF_ITEM_TERM_FOREGROUND_BLUE,
		CONF_ITEM_TERM_FOREGROUND_ALPHA,
		CONF_ITEM_TERM_BACKGROUND_RED,
		CONF_ITEM_TERM_BACKGROUND_GREEN,
		CONF_ITEM_TERM_BACKGROUND_BLUE,
		CONF_ITEM_TERM_BACKGROUND_ALPHA,	
		CONF_ITEM_LAST						//! Checking as last item in the list.
};

// Used configuration options to hold consistency between load/save functions
const char ConfigurationItem [][32] = {
		"port",
		"baudrate",
		"bits",
		"stopbits",
		"parity",
		"flow_control",
		"term_wait_delay",
		"term_wait_char",
		"rs485_rts_time_before_tx",
		"rs485_rts_time_after_tx",
		"macros",
		"term_raw_filename",		
		"term_echo",
		"term_crlfauto",
		"disable_port_lock",
		"term_font",
		"term_show_timestamp",
		"term_block_cursor",				
		"term_show_cursor",
		"term_rows",
		"term_columns",
		"term_scrollback",
		"term_visual_bell",
		"term_foreground_red",
		"term_foreground_green",
		"term_foreground_blue",
		"term_foreground_alpha",
		"term_background_red",
		"term_background_green",
		"term_background_blue",
		"term_background_alpha",
};

void config_file_init(void)
{
	/*
	 * Old location of configuration file was $HOME/.gtktermrc
	 * New location is $XDG_CONFIG_HOME/.gtktermrc
	 *
	 * If configuration file exists at new location, use that one.
	 * Otherwise, if file exists at old location, move file to new location.
	 *
         * Version 2.0: Because we have to use gtkterm_conv, the file is always at
	 * the user directory. So we can skip eventually moving the file.
	 */
	GFile *config_file_old = g_file_new_build_filename(getenv("HOME"), CONFIGURATION_FILENAME, NULL);
	GFile *config_file_v1 = g_file_new_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME_V1, NULL);
	config_file = g_file_new_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME, NULL);

	if (!g_file_query_exists(config_file, NULL) && g_file_query_exists(config_file_old, NULL))
		g_file_move(config_file_old, config_file, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);

	// Save the original file
	g_file_copy(config_file, config_file_v1, G_FILE_COPY_BACKUP, NULL, NULL, NULL, NULL);
}

/* Dumps the section to the command line */
/* We will use this with auto package testing within Debian */
void dump_configuration_to_cli (char *section) {
	char str_buffer[32];

	i18n_printf (_("Configuration loaded from file: [%s]\n"), section);

	//! Print the serial port items
	i18n_printf (_("\nSerial port\n"));
	i18n_printf (_("Port                     : %s\n"), port_conf.port);
	i18n_printf (_("Speed                    : %ld\n"), port_conf.baudrate);
	i18n_printf (_("Bits                     : %d\n"), port_conf.bits);
	i18n_printf (_("Stopbits                 : %d\n"), port_conf.stopbits);

	switch (port_conf.parity) {
		case 0:
			strcpy (str_buffer, _("none"));
			break;
		case 1:
			strcpy (str_buffer, _("odd"));
			break;
		case 2:
			strcpy (str_buffer, _("even"));
			break;
		default: // May never get here
			strcpy (str_buffer, _("unknown"));
	}
	i18n_printf (_("Parity                   : %s\n"), str_buffer);

	switch (port_conf.flow_control) {
		case 0:
			strcpy (str_buffer, _("none"));
			break;
		case 1:
			strcpy (str_buffer, _("xon"));
			break;
		case 2:
			strcpy (str_buffer, _("xoff"));
			break;
		case 3:
			strcpy (str_buffer, _("rs485"));
			break;
		default: // May never get here
			strcpy (str_buffer, _("unknown"));
	}
	i18n_printf (_("Flow control             : %s\n"), str_buffer);
	i18n_printf (_("RS485 RTS time before TX : %d\n"), port_conf.rs485_rts_time_before_transmit);
	i18n_printf (_("RS485 RTS time after TX  : %d\n"), port_conf.rs485_rts_time_after_transmit);
	i18n_printf (_("Disable port lock        : %s\n"), port_conf.disable_port_lock ? "True" : "False");

	//! Print the terminal items
	i18n_printf (_("\nTerminal\n"));
	i18n_printf (_("Font                     : %s\n"), pango_font_description_to_string (term_conf.font));
	i18n_printf (_("Echo                     : %s\n"), term_conf.echo ? _("True") : _("False"));
	i18n_printf (_("CRLF                     : %s\n"), term_conf.crlfauto ? _("True") : _("False"));
	i18n_printf (_("Wait delay               : %d\n"), term_conf.delay);
	i18n_printf (_("Wait char                : %d\n"), term_conf.char_queue);
	i18n_printf (_("Timestamp                : %s\n"), term_conf.timestamp ? _("True") : _("False"));
	i18n_printf (_("Block cursor             : %s\n"), term_conf.block_cursor ? _("True") : _("False"));
	i18n_printf (_("Show cursor              : %s\n"), term_conf.show_cursor ? _("True") : _("False"));
	i18n_printf (_("Rows                     : %d\n"), term_conf.rows);
	i18n_printf (_("Cols                     : %d\n"), term_conf.columns);
	i18n_printf (_("Scrollback               : %d\n"), term_conf.scrollback);
	i18n_printf (_("Visual bell              : %s\n"), term_conf.visual_bell ? _("True") : _("False"));
	i18n_printf (_("Background color red     : %f\n"), term_conf.background_color.red);
	i18n_printf (_("Background color blue    : %f\n"), term_conf.background_color.blue);
	i18n_printf (_("Background color green   : %f\n"), term_conf.background_color.green);
	i18n_printf (_("Background color alpha   : %f\n"), term_conf.background_color.alpha);
	i18n_printf (_("Foreground color red     : %f\n"), term_conf.foreground_color.red);
	i18n_printf (_("Foreground color blue    : %f\n"), term_conf.foreground_color.blue);
	i18n_printf (_("Foreground color green   : %f\n"), term_conf.foreground_color.green);
	i18n_printf (_("Foreground color alpha   : %f\n"), term_conf.foreground_color.alpha);

	//! ... and the macro's
	i18n_printf (_("\nMacro's\n"));
	i18n_printf (_(" Nr  Shortcut  Command\n"));
	for (int i = 0; i < macro_count(); i++) {
		i18n_printf ("[%2d] %-8s  %s\n", i, macros[i].shortcut, macros[i].action);
	}
}

/* Save <section> configuration to file */
void save_configuration_to_file(GKeyFile *config)
{
	GError *error = NULL;

	g_key_file_save_to_file (config, g_file_get_path(config_file), &error);

}

/* Load the configuration from <section> into the port and term config */
/* If it does not exists it creates one from the defaults              */
int load_configuration_from_file(const char *section)
{
	GKeyFile *config_object = NULL;
	GError *error = NULL;
	char *str = NULL;
	char **macrostring;
	gsize nr_of_strings;
	int value = 0;

	char *string = NULL;

	//! Load the key file
	//! Note: all sections are loaded into memory.
	config_object = g_key_file_new ();
	if (!g_key_file_load_from_file (config_object, g_file_get_path (config_file), G_KEY_FILE_NONE, &error)) {
		g_debug ("Failed to load configuration file: %s", error->message);
		g_error_free (error);
		g_key_file_unref (config_object);

		return -1;
	}

	//! Check if the <section> exists in the key file.
	if (!g_key_file_has_group (config_object, section)) {
		string = g_strdup_printf(_("No section \"%s\" in configuration file\n"), section);
		show_message(string, MSG_ERR);
		g_free(string);
		g_key_file_unref (config_object);
		return -1;
	}

	//! First initialize with a default structure.
	//! Not really needed.
	hard_default_configuration();

	// Load all key file items into the port_conf or term_conf so we can use it 
	str = g_key_file_get_string (config_object, section, ConfigurationItem[CONF_ITEM_SERIAL_PORT], NULL);
	if (str != NULL) {
		g_strlcpy (port_conf.port, str, sizeof (port_conf.port) - 1);
		g_free (str);
	}

	value = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_SERIAL_BAUDRATE], NULL);
	if (value != 0) {
		port_conf.baudrate = value;
	}

	value = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_SERIAL_BITS], NULL);
	if (value != 0) {
		port_conf.bits = value;
	}

	value = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_SERIAL_STOPBITS], NULL);
	if (value != 0) {
		port_conf.stopbits = value;
	}

	str = g_key_file_get_string (config_object, section, ConfigurationItem[CONF_ITEM_SERIAL_PARITY], NULL);
	if (str != NULL) {
		if(!g_ascii_strcasecmp(str, "none"))
			port_conf.parity = 0;
		else if(!g_ascii_strcasecmp(str, "odd"))
			port_conf.parity = 1;
		else if(!g_ascii_strcasecmp(str, "even"))
			port_conf.parity = 2;
		g_free (str);
	}

	str = g_key_file_get_string (config_object, section,  ConfigurationItem[CONF_ITEM_SERIAL_FLOW_CONTROL], NULL);
	if (str != NULL) {
		if(!g_ascii_strcasecmp(str, "none"))
			port_conf.flow_control = 0;
		else if(!g_ascii_strcasecmp(str, "xon"))
			port_conf.flow_control = 1;
		else if(!g_ascii_strcasecmp(str, "rts"))
			port_conf.flow_control = 2;
		else if(!g_ascii_strcasecmp(str, "rs485"))
			port_conf.flow_control = 3;
    		
		g_free (str);
	}

	term_conf.delay = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_TERM_WAIT_DELAY], NULL);

	value = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_TERM_WAIT_CHAR], NULL);
	if (value != 0) {
		term_conf.char_queue = (signed char) value;
	} else {
		term_conf.char_queue = -1;
	}
    
	port_conf.rs485_rts_time_before_transmit = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], NULL);
	port_conf.rs485_rts_time_after_transmit = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], NULL);
	term_conf.echo = g_key_file_get_boolean (config_object, section, ConfigurationItem[CONF_ITEM_TERM_ECHO], NULL);
	term_conf.crlfauto = g_key_file_get_boolean (config_object, section, ConfigurationItem[CONF_ITEM_TERM_CRLF_AUTO], NULL);
	port_conf.disable_port_lock = g_key_file_get_boolean (config_object, section, ConfigurationItem[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], NULL);

	//! The Font is a Pango structure. This only can be added to a terminal
	//! So we have to convert it.
	g_clear_pointer (&term_conf.font, pango_font_description_free);
	str = g_key_file_get_string (config_object, section, ConfigurationItem[CONF_ITEM_TERM_FONT], NULL);
	term_conf.font = pango_font_description_from_string (str);
	g_free (str);

	/// Convert the stringlist to macros. Existing shortcuts will be delete from convert_string_to_macros
	macrostring = g_key_file_get_string_list (config_object, section, ConfigurationItem[CONF_ITEM_TERM_MACROS], &nr_of_strings, NULL);
	convert_string_to_macros (macrostring, nr_of_strings);
	g_strfreev(macrostring);

	term_conf.show_cursor = g_key_file_get_boolean (config_object, section, ConfigurationItem[CONF_ITEM_TERM_SHOW_CURSOR], NULL);
	term_conf.rows = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_TERM_ROWS], NULL);
	term_conf.columns = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_TERM_COLS], NULL);
	value = g_key_file_get_integer (config_object, section, ConfigurationItem[CONF_ITEM_TERM_SCROLLBACK], NULL);
	if (value != 0) {
		term_conf.scrollback = value;
	}

	term_conf.visual_bell = g_key_file_get_boolean (config_object, section, ConfigurationItem[CONF_ITEM_TERM_VISUAL_BELL], NULL);
	term_conf.foreground_color.red = g_key_file_get_double (config_object, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_RED], NULL);
	term_conf.foreground_color.green = g_key_file_get_double (config_object, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_GREEN], NULL);
	term_conf.foreground_color.blue = g_key_file_get_double (config_object, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_BLUE], NULL);
	term_conf.foreground_color.alpha = g_key_file_get_double (config_object, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_ALPHA], NULL);
	term_conf.background_color.red = g_key_file_get_double (config_object, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_RED], NULL);
	term_conf.background_color.green = g_key_file_get_double (config_object, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_GREEN], NULL);
	term_conf.background_color.blue = g_key_file_get_double (config_object, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_BLUE], NULL);
	term_conf.background_color.alpha = g_key_file_get_double (config_object, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_ALPHA], NULL);

	return 0;
}

//! This checks if the configuration file exists. If not it creates a
//! new [default]
int check_configuration_file(void)
{
	struct stat my_stat;
	char *string = NULL;

	/* is configuration file present ? */
	if(stat(g_file_get_path(config_file), &my_stat) == 0)
	{
		/* If bad configuration file, fallback to _hardcoded_ defaults! */
		if(load_configuration_from_file("default") == -1)
		{
			hard_default_configuration();
			return -1;
		}
	} /* if not, create it, with the [default] section */
	else
	{
		GKeyFile *config;

		string = g_strdup_printf(_("Configuration file (%s) with [default] configuration has been created.\n"), g_file_get_path(config_file));
		show_message(string, MSG_WRN);
		g_free(string);

		config = g_key_file_new ();
		hard_default_configuration();

		//! Put the new default in the key file
		copy_configuration(config, "default");

		//! And save the config to file
		save_configuration_to_file(config);		

		g_key_file_unref (config);
	}

	return 0;
}

//! Copy the active configuration into <section> of the Key file
void copy_configuration(GKeyFile *configrc, const char *section)
{
	char *string = NULL;
	char **string_list = NULL;
	gsize nr_of_strings = 0;

	g_key_file_set_string (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_PORT], port_conf.port);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_BAUDRATE], port_conf.baudrate);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_BITS], port_conf.bits);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_STOPBITS], port_conf.stopbits);

	switch(port_conf.parity)
	{
		case 0:
			string = g_strdup_printf("none");
			break;
		case 1:
			string = g_strdup_printf("odd");
			break;
		case 2:
			string = g_strdup_printf("even");
			break;
		default:
		    string = g_strdup_printf("none");
	}

	g_key_file_set_string (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_PARITY], string);
	g_free(string);

	switch(port_conf.flow_control)
	{
		case 0:
			string = g_strdup_printf("none");
			break;
		case 1:
			string = g_strdup_printf("xon");
			break;
		case 2:
			string = g_strdup_printf("rts");
			break;
		case 3:
			string = g_strdup_printf("rs485");
			break;
		default:
			string = g_strdup_printf("none");
	}

	g_key_file_set_string (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_FLOW_CONTROL], string);
	g_free(string);

	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_WAIT_DELAY], term_conf.delay);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_WAIT_CHAR], term_conf.char_queue);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX],
    	                        port_conf.rs485_rts_time_before_transmit);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX],
    	                        port_conf.rs485_rts_time_after_transmit);

	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_ECHO], term_conf.echo);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_CRLF_AUTO], term_conf.crlfauto);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], port_conf.disable_port_lock);

	string = pango_font_description_to_string (term_conf.font);
	g_key_file_set_string (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FONT], string);
	g_free(string);

	//! Macros are an array of strings, so we have to convert it
	//! All macros ends up in the string_list
	string_list = g_malloc ( macro_count () * sizeof (char *) * 2 + 1);
	nr_of_strings = convert_macros_to_string (string_list);
	g_key_file_set_string_list (configrc, section, ConfigurationItem[CONF_ITEM_TERM_MACROS], (const char * const*) string_list, nr_of_strings);
	g_free(string_list);	

	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_SHOW_CURSOR], term_conf.show_cursor);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BLOCK_CURSOR], term_conf.block_cursor);	
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_ROWS], term_conf.rows);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_COLS], term_conf.columns);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_SCROLLBACK], term_conf.scrollback);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_VISUAL_BELL], term_conf.visual_bell);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_TIMESTAMP], term_conf.timestamp);	

	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_RED], term_conf.foreground_color.red);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_GREEN], term_conf.foreground_color.green);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_BLUE], term_conf.foreground_color.blue);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_ALPHA], term_conf.foreground_color.alpha);

	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_RED], term_conf.background_color.red);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_GREEN], term_conf.background_color.green);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_BLUE], term_conf.background_color.blue);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_ALPHA], term_conf.background_color.alpha);
}

//! Remove a section from the file
//! TODO: Perhaps remove because we dont need it.
int remove_section(char *cfg_file, char *section)
{
	FILE *f = NULL;
	char *buffer = NULL;
	char *buf;
	size_t size;
	char *to_search;
	size_t i, j, length, sect;

	f = fopen(cfg_file, "r");
	if(f == NULL)
	{
		perror(cfg_file);
		return -1;
	}

	fseek(f, 0L, SEEK_END);
	size = ftell(f);
	rewind(f);

	buffer = g_malloc(size);
	if(buffer == NULL)
	{
		perror("malloc");
		return -1;
	}

	if(fread(buffer, 1, size, f) != size)
	{
		perror(cfg_file);
		fclose(f);
		return -1;
	}

	to_search = g_strdup_printf("[%s]", section);
	length = strlen(to_search);

	/* Search section */
	for(i = 0; i < size - length; i++)
	{
		for(j = 0; j < length; j++)
		{
			if(to_search[j] != buffer[i + j])
				break;
		}

		if(j == length)
			break;
	}

	if(i == size - length)
	{
		i18n_printf(_("Cannot find section %s\n"), to_search);
		return -1;
	}

	sect = i;

	/* Search for next section */
	for(i = sect + length; i < size; i++)
	{
		if(buffer[i] == '[')
			break;
	}

	f = fopen(cfg_file, "w");
	if(f == NULL)
	{
		perror(cfg_file);
		return -1;
	}

	fwrite(buffer, 1, sect, f);
	buf = buffer + i;
	fwrite(buf, 1, size - i, f);
	fclose(f);

	g_free(to_search);
	g_free(buffer);

	return 0;
}

//! Create a new <default> configuration
void hard_default_configuration(void)
{
	g_strlcpy(port_conf.port, DEFAULT_PORT, sizeof (port_conf.port));
	port_conf.baudrate = DEFAULT_BAUDRATE;
	port_conf.parity = 0;
	port_conf.bits = DEFAULT_BITS;
	port_conf.stopbits = DEFAULT_STOPBITS;
	port_conf.flow_control = FALSE;
	port_conf.rs485_rts_time_before_transmit = DEFAULT_DELAY_RS485;
	port_conf.rs485_rts_time_after_transmit = DEFAULT_DELAY_RS485;
	port_conf.disable_port_lock = FALSE;

	term_conf.char_queue = DEFAULT_CHAR;
	term_conf.delay = DEFAULT_DELAY;
	term_conf.echo = DEFAULT_ECHO;
	term_conf.crlfauto = FALSE;
	term_conf.timestamp = FALSE;
	term_conf.font = pango_font_description_from_string (DEFAULT_FONT);
	term_conf.block_cursor = TRUE;
	term_conf.show_cursor = TRUE;
	term_conf.rows = 80;
	term_conf.columns = 25;
	term_conf.scrollback = DEFAULT_SCROLLBACK;
	term_conf.visual_bell = TRUE;

	set_color (&term_conf.foreground_color, 0.66, 0.66, 0.66, 1.0);
	set_color (&term_conf.background_color, 0, 0, 0, 1.0);
}

//! validate the active configuration
void validate_configuration(void)
{
	char *string = NULL;

	switch(port_conf.baudrate)
	{
		case 300:
		case 600:
		case 1200:
		case 2400:
		case 4800:
		case 9600:
		case 19200:
		case 38400:
		case 57600:
		case 115200:
		case 230400:
		case 460800:
		case 576000:
		case 921600:
		case 1000000:
		case 2000000:
			break;

		default:
			string = g_strdup_printf(_("Baudrate %ld may not be supported by all hardware"), port_conf.baudrate);
			show_message(string, MSG_ERR);
	    
			g_free(string);
	}

	if(port_conf.stopbits != 1 && port_conf.stopbits != 2)
	{
		string = g_strdup_printf(_("Invalid number of stop-bits: %d\nFalling back to default number of stop-bits number: %d\n"), port_conf.stopbits, DEFAULT_STOPBITS);
		show_message(string, MSG_ERR);
		port_conf.stopbits = DEFAULT_STOPBITS;

		g_free(string);
	}

	if(port_conf.bits < 5 || port_conf.bits > 8)
	{
		string = g_strdup_printf(_("Invalid number of bits: %d\nFalling back to default number of bits: %d\n"), port_conf.bits, DEFAULT_BITS);
		show_message(string, MSG_ERR);
		port_conf.bits = DEFAULT_BITS;

		g_free(string);
	}

	if(term_conf.delay < 0 || term_conf.delay > 500)
	{
		string = g_strdup_printf(_("Invalid delay: %d ms\nFalling back to default delay: %d ms\n"), term_conf.delay, DEFAULT_DELAY);
		show_message(string, MSG_ERR);
		term_conf.delay = DEFAULT_DELAY;

		g_free(string);
	}

	if(term_conf.font == NULL)
		term_conf.font = pango_font_description_from_string (DEFAULT_FONT);
}

//! Convert the colors RGB to internal color scheme
void set_color(GdkRGBA *color, float R, float G, float B, float A)
{
	color->red = R;
	color->green = G;
	color->blue = B;
	color->alpha = A;
}