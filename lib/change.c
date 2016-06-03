/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  change.c - libphclient code to modify server data
**
**  Mark D. Roth <roth@feep.net>
*/

#include <internal.h>

#include <errno.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif


/*
** ph_change() - change a PH entry.
** returns:
**	number of entries changed	success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			not logged in
**	PH_ERR_DATAERR			invalid field data
*/
int
ph_change(PH *ph, struct ph_fieldselector query[],
	  struct ph_fieldselector change[], int flags)
{
	char buf[PH_BUF_SIZE];
	size_t buflen = 0;
	int code;

#ifdef DEBUG
	puts("==> ph_change()");
#endif

	/* make sure we're logged in */
	if (ph->ph_auth == NULL)
		return PH_ERR_NOTLOG;

	/* encode command */
	buflen = strlcpy(buf, "change", sizeof(buf));
	buflen += ph_decode_selectors(query, buf + buflen,
				      sizeof(buf) - buflen);
	buflen += snprintf(buf + buflen, sizeof(buf) - buflen, " %s",
			   (BIT_ISSET(flags, PH_CHANGE_FORCE)
			    ? "force" : "make"));
	buflen += ph_decode_selectors(change, buf + buflen,
				      sizeof(buf) - buflen);

	/* send command */
#ifdef DEBUG
	printf("ph_change(): sending command: \"%s\"\n", buf);
#endif
	if (ph_send_command(ph, "%s", buf) == -1)
		return -1;

	/* check response code */
	do
	{
		if (ph_get_response(ph, &code, buf, sizeof(buf)) == -1)
			return -1;
	}
	while (code < LR_OK);

	switch (code)
	{
	case LR_OK:
		/* return number of entries changed */
		return atoi(buf);

	case LR_NOMATCH:
		/*
		** no match means 0 entries matched
		** we could also return PH_ERR_NOMATCH here...
		*/
		return 0;

	case LR_VALUE:
		return PH_ERR_DATAERR;

	case LR_NOTLOG:
		/*
		** can't happen, since we check for login above
		** but we'll handle it, just in case
		*/
		return PH_ERR_NOTLOG;

	case LR_READONLY:
		return PH_ERR_READONLY;

	default:
		break;
	}

	/* unknown response code from server, fail with EINVAL */
	errno = EINVAL;
	return -1;
}


/*
** ph_add() - add a PH entry.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			not logged in
**	PH_ERR_DATAERR			invalid field data
*/
int
ph_add(PH *ph, struct ph_fieldselector fields[])
{
	int code;
	size_t buflen = 0;
	char buf[PH_BUF_SIZE];

	/* make sure we're logged in */
	if (ph->ph_auth == NULL)
		return PH_ERR_NOTLOG;

	/* encode command */
	buflen = strlcpy(buf, "add", sizeof(buf));
	buflen += ph_decode_selectors(fields, buf + buflen,
				      sizeof(buf) - buflen);

	/* send to server */
	if (ph_send_command(ph, "%s", buf) == -1)
		return -1;

	/* check response */
	do
	{
		if (ph_get_response(ph, &code, buf, sizeof(buf)) == -1)
			return -1;
	}
	while (code < LR_OK);

	switch (code)
	{
	case LR_OK:
		return 0;

	case LR_SYNTAX:
		return PH_ERR_DATAERR;

	case LR_NOTLOG:
		/*
		** can't happen, since we check for login above
		** but we'll handle it, just in case
		*/
		return PH_ERR_NOTLOG;

	case LR_READONLY:
		return PH_ERR_READONLY;

	default:
		break;
	}

	/* unknown response code from server, fail with EINVAL */
	errno = EINVAL;
	return -1;
}


/*
** ph_delete() - delete a PH entry.
** returns:
**	number of entries deleted	success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			not logged in
*/
int
ph_delete(PH *ph, struct ph_fieldselector query[])
{
	char buf[PH_BUF_SIZE];
	int code;
	size_t buflen = 0;

	/* make sure we're logged in */
	if (ph->ph_auth == NULL)
		return PH_ERR_NOTLOG;

	/* encode command */
	buflen = strlcpy(buf, "delete", sizeof(buf));
	buflen += ph_decode_selectors(query, buf + buflen,
				      sizeof(buf) - buflen);

	/* send to server */
	if (ph_send_command(ph, "%s", buf) == -1)
		return -1;

	/* check response */
	do
	{
		if (ph_get_response(ph, &code, buf, sizeof(buf)) == -1)
			return -1;
	}
	while (code < LR_OK);

	switch (code)
	{
	case LR_OK:
		return atoi(buf);

	case LR_NOMATCH:
		/*
		** no match means 0 entries matched
		** we could also return PH_ERR_NOMATCH here...
		*/
		return 0;

	case LR_NOTLOG:
		/*
		** can't happen, since we check for login above
		** but we'll handle it, just in case
		*/
		return PH_ERR_NOTLOG;

	case LR_READONLY:
		return PH_ERR_READONLY;

	default:
		break;
	}

	/* unknown response code from server, fail with EINVAL */
	errno = EINVAL;
	return -1;
}


