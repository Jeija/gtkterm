/***********************************************************************/
/* gtkterm_serial.h                                                    */
/* -------                                                             */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Serial port access functions                                   */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef GTKTERM_SERIAL_H_
#define GTKTERM_SERIAL_H_

typedef enum {
	GTKTERM_SERIAL_PORT_OPEN,
	GTKTERM_SERIAL_PORT_CLOSE
	
} GtkTermSerialPortStatus;

typedef enum {
	GTKTERM_SERIAL_PORT_PARITY_NONE,	
	GTKTERM_SERIAL_PORT_PARITY_EVEN,
	GTKTERM_SERIAL_PORT_PARITY_ODD

} GtkTermSerialPortParity;

typedef enum {
	GTKTERM_SERIAL_PORT_FLOWCONTROL_NONE,	
	GTKTERM_SERIAL_PORT_FLOWCONTROL_XON_XOFF,
	GTKTERM_SERIAL_PORT_FLOWCONTROL_RTS_CTS,
	GTKTERM_SERIAL_PORT_FLOWCONTROL_RS485_HD,	

} GtkTermSerialPortFlowControl;

typedef enum {
    GTKTERM_SERIAL_PORT_CONNECTED,
    GTKTERM_SERIAL_PORT_DISCONNECTED,
    GTKTERM_SERIAL_PORT_ERROR

} GtkTermSerialPortState;

/**
 * @brief The typedef for the serial configuration.
 *
 */
typedef struct {

	char *port;
	long int baudrate;              			/**< 300 - 600 - 1200 - ... - 2000000 								*/
	int bits;                   				/**< 5 - 6 - 7 - 8													*/
	int stopbits;                   			/**< 1 - 2															*/
	GtkTermSerialPortParity parity;     		/**< 0 : None, 1 : Odd, 2 : Even									*/
	GtkTermSerialPortFlowControl flow_control;  /**< 0 : None, 1 : Xon/Xoff, 2 : RTS/CTS, 3 : RS485halfduplex		*/
	int rs485_rts_time_before_transmit; 		/**< Waiting time between RTS onand start to transmit				*/
	int rs485_rts_time_after_transmit;			/**< Waiting time between end of transmit and RTS on				*/
	bool disable_port_lock;						/**< Lock the serial port to one terminal (can cause garbage if no)	*/

} port_config_t;

G_BEGIN_DECLS

typedef struct _GtkTermSerialPort GtkTermSerialPort;

#define GTKTERM_TYPE_SERIAL_PORT gtkterm_serial_port_get_type ()
G_DECLARE_FINAL_TYPE (GtkTermSerialPort, gtkterm_serial_port, GTKTERM, SERIAL_PORT, GObject)

GtkTermSerialPort *gtkterm_serial_port_new (port_config_t *);

G_END_DECLS

/** Global functions */
char* gtkterm_serial_port_get_string (GtkTermSerialPort *);
GtkTermSerialPortState gtkterm_serial_port_get_status (GtkTermSerialPort *);
GError *gtkterm_serial_port_get_error (GtkTermSerialPort *);

extern const char GtkTermSerialPortStateString [][DEFAULT_STRING_LEN];

#endif // GTKTERM_SERIAL_H
