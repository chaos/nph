/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  misc.c - miscellaneous PH access code for libphclient
**
**  Mark D. Roth <roth@feep.net>
*/

#include <internal.h>

#include <errno.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
#endif


/*
** ph_id() - send id command to server.
** returns:
**	0				success
**	-1				error (sets errno)
*/
int
ph_id(PH *ph, char *id)
{
	int code;

	if (ph_send_command(ph, "id %s", id) == -1)
		return -1;

	do
	{
		if (ph_get_response(ph, &code, NULL, 0) == -1)
			return -1;
	}
	while (code < LR_OK);

	if (code == LR_OK)
		return 0;

	errno = EINVAL;
	return -1;
}


/*
** ph_status() - get status info.
**	0				success
**	-1				error (sets errno)
*/
int
ph_status(PH *ph, char **motd)
{
	int code;
	size_t buflen = 0;
	char buf[PH_BUF_SIZE];
	void *ptr;

	*motd = NULL;

	if (ph_send_command(ph, "status") == -1)
		return -1;

	while (ph_get_response(ph, &code, buf, sizeof(buf)) == 0)
	{
		if (code != LR_PROGRESS && code < LR_OK && code > 0)
			continue;
		if (code == LR_OK)
			return 0;
		if (code != LR_PROGRESS)
		{
			errno = EINVAL;
			return -1;
		}

		*motd = (char *)((ptr = realloc(*motd,
						buflen + strlen(buf) + 2))
				 ? ptr
				 : *motd);
		if (ptr == NULL)
			return -1;
		buflen += sprintf(*motd + buflen, "%s\n", buf);
	}

	return -1;
}


/*
** ph_external() - send "external" command.
**	0				success
**	-1				error (sets errno)
*/
int
ph_external(PH *ph)
{
	int code;

	if (ph_send_command(ph, "external") == -1)
		return -1;
	do
	{
		if (ph_get_response(ph, &code, NULL, 0) == -1)
			return -1;
	}
	while (code < LR_OK);

	if (code != LR_OK)
	{
		errno = EINVAL;
		return -1;
	}

	return 0;
}


