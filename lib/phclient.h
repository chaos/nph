/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  phclient.h - main header file for libphclient
**
**  Mark D. Roth <roth@feep.net>
*/

#ifndef PHCLIENT_H
#define PHCLIENT_H


/* general return values */
#define PH_ERR_NOTLOG		-2	/* not logged in */
#define PH_ERR_NOTHERO		-3	/* not hero */
#define PH_ERR_DATAERR		-4	/* invalid field data */
#define PH_ERR_NOMATCH		-5	/* no matching entries found */
#define PH_ERR_TOOMANY		-6	/* too many entries matched */
#define PH_ERR_READONLY		-7	/* database is read-only */


/* PH server handle */
typedef struct ph_handle PH;


/***** auth.c **********************************************************/

/* macros for ph_login()'s authtype argument */

#ifdef PHLIB_XLOGIN	/* not currently supported */
# define PH_AUTH_FWTK		  4	/* TIS authsrv */
# define PH_AUTH_KRB4		  8	/* Kerberos 4 */
# define PH_AUTH_KRB5		 16	/* Kerberos 5 */
# define PH_AUTH_GSS		 32	/* GSS-API */
#endif

#define PH_AUTH_EMAIL		 64	/* email field */
#define PH_AUTH_PASSWORD	128	/* encrypted password */
#define PH_AUTH_CLEAR		256	/* cleartext password */


/*
** ph_login() - login to the PH server.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			authentication denied
*/
int ph_login(PH *, char *, int, void *);

/*
** ph_logout() - logout of the PH server.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			not logged in
*/
int ph_logout(PH *);

/*
** ph_suser() - assume another user's identity (heros only).
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOTHERO			not a hero
**	PH_ERR_NOTLOG			not logged in
*/
int ph_suser(PH *, char *);

/*
** ph_passwd() - change the current user's PH password.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			not logged in
*/
int ph_passwd(PH *, char *);

/*
** ph_whoami() - determine the currently logged-in alias
** returns:
**	pointer to the current alias	success
**	NULL				not logged in
*/
char *ph_whoami(PH *);


/***** change.c ********************************************************/

/* representation of a field selector */
struct ph_fieldselector
{
	char *pfs_field;
	char pfs_operation;		/* must be '=', '~', '>', or '<' */
	char *pfs_value;
};

/* flags for ph_change() */
#define PH_CHANGE_FORCE		1	/* use "force" instead of "make" */

/*
** ph_change() - change a PH entry.
** returns:
**	number of entries changed	success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			not logged in
**	PH_ERR_DATAERR			invalid field data
*/
int ph_change(PH *, struct ph_fieldselector[], struct ph_fieldselector[], int);

/*
** ph_add() - add a PH entry.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			not logged in
**	PH_ERR_DATAERR			invalid field data
*/
int ph_add(PH *, struct ph_fieldselector[]);

/*
** ph_delete() - delete a PH entry.
** returns:
**	number of entries deleted	success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			not logged in
*/
int ph_delete(PH *, struct ph_fieldselector[]);


/***** fields.c ********************************************************/

/* values for attribute bitmask */
#define PH_ATTRIB_ALWAYS		     1
#define PH_ATTRIB_ANY			     2
#define PH_ATTRIB_CHANGE		     4
#define PH_ATTRIB_DEFAULT		     8
#define PH_ATTRIB_ENCRYPT		    16
#define PH_ATTRIB_FORCEPUB		    32
#define PH_ATTRIB_INDEXED		    64
#define PH_ATTRIB_LOCALPUB		   128
#define PH_ATTRIB_LOOKUP		   256
#define PH_ATTRIB_NOMETA		   512
#define PH_ATTRIB_NOPEOPLE		  1024
#define PH_ATTRIB_PRIVATE		  2048
#define PH_ATTRIB_PUBLIC		  4096
#define PH_ATTRIB_SACRED		  8192
#define PH_ATTRIB_TURN			 16384
#define PH_ATTRIB_UNIQUE		 32768
#define PH_ATTRIB_PRIVLOOKUP		 65536
#define PH_ATTRIB_SACREDPERSON		131072

/* fields supported by the server */
struct ph_fieldinfo
{
	int pf_id;
	char **pf_fnames;
	int pf_max_size;
	char *pf_description;
	unsigned long pf_attrib;
};

/*
** ph_retrieve_fieldinfo() - obtain a list of fields supported by the PH server.
** returns:
**	0				success
**	-1				error (sets errno)
*/
int ph_retrieve_fieldinfo(PH *);

/*
** writes a string of space-delimited attribute names corresponding to
** the numeric bitmask
*/
void ph_decode_field_attributes(unsigned long, char *, size_t);

/*
** ph_get_fieldinfo() - return the description for a given field.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_DATAERR			unknown field
*/
int ph_get_fieldinfo(PH *, char *, struct ph_fieldinfo **);

/*
** ph_fieldinfo_iterate() - return the next field info structure.
** returns:
**	1				data returned
**	0				no more data
**	-1				error (sets errno)
*/
int ph_fieldinfo_iterate(PH *, struct ph_fieldinfo **);

/*
** ph_free_fieldinfo() - free fieldlist associated with ph.
*/
void ph_free_fieldinfo(PH *);

/*
** ph_is_fieldlist_cached() - check if field list is cached.
** returns:
**	0				not cached
**	1				cached
*/
int ph_is_fieldlist_cached(PH *);


/***** handle.c *******************************************************/

extern const char libphclient_version[];

extern const int libphclient_is_thread_safe;

/* typedef for debug hooks */
typedef void (*ph_debug_func_t)(void *, char *);


/* flags for ph_open() */
#define PH_OPEN_PRIVPORT	1	/* connect from a priveledged port */
#define PH_OPEN_ROUNDROBIN	2	/* try all addresses of a DNS
					   round-robin before failing */
#define PH_OPEN_LOCAL		4	/* open pipe to local PH server
					   process */
#define PH_OPEN_DONTID		8	/* don't identify library version
					   to server */

/* flags for ph_close() */
#define PH_CLOSE_FAST		1	/* drop connection without sending
					   "quit" */


/*
** ph_open() - opens a connection to the PH server.
** returns:
**	0				success
**	-1				error (sets errno)
*/
int ph_open(PH **, char *, int, ph_debug_func_t, ph_debug_func_t, void *);

/*
** ph_close() - closes a connection to the PH server.
**	0				success
**	-1				error (sets errno)
*/
int ph_close(PH *, int);

/*
** ph_rfd() - return the read file descriptor associated with the PH handle.
*/
int ph_rfd(PH *);

/*
** ph_wfd() - return the write file descriptor associated with the PH handle.
*/
int ph_wfd(PH *);


/***** misc.c **********************************************************/

/*
** ph_id() - send id command to server.
** returns:
**	0				success
**	-1				error (sets errno)
*/
int ph_id(PH *, char *);

/*
** ph_status() - get status info.
**	0				success
**	-1				error (sets errno)
*/
int ph_status(PH *, char **);

/*
** ph_external() - send "external" command.
**	0				success
**	-1				error (sets errno)
*/
int ph_external(PH *);


/***** options.c *******************************************************/

/* options supported by the server */
struct ph_option
{
	char *po_option;
	char *po_setting;
};

/*
** ph_retrieve_options() - obtain options from server.
**	0				success
**	-1				error (sets errno)
*/
int ph_retrieve_options(PH *);

/*
** ph_set_option() - set an option on the server.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_DATAERR			no such option
*/
int ph_set_option(PH *, char *, char *);

/*
** ph_get_option() - obtain the current setting of a given option.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_DATAERR			no such option
*/
int ph_get_option(PH *, char *, char **);

/*
** ph_option_iterate() - iterate through server option list.
** returns:
**	1				data returned
**	0				no more data
**	-1				error (sets errno)
*/
int ph_option_iterate(PH *, struct ph_option **);

/*
** ph_free_options() - free option list associated with PH handle ph.
*/
void ph_free_options(PH *);

/*
** ph_is_optionlist_cached() - check if option list is cached.
** returns:
**	0				not cached
**	1				cached
*/
int ph_is_optionlist_cached(PH *);


/***** protocol.c ******************************************************/

/*
** server reply codes (from qi distribution):
**	1XX - status
**	2XX - information
**	3XX - additional information or action needed
**	4XX - temporary errors
**	5XX - permanent errors
*/

#define LR_PROGRESS	100	/* in progress */
#define LR_ECHO		101	/* echoing cmd */
#define LR_NUMRET	102	/* how many entries are being returned */
#define LR_NONAME	103	/* no hostname found for IP address */

#define LR_OK		200	/* success */
#define LR_RONLY	201	/* database ready in read only mode */

#define LR_MORE		300	/* need more info */
#define LR_LOGIN	301	/* encrypt this string */
#define LR_XLOGIN	302	/* print this prompt */

#define LR_TEMP		400	/* temporary error */
#define LR_INTERNAL	401	/* database error, possibly temporary */
#define LR_LOCK		402	/* lock not obtained within timeout period */
#define LR_COULDA_BEEN	403	/* login would have been ok but db read only */
#define LR_DOWN		475	/* database unavailable; try again later */

#define LR_ERROR	500	/* hard error; general */
#define	LR_NOMATCH	501	/* no matches to query */
#define LR_TOOMANY	502	/* too many matches to query */
#define LR_AINFO	503	/* may not see that field */
#define LR_ASEARCH	504	/* may not search on that field */
#define LR_ACHANGE	505	/* may not change field */
#define LR_NOTLOG	506	/* must be logged in */
#define LR_FIELD	507	/* field unknown */
#define LR_ABSENT	508	/* field not present in entry */
#define LR_ALIAS	509	/* requested alias is already in use */
#define LR_AENTRY	510	/* may not change entry */
#define LR_ADD		511	/* may not add entries */
#define LR_VALUE	512	/* illegal value */
#define LR_OPTION	513	/* unknown option */
#define LR_UNKNOWN	514	/* unknown command */
#define LR_NOKEY	515	/* no indexed field found in query */
#define LR_AUTH		516	/* no authorization for query */
#define LR_READONLY	517	/* operation failed; database is read-only */
#define LR_LIMIT	518	/* too many entries selected for change */
#define LR_HISTORY	519	/* history substitution failed (obsolete) */
#define LR_XCPU		520	/* too much cpu used */
#define LR_ADDONLY	521	/* addonly option set and change command
				   applied to a field with data */
#define LR_ISCRYPT	522	/* attempt to view encrypted field */
#define LR_NOANSWER	523	/* "answer" was expected but not gotten */
#define LR_BADHELP	524	/* help topics cannot contain slashes */
#define LR_NOEMAIL	525	/* email authentication failed */
#define LR_NOADDR	526	/* host name address not found in DNS */
#define LR_MISMATCH	527	/* host = gethostbyaddr(foo); foo != gethostbyname(host) */
#define LR_KDB5		528	/* general kerberos database error */
#define LR_NOAUTH	529	/* selected authentication method not avail */
#define LR_NOCMD	598	/* no such command */
#define LR_SYNTAX	599	/* syntax error */

/*
** ph_set_sendhook() - set the send hook function.
*/
void ph_set_sendhook(PH *, ph_debug_func_t);

/*
** ph_set_recvhook() - set the receive hook function.
*/
void ph_set_recvhook(PH *, ph_debug_func_t);

/*
** ph_set_hookdata() - set the hook data to be passed to the debug hooks
*/
void ph_set_hookdata(PH *, void *);

/*
** ph_encode_selector() - parse the selector string and add an entry to
** the selectors array.
** returns:
**	0				success
**	-1				parse error
*/
int ph_encode_selector(char *, int, int *, struct ph_fieldselector **);

/*
** ph_free_selectors() - free memory associated with an array of selectors
*/
void ph_free_selectors(struct ph_fieldselector *);


/***** query.c *********************************************************/

struct ph_fieldvalue
{
	int pfv_code;
	char *pfv_field;
	char *pfv_value;
};

typedef struct ph_fieldvalue *ph_entry;

/*
** ph_query() - send a query to the PH server and return matching entries.
** returns:
**	number of entries returned	success
**	-1				error (sets errno)
**	PH_ERR_TOOMANY			too many entries matched
**	PH_ERR_NOMATCH			no matching entries
**	PH_ERR_DATAERR			invalid query
*/
int ph_query(PH *, struct ph_fieldselector[], char *[],
	     ph_entry **);

/*
** ph_free_entries() - free memory returned by ph_query().
*/
void ph_free_entries(ph_entry *);


/***** redirection.c **************************************************/

/*
** ph_advertised_email() - returns alias-based email address for alias
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOMATCH			alias does not exist
**	PH_ERR_DATAERR			no email field
*/
int ph_advertised_email(PH *, char *, int, char **);

/*
** ph_email_resolve() - returns the expanded email address for user
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOMATCH			no single match found
**	PH_ERR_DATAERR			no email field
*/
int ph_email_resolve(PH *, char *, char *, char **);

/*
** ph_advertised_www() - returns alias-based URL for alias
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOMATCH			alias does not exist
**	PH_ERR_DATAERR			no www field
*/
int ph_advertised_www(PH *, char *, int, char **);

/*
** ph_www_resolve() - returns the expanded URL for user
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOMATCH			no single entry matched
**	PH_ERR_DATAERR			no www field
*/
int ph_www_resolve(PH *, char *, char **);


/***** serverlist.c ****************************************************/

/* structure to describe other PH servers */
struct ph_serversite
{
	char *ss_server;
	char *ss_site;
};

struct ph_serverlist
{
	struct ph_serversite *sl_list;
	unsigned int sl_numservers;
};


/*
** ph_serverlist_add() - add an entry to a list of known PH servers.
** returns:
**	0				success
**	-1				error (sets errno)
*/
int ph_serverlist_add(struct ph_serverlist *, char *, char *);

/*
** ph_serverlist_iterate() - iterate through all matching PH server entries.
** returns:
**	1				data returned
**	0				no more data
**	-1				error (sets errno)
*/
int ph_serverlist_iterate(struct ph_serverlist *, char *, char *,
			  struct ph_serversite **);

/*
** ph_serverlist_merge() - read list of known PH servers from the server
** associated with ph and merge them into serverlist.
** returns:
**	0				success
**	-1				error (sets errno)
*/
int ph_serverlist_merge(PH *, struct ph_serverlist *);

/*
** ph_free_serverlist() - free memory associated with serverlist.
*/
void ph_free_serverlist(struct ph_serverlist *);


/***** siteinfo.c ******************************************************/

/* siteinfo provided by the server */
struct ph_siteinfo
{
	char *ps_info;
	char *ps_data;
};

/*
** ph_retrieve_siteinfo() - read siteinfo from server.
** returns:
**	0				success
**	-1				error (sets errno)
*/
int ph_retrieve_siteinfo(PH *);

/*
** ph_get_siteinfo() - return the value of a specific info field.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_DATAERR			unknown siteinfo field
*/
int ph_get_siteinfo(PH *, char *, char **);

/*
** ph_siteinfo_iterate() - iterate through siteinfo list.
** returns:
**	1				data returned
**	0				no more data
**	-1				error (sets errno)
*/
int ph_siteinfo_iterate(PH *, struct ph_siteinfo **);

/*
** ph_free_siteinfo() - free the siteinfo list associated with ph.
*/
void ph_free_siteinfo(PH *);

/*
** ph_is_siteinfo_cached() - check if siteinfo data is cached.
** returns:
**	0				not cached
**	1				cached
*/
int ph_is_siteinfo_cached(PH *);


#endif /* PHCLIENT_H */
