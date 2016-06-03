/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  internal.h - internal header file for libphclient
**
**  Mark D. Roth <roth@feep.net>
*/

#include <stdio.h>

#include <compat.h>

#include <phclient.h>

#include <ph_buffer.h>
#include <ph_mmgr.h>


#define PH_BUF_SIZE	8192		/* useful constant */

#define PH_SERVICE	"csnet-ns"	/* service name from /etc/services */
#define PH_PORT		105		/* fallback port number */


/* useful macros */
#define BIT_ISSET(bitfield, opt)	((bitfield) & (opt))
#define BIT_SET(bitfield, opt)		((bitfield) |= (opt))
#define BIT_CLEAR(bitfield, opt)	((bitfield) &= ~(opt))
#define BIT_TOGGLE(bitfield, opt) { \
  if (BIT_ISSET(bitfield, opt)) \
    BIT_CLEAR(bitfield, opt); \
  else \
    BIT_SET(bitfield, opt); \
  }


/* PH server handle */
struct ph_handle
{
	int ph_rfd[2];		/* file descriptor to read from server */
	int ph_wfd[2];		/* file descriptor to write to server */
	ph_debug_func_t ph_sendhook;	/* debug hook for sending */
	ph_debug_func_t ph_recvhook;	/* debug hook for receiving */
	void *ph_hook_data;		/* app-supplied ptr to pass to hooks */
	ph_buffer_t ph_buf;		/* receive buffer */
	struct ph_siteinfo *ph_siteinfo;	/* siteinfo list */
	struct ph_fieldinfo *ph_fieldlist;	/* field information */
	struct ph_option *ph_options;	/* server options */
	struct ph_auth *ph_auth;	/* currently logged-in user */
	ph_mmgr_t ph_mmgr;		/* memory management handle */
};


/***** auth.c **********************************************************/

/* auth data for current user */
struct ph_auth
{
	char *pa_alias;
	int pa_authtype;
	void *pa_authkey;
};

/* free ph_auth memory */
void ph_free_auth(struct ph_auth *);


/***** protocol.c ******************************************************/

/* buffer plugin to read from server */
ssize_t ph_buffer_read(void *, char *, size_t);

/* ph_get_response() - read and parse a response from the server. */
int ph_get_response(PH *, int *, char *, size_t);

/*
** ph_parse_response() - utility function to split up ':'-delimited
** sections of the text returned by ph_get_response()
*/
int ph_parse_response(char *, int *, char **, char **);

/* ph_send_command() - send a command to the server */
ssize_t
#ifdef STDC_HEADERS
ph_send_command(PH *, char *, ...);
#else
ph_send_command(PH *, char *, int);
#endif

/*
** ph_dequote_value() - convert a server-parsable quoted value to a literal
** field value.  returns number of characters written.
*/
size_t ph_dequote_value(char *, char *, size_t);

/*
** ph_decode_selectors() - decode a ph_fieldselector array into a string
** to send to the server.  returns number of characters written.
*/
size_t ph_decode_selectors(struct ph_fieldselector[], char *, size_t);


