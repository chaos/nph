/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  nph.h - main header file for nph
**
**  Mark D. Roth <roth@feep.net>
*/

#include <stdio.h>

#include <config.h>
#include <compat.h>
#include <phclient.h>

#include <setjmp.h>



#define NPH_BUF_SIZE	8192


/***** commands.c ******************************************************/

/* parse a command vector and call the appropriate function */
int nph_command(char *);

/* prints the help text for command */
void print_help(char *);

/* client help (not server help command) */
int nph_help(int, char **);

/* quit command */
int nph_quit(int, char **);

/* whoami command */
int nph_whoami(int, char **);


/***** conf.c **********************************************************/

/* change the authentication method used to login to the server */
int nph_option(int, char **);

/* get the value for an option */
int get_option(char *, void *);

/* set the value for an option */
int set_option(char *, char *);

/* connect to PH server and initialize options */
int server_init(PH **, char *);


/***** content.c *******************************************************/

/* select entries for further processing */
int nph_select(int, char **);

/* change entries on the server */
int nph_change(int, char **);

/* delete entries */
int nph_delete(int, char **);

/* add an entry */
int nph_add(int, char **);

/* retrieve entries from the server */
int nph_query(int, char **);

/* me command */
int nph_me(int, char **);

/* resolve an email address */
int nph_resolvemail(int, char **);

/* determine the advertised email address */
int nph_publicemail(int, char **);

/* find www field for a given alias */
int nph_resolvewww(int, char **);

/* determine advertised URL for a given alias */
int nph_publicwww(int, char **);


/***** edit.c **********************************************************/

/* edit command */
int nph_edit(int, char **);


/***** interactive.c ***************************************************/

/* interactive mode */
void interactive(void);


/***** init.c **********************************************************/

/* verify that a field exists on the server */
int check_field(PH *, char *);

/* verify that a list of fields exist on the server */
int check_fields(PH *, char *);

/* read the system-wide ph_server file */
char *read_server_file(void);

/* change servers */
int nph_connect(int, char **);

/* read the user's ~/.nphrc file */
int read_nphrc(char *);

/* read commands from a file */
int nph_source(int, char **);

/* read commands from a file and execute them */
int read_command_file(char *);


/***** login.c *********************************************************/

/* send an id for the server's logs */
int nph_id(int, char **);

/* make the session non-local */
int nph_external(int, char **);

/* become another user (heros only) */
int nph_suser(int, char **);

/* login to the server */
int nph_login(int, char **);

/* logout from the server */
int nph_logout(int, char **);

/* change password */
int nph_passwd(int, char **);


/***** nph.c ***********************************************************/

extern PH *ph;
extern char *ph_server;
extern struct ph_serverlist serverlist;
extern short got_servers;	/* flag to indicate we've merged the
				   server list from ph into serverlist */


/***** output.c ********************************************************/

/* output to a pipe */
int output_pipe(char *);

/* send output to a file (optionally appending) */
int output_file(char *, int);

/* reset output file */
void reset_output(int);

/* print a message */
#ifdef STDC_HEADERS
int nph_printf(int, char *, ...);
#else
int nph_printf(int, char *, int);
#endif

/*
** returns 1 if one of the aliases of result_name is listed in showfields,
** 0 if not listed, or -1 on error from ph_get_fieldinfo()
*/
int field_match(char *, char *[]);

/* print an entry */
void print_entry(ph_entry, char *[], int);

/* debugging hooks */
void send_debug(void *, char *);
void recv_debug(void *, char *);


/***** serverinfo.c ****************************************************/

/* view list of known PH servers */
int nph_listservers(int, char **);

/* add entry to list of known PH servers */
int nph_serveradd(int, char **);

/* view server's message of the day */
int nph_status(int, char **);

/* show or set server options */
int nph_set(int, char **);

/* view siteinfo data */
int nph_siteinfo(int, char **);

/* view fields supported by server */
int nph_fields(int, char **);


/***** sighandler.c ****************************************************/

extern sigjmp_buf complete_env, command_env;
extern int sighandler_state;

/* values for sighandler_state */
#define HANDLER_DEFAULT		0	/* default dispositions */
#define HANDLER_INPUT		1	/* normal input */
#define HANDLER_COMPLETION	2	/* commandline completion */
#define HANDLER_COMMAND		3	/* performing operation */

/* set up signal handlers */
void set_signals(void);


