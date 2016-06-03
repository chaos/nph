/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  query.c - libphclient query code
**
**  Mark D. Roth <roth@feep.net>
*/

#include <internal.h>

#include <errno.h>
#include <ctype.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif


/*
** ph_query() - send a query to the PH server and return matching entries.
** returns:
**	number of entries returned	success
**	-1				error (sets errno)
**	PH_ERR_TOOMANY			too many entries matched
**	PH_ERR_NOMATCH			no matching entries
**	PH_ERR_DATAERR			invalid query
*/
int
ph_query(PH *ph, struct ph_fieldselector query[], char *retfields[],
	 ph_entry **entries)
{
	int i;
	size_t sz, buflen = 0;
	char buf[PH_BUF_SIZE];
	int code, entrynum;
	int numresponses = 0, numfields = 0;
	char *field, *value;
	void *ptr;
	ph_entry entry;

#ifdef DEBUG
	puts("==> ph_query()");
	for (i = 0; query[i].pfs_field; i++)
		printf("   query[%d]: %s%c%s\n",
		       i, query[i].pfs_field, query[i].pfs_operation,
		       query[i].pfs_value);
#endif

	/* important initialization */
	*entries = NULL;

	/* construct the query command */
	buflen = sprintf(buf, "query");
	buflen += ph_decode_selectors(query, buf + buflen,
				      sizeof(buf) - buflen);
	if (retfields != NULL)
	{
		buflen = strlcat(buf, " return", sizeof(buf));
		for (i = 0; retfields[i] != NULL; i++)
			buflen += snprintf(buf + buflen, sizeof(buf) - buflen,
					   " %s", retfields[i]);
	}

	/* send the query command */
#ifdef DEBUG
	printf("ph_query(): sending query: \"%s\"\n", buf);
#endif
	if (ph_send_command(ph, "%s", buf) == -1)
		return -1;

	/* read the server response */
	while (ph_get_response(ph, &code, buf, sizeof(buf)) == 0)
	{
#ifdef DEBUG
		printf("top of ph_get_response() loop: code=%d, buf=\"%s\"\n",
		       code, buf);
#endif

		/* check response code */
		if ((code < LR_OK && code > 99)
		    || (code < 0 && !isdigit((int)buf[0])))
			continue;
		if (code == LR_OK || code == LR_NOMATCH)
			return (numresponses > 0
				? numresponses
				: PH_ERR_NOMATCH);
		if (code == LR_TOOMANY)
			return PH_ERR_TOOMANY;
		if (code > 0)
			return PH_ERR_DATAERR;

		/* parse the response text */
		if (ph_parse_response(buf, &entrynum, &field, &value) == -1)
			return -1;

		sz = strspn(field, " ");

		/* first field of a new entry */
		if (entrynum > numresponses)
		{
			/*
			** field name can't be all spaces on first line
			** of a new entry
			*/
			if (sz == strlen(field))
			{
				errno = EINVAL;
				return -1;
			}

			ptr = realloc(*entries,
				      (entrynum + 1) * sizeof(ph_entry));
			if (ptr == NULL)
				return -1;
			*entries = (ph_entry *)ptr;
			memset(&((*entries)[entrynum - 1]), 0,
			       (entrynum - numresponses + 1) * sizeof(ph_entry));
			numresponses = entrynum;
			numfields = 0;
		}

		value += strspn(value, " ");

		/*
		** if field name is all spaces, value should be appended
		** to last field
		*/
		if (sz == strlen(field))
		{
#ifdef DEBUG
			printf("old value: (length = %d) \"%s\"\n",
			       strlen(entry[numfields - 1].pfv_value),
			       entry[numfields - 1].pfv_value);
			printf("new value: (length = %d) \"%s\"\n",
			       strlen(value), value);
#endif
			sz = strlen(entry[numfields - 1].pfv_value) + strlen(value) + 2;
			ptr = realloc(entry[numfields - 1].pfv_value, sz);
			if (ptr == NULL)
				return -1;
			entry[numfields - 1].pfv_value = (char *)ptr;
			strlcat(entry[numfields - 1].pfv_value, "\n", sz);
			if (value[0] != '\0')
				strlcat(entry[numfields - 1].pfv_value,
					value, sz);
#ifdef DEBUG
			printf("combined value: (length = %d) \"%s\"\n",
			       strlen(entry[numfields - 1].pfv_value),
			       entry[numfields - 1].pfv_value);
#endif
		}
		else
		{
			/* new field */
			ptr = realloc((*entries)[entrynum - 1],
				      (numfields + 2) * sizeof(struct ph_fieldvalue));
			if (ptr == NULL)
				return -1;
			(*entries)[entrynum - 1] = (struct ph_fieldvalue *)ptr;
			entry = (*entries)[entrynum - 1];
			memset(entry + numfields, 0,
			       2 * sizeof(struct ph_fieldvalue));
			entry[numfields].pfv_field = strdup(field + sz);
			entry[numfields].pfv_value = strdup(value);
			if (entry[numfields].pfv_field == NULL
			    || entry[numfields].pfv_value == NULL)
				return -1;
			entry[numfields].pfv_code = code;
			numfields++;
		}
	}

#ifdef DEBUG
	puts("ph_query(): reached end of ph_get_response() loop!");
#endif
	return -1;
}


/*
** ph_free_entries() - free memory returned by ph_query().
*/
void
ph_free_entries(ph_entry *entries)
{
	int i, j;

	if (entries == NULL)
		return;

	for (i = 0; entries[i] != NULL; i++)
	{
		for (j = 0; entries[i][j].pfv_field != NULL; j++)
		{
			free(entries[i][j].pfv_field);
			free(entries[i][j].pfv_value);
		}
		free(entries[i]);
	}
	free(entries);
}


