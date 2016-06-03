/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  protocol.c - libphclient protocol code
**
**  Mark D. Roth <roth@feep.net>
*/

#include <internal.h>

#include <errno.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
# include <stdarg.h>
#else
# ifdef HAVE_VARARGS_H
#  include <varargs.h>
# endif
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#else
# ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
# endif
#endif


/*
** ph_set_sendhook() - set the send hook function.
*/
void
ph_set_sendhook(PH *ph, ph_debug_func_t sendfunc)
{
	ph->ph_sendhook = sendfunc;
}

/*
** ph_set_recvhook() - set the receive hook function.
*/
void
ph_set_recvhook(PH *ph, ph_debug_func_t recvfunc)
{
	ph->ph_recvhook = recvfunc;
}

/*
** ph_set_hookdata() - set the hook data to be passed to the debug hooks
*/
void
ph_set_hookdata(PH *ph, void *hook_data)
{
	ph->ph_hook_data = hook_data;
}


/* buffer plugin to read from server */
ssize_t
ph_buffer_read(void *app_data, char *buf, size_t buflen)
{
	PH *ph = (PH *)app_data;

	return read(ph->ph_rfd[0], buf, buflen);
}


/* ph_get_response() - read and parse a response from the server. */
int
ph_get_response(PH *ph, int *code, char *buf, size_t bufsize)
{
	char linebuf[PH_BUF_SIZE];
	char *linep, *fieldp;
	ssize_t sz;

	if (ph_buffer_read_line(ph->ph_buf, linebuf, sizeof(linebuf)) == -1)
		return -1;

	/* call receive hook, if set */
	if (ph->ph_recvhook != NULL)
		(*(ph->ph_recvhook))(ph->ph_hook_data, linebuf);

	/* blank line */
	if (linebuf[0] == '\0')
	{
		errno = EINVAL;
		return -1;
	}

	/* extract response code */
	linep = linebuf;
	fieldp = strsep(&linep, ":");
	if (sscanf(fieldp, "%d", code) != 1)
	{
		errno = EINVAL;
		return -1;
	}

	/* response code with no text */
	if (linep == NULL)
	{
		if (buf != NULL)
			buf[0] = '\0';
		errno = EINVAL;
		return -1;
	}

	/* valid line, copy it to caller-supplied buffer */
	if (buf != NULL)
		strlcpy(buf, linep, bufsize);

	return 0;
}


/*
** ph_parse_response() - utility function to split up ':'-delimited
** sections of the text returned by ph_get_response()
*/
int
ph_parse_response(char *buf, int *subcode, char **field1, char **field2)
{
	/* extract entry number */
	if (subcode != NULL)
	{
		if (sscanf(buf, "%d:", subcode) != 1)
		{
			errno = EINVAL;
			return -1;
		}
		strsep(&buf, ":");
	}

	/* extract fields */
	if (field1 != NULL)
	{
		*field1 = strsep(&buf, ":");
		if (*field1 == NULL)
		{
			errno = EINVAL;
			return -1;
		}
	}
	if (field2 != NULL)
	{
		*field2 = buf;
		if (*field2 == NULL)
		{
			errno = EINVAL;
			return -1;
		}
	}

	return 0;
}


/* ph_send_command() - send a command to the server */
ssize_t
#ifdef STDC_HEADERS
ph_send_command(PH *ph, char *fmt, ...)
#else
ph_send_command(PH *ph, char *fmt, int va_alist)
#endif
{
	va_list args;
	char buf[PH_BUF_SIZE];
	int fd;
#ifdef HAVE_POLL
	struct pollfd pfd[1];
#else /* ! HAVE_POLL */
	fd_set wfds, efds;
	int max;
	struct timeval tm;
#endif /* HAVE_POLL */

	fd = ph_wfd(ph);

#ifdef HAVE_POLL
	memset(pfd, 0, sizeof(pfd));
	pfd[0].events = POLLOUT;
	pfd[0].fd = fd;
	if (poll(pfd, 1, 0) == -1)
		return -1;
	if (! BIT_ISSET(pfd[0].revents, POLLOUT))
	{
		errno = EPIPE;
		return -1;
	}
#else /* ! HAVE_POLL */
	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	FD_SET(fd, &wfds);
	FD_SET(fd, &efds);
	memset(&tm, 0, sizeof(tm));
	if (select(fd + 1, NULL, &wfds, &efds, &tm) == -1)
		return -1;
	if (! FD_ISSET(fd, &wfds) || FD_ISSET(fd, &efds))
	{
		errno = EPIPE;
		return -1;
	}
#endif /* HAVE_POLL */

#ifdef STDC_HEADERS
	va_start(args, fmt);
#else
	va_start(args);
#endif
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

#ifdef DEBUG
	printf("ph_send_command(): sending \"%s\"\n", buf);
#endif

	/* execute the send hook, if set */
	if (ph->ph_sendhook != NULL)
		(*(ph->ph_sendhook))(ph->ph_hook_data, buf);

	strlcat(buf, "\n", sizeof(buf));
	return write(fd, buf, strlen(buf) + 1);
}


/*
** ph_quote_value() - convert a literal field value to a server-parsable
** quoted string.  returns number of characters written.
*/
static size_t
ph_quote_value(char *value, char *buf, size_t bufsize)
{
	char tmpbuf[PH_BUF_SIZE];
	char *sp, *p, *q;
	char c;

	/*
	** FIXME: need to check return value from strlcpy() and strlcat()
	*/

	strlcpy(tmpbuf, value, sizeof(tmpbuf));
	strlcat(buf, "\"", bufsize);

	for (sp = tmpbuf, p = strpbrk(sp, "\n\t\"\\");
	     p != NULL;
	     sp = p + 1, p = strpbrk(sp, "\n\t\"\\"))
	{
		c = *p;
		*p = '\0';
		strlcat(buf, sp, bufsize);

		switch (c)
		{
		case '\n':
			q = "\\n";
			break;
		case '\t':
			q = "\\t";
			break;
		case '\\':
			q = "\\\\";
			break;
		case '"':
			if (p == value || *(p - 1) != '\\')
				q = "\\\"";
			else
				q = "\"";
			break;
		default:
			q = "";
			break;
		}
		if (strlen(q) != 0)
			strlcat(buf, q, bufsize);
	}
	strlcat(buf, sp, bufsize);

	return strlcat(buf, "\"", bufsize);
}


/*
** ph_dequote_value() - convert a server-parsable quoted value to a literal
** field value.  returns number of characters written.
*/
size_t
ph_dequote_value(char *value, char *buf, size_t bufsize)
{
	size_t sz;
	char *sp, *p;

#ifdef DEBUG
	printf("==> ph_dequote_value(\"%s\")\n", value);
#endif

	sz = strlcpy(buf, value, bufsize);

	/* first, eliminate the non-escaped quotes */
	for (sp = buf; (p = strchr(sp, '"')) != NULL; sp = p)
		if (p == buf || *(p - 1) != '\\')
		{
			memmove(p, p + 1, strlen(p + 1) + 1);
			sz--;
		}
		else
			p++;

	/* change ``\\"'' to "\\\"" and ``\"'' to "\"" */
	for (sp = buf; (p = strstr(sp, "\\\"")) != NULL; sp = p)
	{
		if (p == buf || *(p - 1) == '\\')
			memmove(p - 1, p, strlen(p) + 1);
		else
		{
			*p = '"';
			memmove(p + 1, p + 2, strlen(p + 2) + 1);
		}
		sz--;
	}

	/* change ``\\n'' to "\\n" and ``\n'' to "\n" */
	for (sp = buf; (p = strstr(sp, "\\n")) != NULL; sp = p)
	{
		if (p == buf || *(p - 1) == '\\')
			memmove(p - 1, p, strlen(p) + 1);
		else
		{
			*p = '\n';
			memmove(p + 1, p + 2, strlen(p + 2) + 1);
		}
		sz--;
	}

	/* change ``\\t'' to "\\t" and ``\t'' to "\t" */
	for (sp = buf; (p = strstr(sp, "\\t")) != NULL; sp = p)
	{
		if (p == buf || *(p - 1) == '\\')
			memmove(p - 1, p, strlen(p) + 1);
		else
		{
			*p = '\t';
			memmove(p + 1, p + 2, strlen(p + 2) + 1);
		}
		sz--;
	}

	/* change ``\\'' to "\\" and ``\'' to "" */
	for (sp = buf; (p = strstr(sp, "\\\\")) != NULL; sp = p)
	{
		if (p == buf || *(p - 1) == '\\')
			memmove(p - 1, p, strlen(p) + 1);
		else
		{
			*p = '"';
			memmove(p + 1, p + 2, strlen(p + 2) + 1);
		}
		sz--;
	}

	return sz;
}


/*
** ph_decode_selectors() - decode a ph_fieldselector array into a string
** to send to the server.  returns number of characters written.
*/
size_t
ph_decode_selectors(struct ph_fieldselector selectors[],
		    char *buf, size_t bufsize)
{
	int i;
	size_t buflen = 0;

	for (i = 0; selectors[i].pfs_value != NULL; i++)
	{
		buflen = strlcat(buf, " ", bufsize);
		if (selectors[i].pfs_field != NULL
		    && selectors[i].pfs_field[0] != '\0')
			buflen += snprintf(buf + buflen, bufsize - buflen,
					   "%s%c",
					   selectors[i].pfs_field,
					   selectors[i].pfs_operation);
		buflen += ph_quote_value(selectors[i].pfs_value, buf + buflen,
					 bufsize - buflen);
	}

	return buflen;
}


/*
** ph_encode_selector() - parse the selector string and add an entry to
** the selectors array.
** returns:
**	0				success
**	-1				parse error
*/
int
ph_encode_selector(char *string, int requirefield, int *lastindex,
		   struct ph_fieldselector **selectors)
{
	char *p;
	char buf[PH_BUF_SIZE], valbuf[PH_BUF_SIZE];
	char op;
	void *ptr;

#ifdef DEBUG
	printf("==> ph_encode_selector(\"%s\")\n", string);
#endif

	strlcpy(buf, string, sizeof(buf));

	/* check for an operator */
	p = strpbrk(buf, "=><~");
	if (p == NULL
	    && requirefield)
	{
		errno = EINVAL;
		return -1;
	}

	/* allocate memory */
	ptr = realloc(*selectors,
		      (*lastindex + 2) * sizeof(struct ph_fieldselector));
	if (ptr == NULL)
		return -1;
	*selectors = (struct ph_fieldselector *)ptr;
	memset(&((*selectors)[*lastindex]), 0,
	       2 * sizeof(struct ph_fieldselector));

	if (p != NULL)
	{
		/* encode field and operator */
		op = *p;
		*p++ = '\0';
		(*selectors)[*lastindex].pfs_field = strdup(buf);
		if ((*selectors)[*lastindex].pfs_field == NULL)
			return -1;
		(*selectors)[*lastindex].pfs_operation = op;
	}
	else
		/* set p to point to value */
		p = buf;

	/* encode value */
	ph_dequote_value(p, valbuf, sizeof(valbuf));
	(*selectors)[*lastindex].pfs_value = strdup(valbuf);
	if ((*selectors)[*lastindex].pfs_value == NULL)
		return -1;

#ifdef DEBUG
	printf("field=\"%s\", value=\"%s\"\n",
	       (*selectors)[*lastindex].pfs_field
	       ? (*selectors)[*lastindex].pfs_field
	       : "NULL",
	       (*selectors)[*lastindex].pfs_value
	       ? (*selectors)[*lastindex].pfs_value
	       : "NULL");
#endif

	/* increment last index */
	(*lastindex)++;

	return 0;
}


/*
** ph_free_selectors() - free memory associated with an array of selectors
*/
void
ph_free_selectors(struct ph_fieldselector *selectors)
{
	int i;

	if (selectors == NULL)
		return;

	for (i = 0; selectors[i].pfs_value != NULL; i++)
	{
		if (selectors[i].pfs_field != NULL)
			free(selectors[i].pfs_field);
		if (selectors[i].pfs_value != NULL)
			free(selectors[i].pfs_value);
	}

	free(selectors);
}


