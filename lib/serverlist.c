/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  serverlist.c - code to handle references to other PH servers
**
**  Mark D. Roth <roth@feep.net>
*/

#include <internal.h>

#include <errno.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif

#include <strings.h>


/*
** ph_serverlist_add() - add an entry to a list of known PH servers.
** returns:
**	0				success
**	-1				error (sets errno)
*/
int
ph_serverlist_add(struct ph_serverlist *serverlist, char *server, char *site)
{
	void *ptr;

#ifdef DEBUG
	printf("in ph_serverlist_add([serverlist], \"%s\", \"%s\")\n", server,
	       site);
#endif

	ptr = realloc(serverlist->sl_list,
		      (serverlist->sl_numservers + 2) * sizeof(struct ph_serversite));
	if (ptr == NULL)
		return -1;
	serverlist->sl_list = (struct ph_serversite *)ptr;

	memset(&(serverlist->sl_list[serverlist->sl_numservers + 1]),
	       0, sizeof(struct ph_serversite));
	serverlist->sl_list[serverlist->sl_numservers].ss_server = strdup(server);
	serverlist->sl_list[serverlist->sl_numservers].ss_site = strdup(site);
	serverlist->sl_numservers++;

	return 0;
}


/*
** ph_serverlist_iterate() - iterate through all matching PH server entries.
** returns:
**	1                               data returned
**	0				no more data
**	-1				error (sets errno)
*/
int
ph_serverlist_iterate(struct ph_serverlist *serverlist,
		      char *server, char *site,
		      struct ph_serversite **serversite)
{
	char *p;

#ifdef DEBUG
	printf("in ph_serverlist_iterate([serverlist], \"%s\", \"%s\", "
	       "0x%lx)\n",
	       (server ? server : "NULL"),
	       (site ? site : "NULL"),
	       *serversite);
#endif

	if (server != NULL)
	{
		/* make sure it's at least two levels */
		p = strchr(server, '.');
		if (p == NULL || p == server || *(p + 1) == '\0')
		{
			errno = EINVAL;
			return -1;
		}

		/* strip trailing '.' */
		if (server[strlen(server) - 1] == '.')
			server[strlen(server) - 1] = '\0';
	}

	if (*serversite == NULL)
		*serversite = serverlist->sl_list;
	else
		(*serversite)++;

	for (; (*serversite)->ss_site != NULL; (*serversite)++)
	{
#ifdef DEBUG
		printf("ph_serverlist_iterate(): checking server=\"%s\" "
		       "site=\"%s\"\n",
		       (*serversite)->ss_server, (*serversite)->ss_site);
#endif

		/* match server name only on final set of domain components */
		if (server != NULL)
		{
			p = (*serversite)->ss_server + strlen((*serversite)->ss_server) - strlen(server);
			if (p < (*serversite)->ss_server
			    || strcasecmp(p, server) != 0
			    || (p != (*serversite)->ss_server
				&& *(p - 1) != '.'))
				continue;
		}

		/* match site on substring match */
		if (site != NULL
		    && strstr((*serversite)->ss_site, site) == NULL)
			continue;

		/* found a match */
		return 1;
	}

	return 0;
}


/*
** ph_serverlist_merge() - read list of known PH servers from the server
** associated with ph and merge them into serverlist.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_DATAERR			invalid query
*/
int
ph_serverlist_merge(PH *ph, struct ph_serverlist *serverlist)
{
	char *site;
	char *linep, *nextp, *typep;
	int i, save_errno, numentries;
	ph_memblock_t *blockp;
	struct ph_fieldselector query[] = {
		{ "name",	'=',	"ns-servers" },
		{ "type",	'=',	"serverlist" },
		{ NULL,		'\0',	NULL }
	};
	char *getfields[] = { "text", NULL };
	ph_entry *entries;

	/* get list of known servers */
	if (ph_mmgr_malloc(ph->ph_mmgr, 0, 0, 0,
			   (ph_free_func_t)ph_free_entries,
			   &blockp) == -1)
		return -1;
	numentries = ph_query(ph, query, getfields,
			      (ph_entry **)&(ph_mmgr_ptr(blockp)));
	if (numentries == PH_ERR_NOMATCH)
	{
		ph_mmgr_free(blockp, 1);
		return 0;
	}
	if (numentries <= 0)
	{
		save_errno = errno;
		ph_mmgr_free(blockp, 1);
		errno = save_errno;
		return numentries;
	}

	entries = (ph_entry *)ph_mmgr_ptr(blockp);

	/* read the responses */
	for (i = 0; i < numentries; i++)
	{
		site = NULL;

		/* skip entries which don't contain valid data */
		if (entries[i][0].pfv_code != -(LR_OK))
			continue;

		/* parse each line */
		nextp = entries[i][0].pfv_value;
		while ((linep = strsep(&nextp, "\n")) != NULL)
		{
			/* format is "(site|server):data" */
			typep = strsep(&linep, ":");
			if (typep == NULL || *typep == '\0')
				continue;

			/*
			** "site" line comes first,
			** so save pointer and read next line
			*/
			if (strcasecmp(typep, "site") == 0)
			{
				site = linep;
				continue;
			}

			/* for "server" line, create a new element */
			if (strcasecmp(typep, "server") == 0)
			{
				/*
				** if we get a "server" line without a
				** "site" line, skip it
				*/
				if (site == NULL)
					continue;

				if (ph_serverlist_add(serverlist, linep,
						      site) == -1)
				{
					save_errno = errno;
					ph_mmgr_free(blockp, 1);
					errno = save_errno;
					return -1;
				}
			}
		}
	}

	ph_mmgr_free(blockp, 1);
	return 0;
}


/*
** ph_free_serverlist() - free memory associated with serverlist.
*/
void
ph_free_serverlist(struct ph_serverlist *serverlist)
{
	int i;

	for (i = 0; serverlist->sl_list[i].ss_server != NULL; i++)
	{
		free(serverlist->sl_list[i].ss_server);
		free(serverlist->sl_list[i].ss_site);
	}

	free(serverlist->sl_list);
	serverlist->sl_list = NULL;
	serverlist->sl_numservers = 0;
}


