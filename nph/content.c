/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  content.c - nph code to access server data
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <errno.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif

#include <strings.h>


/* find www field for a given alias */
int
nph_resolvewww(int cmdc, char **cmdv)
{
	int i;
	char *p;

	if (cmdv[1] == NULL
	    || cmdv[2] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	i = ph_www_resolve(ph, cmdv[1], &p);
	if (i == -1)
		nph_printf(1, "ph_www_resolve(): %s\n", strerror(errno));
	else if (i == PH_ERR_NOMATCH)
		nph_printf(1, "user unknown\n");
	else if (i == PH_ERR_DATAERR)
		nph_printf(1, "invalid query or no \"www\" field\n");
	else
	{
		nph_printf(0, "resolved URL for user %s is \"%s\"\n",
			   cmdv[1], p);
		free(p);
	}

	return i;
}


/* determine advertised URL for a given alias */
int
nph_publicwww(int cmdc, char **cmdv)
{
	int i;
	char *p;

	if (cmdv[1] == NULL
	    || cmdv[2] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	i = ph_advertised_www(ph, cmdv[1], 1, &p);
	if (i == -1)
		nph_printf(1, "ph_advertised_www(): %s\n", strerror(errno));
	else if (i == PH_ERR_NOMATCH)
		nph_printf(1, "alias not found\n");
	else if (i == PH_ERR_DATAERR)
		nph_printf(1, "invalid query or no \"www\" field\n");
	else
	{
		nph_printf(0, "advertised URL for alias %s is \"%s\"\n",
			   cmdv[1], p);
		free(p);
	}

	return i;
}


/* resolve user portion of email address */
int
nph_resolvemail(int cmdc, char **cmdv)
{
	int i;
	char *cp, *keyfields = NULL, *alias;

	switch (cmdc)
	{
	case 2:
		alias = cmdv[1];
		break;

	case 3:
		if (strncmp(cmdv[1], "keyfields=", 10) == 0)
		{
			keyfields = cmdv[1] + 10;
			alias = cmdv[2];
			break;
		}
		/* FALLTHROUGH */

	default:
		print_help(cmdv[0]);
		return -1;
	}

	i = ph_email_resolve(ph, alias, keyfields, &cp);
	if (i == -1)
		nph_printf(1, "ph_email_resolve(): %s\n", strerror(errno));
	else if (i == PH_ERR_NOMATCH)
		nph_printf(1, "user unknown\n");
	else if (i == PH_ERR_DATAERR)
		nph_printf(1, "invalid query or no \"email\" field\n");
	else
	{
		nph_printf(0, "resolved email address for user %s is \"%s\"\n",
			   alias, cp);
		free(cp);
	}

	return i;
}


/* determine advertised email address for a given alias */
int
nph_publicemail(int cmdc, char **cmdv)
{
	int i;
	char *p;

	if (cmdv[1] == NULL
	    || cmdv[2] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	i = ph_advertised_email(ph, cmdv[1], 1, &p);
	if (i == -1)
		nph_printf(1, "ph_advertised_email(): %s\n", strerror(errno));
	else if (i == PH_ERR_NOMATCH)
		nph_printf(1, "alias not found\n");
	else if (i == PH_ERR_DATAERR)
		nph_printf(1, "invalid query or no \"email\" field\n");
	else
	{
		nph_printf(0, "advertised email address for alias "
			   "%s is \"%s\"\n", cmdv[1], p);
		free(p);
	}

	return i;
}


/*
** parse field selectors.
** if fieldflag is set, requires "field=" portion to be present.
** starts from cmdv[*cmdi] and increments *cmdi as it goes.
** returns:
**	-1		parse error
**	-2		all input consumed
**	0 or greater	index into stop[] which matched the current token
*/
static int
nph_parse_selectors(char **cmdv, int *cmdi,
		    const char *stop[], int requirefield,
		    struct ph_fieldselector **selectors)
{
	int i, numfields = 0;
	char *default_field, *selector;
	char buf[NPH_BUF_SIZE];

#ifdef DEBUG
	puts("==> nph_parse_selectors()");
#endif

	get_option("defaultfield", &default_field);

	for (; cmdv[*cmdi] != NULL; (*cmdi)++)
	{
#ifdef DEBUG
		printf("nph_parse_selectors(): cmdv[%d] = \"%s\"\n", *cmdi,
		       cmdv[*cmdi]);
#endif

		/*
		** if stop is set, check each token to see if
		** we should stop here
		*/
		if (stop != NULL && (*selectors) != NULL)
			for (i = 0; stop[i] != NULL; i++)
				if (strcasecmp(cmdv[*cmdi], stop[i]) == 0)
					return i;

		if (strchr(cmdv[*cmdi], '=') == NULL
		    && default_field != NULL
		    && *default_field != '\0')
		{
			snprintf(buf, sizeof(buf), "%s=%s", default_field,
				 cmdv[*cmdi]);
			selector = buf;
		}
		else
			selector = cmdv[*cmdi];

		if (ph_encode_selector(selector, requirefield,
				       &numfields, selectors) == -1)
		{
			nph_printf(1, "parse error in field selectors\n");
			return -1;
		}

#ifdef DEBUG
		printf("field=\"%s\", value=\"%s\"\n",
		       (*selectors)[numfields - 1].pfs_field
		       ? (*selectors)[numfields - 1].pfs_field
		       : "NULL",
		       (*selectors)[numfields - 1].pfs_value
		       ? (*selectors)[numfields - 1].pfs_value
		       : "NULL");
#endif
	}

	return -2;
}


#if 0
/* select entries for further processing */
int
nph_select(int cmdc, char **cmdv)
{
	int i, cmdi = 1;
	struct ph_fieldselector *selector;
	char *retfields = { "alias", NULL };
	ph_entry *entries;

	if (cmdv[1] == NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	i = nph_parse_selectors(cmdv, &cmdi, NULL, 0, &selector);
	if (i != -2)
	{
		nph_printf(1, "parse error\n");
		return -1;
	}

	i = ph_query(ph, selector, retfields, &entries);
	ph_free_entries(entries);

	if (i > 0)
	{
		ph_free_selectors(default_selectors);
		default_selectors = selectors;
		nph_printf(0, "%d entries selected\n", i);
		return 0;
	}

	ph_free_selectors(selector);
	nph_printf(1, "ph_query(): %s\n", strerror(errno));
	return -1;
}
#endif


/* change an entry */
int
nph_change(int cmdc, char **cmdv)
{
	int i, cmdi = 1, changeflag = 0;
	const char *stoptokens[] = { "make", "force", NULL };
	struct ph_fieldselector *selectors = NULL, *changes = NULL;
	char *user;

	if (strcasecmp(cmdv[0], "make") == 0)
	{
		user = ph_whoami(ph);
		if (user == NULL)
		{
			nph_printf(1, "not logged in\n");
			print_help(cmdv[0]);
			return -1;
		}

		selectors = (struct ph_fieldselector *)calloc(2, sizeof(struct ph_fieldselector));
		selectors[0].pfs_field = strdup("alias");
		selectors[0].pfs_value = strdup(user);
		selectors[0].pfs_operation = '=';
	}
	else
	{
		/* parse field selectors */
		i = nph_parse_selectors(cmdv, &cmdi, stoptokens, 0,
					&selectors);
		if (i == -2)
		{
			nph_printf(1, "%s: no %s specified\n", cmdv[0],
				   (cmdi == 1 ? "selectors" : "changes"));
			print_help(cmdv[0]);
			if (selectors != NULL)
				ph_free_selectors(selectors);
			return -1;
		}

		/* set force flag if it's a "force" command */
		if (i == 1)
			changeflag |= PH_CHANGE_FORCE;
		cmdi++;

	}

	/* parse changes */
	i = nph_parse_selectors(cmdv, &cmdi, NULL, 1, &changes);
	if (i != -2)
	{
		nph_printf(1, "%s: invalid change: \"%s\"\n", cmdv[0],
			   cmdv[cmdi]);
		print_help(cmdv[0]);
		ph_free_selectors(selectors);
		if (changes != NULL)
			ph_free_selectors(changes);
		return -1;
	}

	/* send change command */
	i = ph_change(ph, selectors, changes, changeflag);
	switch (i)
	{
	case -1:
		nph_printf(1, "ph_change(): %s\n", strerror(errno));
		break;

	case PH_ERR_NOTLOG:
		nph_printf(1, "not logged in\n");
		break;

	case PH_ERR_READONLY:
		nph_printf(1, "server is in read-only mode\n");
		break;

	case PH_ERR_DATAERR:
		nph_printf(1, "illegal value specified\n");
		break;

	default:
		nph_printf(0, "%d entries changed\n", i);
	}

	/* clean up */
	ph_free_selectors(selectors);
	ph_free_selectors(changes);
	return i;
}


/* delete entries */
int
nph_delete(int cmdc, char **cmdv)
{
	int i, cmdi = 1;
	struct ph_fieldselector *selectors = NULL;

	/* parse field selectors */
	if (nph_parse_selectors(cmdv, &cmdi, NULL, 0, &selectors) != -2)
	{
		nph_printf(1, "%s: invalid argument: \"%s\"\n", cmdv[0],
			   cmdv[cmdi]);
		print_help(cmdv[0]);
		if (selectors != NULL)
			ph_free_selectors(selectors);
		return -1;
	}

	/* send delete command */
	i = ph_delete(ph, selectors);
	switch (i)
	{
	case -1:
		nph_printf(1, "ph_delete(): %s\n", strerror(errno));
		break;

	case PH_ERR_NOTLOG:
		nph_printf(1, "not logged in\n");
		break;

	case PH_ERR_READONLY:
		nph_printf(1, "server is in read-only mode\n");
		break;

	default:
		nph_printf(0, "deleted %d entries\n", i);
	}

	/* clean up */
	ph_free_selectors(selectors);
	return i;
}


/* add an entry */
int
nph_add(int cmdc, char **cmdv)
{
	int i, cmdi = 1;
	struct ph_fieldselector *fields = NULL;

	/* parse field data */
	if (nph_parse_selectors(cmdv, &cmdi, NULL, 1, &fields) != -2)
	{
		nph_printf(1, "%s: invalid argument: \"%s\"\n", cmdv[0],
			   cmdv[cmdi]);
		print_help(cmdv[0]);
		if (fields != NULL)
			ph_free_selectors(fields);
		return -1;
	}

	/* send add command */
	i = ph_add(ph, fields);
	switch (i)
	{
	case -1:
		nph_printf(1, "ph_add(): %s\n", strerror(errno));
		break;

	case PH_ERR_NOTLOG:
		nph_printf(1, "not logged in\n");
		break;

	case PH_ERR_READONLY:
		nph_printf(1, "server is in read-only mode\n");
		break;

	case PH_ERR_DATAERR:
		nph_printf(1, "invalid field data\n");
		break;

	default:
		nph_printf(0, "add successful\n");
	}

	/* clean up */
	ph_free_selectors(fields);
	return i;
}


/* retrieve data from server */
int
nph_query(int cmdc, char **cmdv)
{
	char *altdomain = NULL, *altserver = NULL;
	struct ph_serversite *serversite = NULL;
	PH *altph = NULL;
	int usealtph = 0;
	int i, j = 0, cmdi = 1;
	struct ph_fieldselector *selectors = NULL;
	ph_entry *entries;
	const char *stoppers[] = { "return", NULL };
	char **retfields = NULL, **realretfields = NULL, *default_fields;
	int redirect_flag = 0;
	char buf[NPH_BUF_SIZE];
	char *wordp, *nextp;

#ifdef DEBUG
	printf("==> nph_query(cmdc=%d, cmdv[]={\"%s\"", cmdc, cmdv[0]);
	for (i = 1; i < cmdc; i++)
		printf(",\"%s\"", cmdv[i]);
	printf("})\n");
#endif

	if (cmdc < 2)
	{
		nph_printf(1, "%s: required argument missing\n", cmdv[0]);
		print_help(cmdv[0]);
		return -1;
	}

	/*
	** if "@domain" is specified, remove it from the argument list
	** and save
	*/
	if (cmdv[cmdc - 1][0] == '@')
	{
		altdomain = cmdv[cmdc - 1] + 1;
		cmdv[cmdc - 1] = NULL;
	}

	/* parse field data */
	i = nph_parse_selectors(cmdv, &cmdi, stoppers, 0, &selectors);
	if (i == -1)
	{
		nph_printf(1, "parse error in query command\n");
		return -1;
	}

	/* if the return fields have been specified, parse them */
	if (i == 0)
		for (j = 0, cmdi++; cmdv[cmdi]; cmdi++, j++)
		{
			retfields = (char **)realloc(retfields,
						     (j + 2) * sizeof(char *));
			retfields[j] = cmdv[cmdi];
			retfields[j + 1] = NULL;
		}
	else
	{
		get_option("returnfields", &default_fields);
		if (default_fields != NULL)
		{
			snprintf(buf, sizeof(buf), "%s", default_fields);
			nextp = buf;
			j = 0;
			while ((wordp = strsep(&nextp, " ")) != NULL)
			{
				if (*wordp == '\0')
					continue;

				retfields = (char **)realloc(retfields,
							     (j + 2) * sizeof(char *));
				retfields[j] = wordp;
				retfields[++j] = NULL;
			}
		}
	}
	get_option("canonicaladdrs", &redirect_flag);
	if (redirect_flag
	    && !field_match("alias", retfields))
	{
		realretfields = (char **)malloc((j + 2) * sizeof(char *));
		memcpy(realretfields, retfields, j * sizeof(char *));
		realretfields[j] = "alias";
		realretfields[j + 1] = NULL;
	}
	else
		realretfields = retfields;

#ifdef DEBUG
	if (retfields != NULL)
		for (i = 0; retfields[i] != NULL; i++)
			printf("retfields[%d] = \"%s\"\n", i, retfields[i]);
	if (realretfields != NULL)
		for (i = 0; realretfields[i] != NULL; i++)
			printf("realretfields[%d] = \"%s\"\n", i,
			       realretfields[i]);
#endif

	/*
	** if "@domain" was specified,
	** try to find the PH server for that domain
	*/
	if (altdomain != NULL)
	{
		if (!got_servers
		    && ph_serverlist_merge(ph, &serverlist) == -1)
		{
			nph_printf(1, "%s: ph_serverlist_merge(): %s\n",
				   cmdv[0], strerror(errno));
			return -1;
		}
		got_servers = 1;

		i = ph_serverlist_iterate(&serverlist, altdomain, NULL,
					  &serversite);
		if (i == 0)
		{
			nph_printf(1,
				   "%s: no known PH server for domain \"%s\"\n",
				   cmdv[0], altdomain);
			return -1;
		}
		if (i == -1)
		{
			nph_printf(1, "%s: ph_serverlist_iterate(): %s\n",
				   cmdv[0], strerror(errno));
			return -1;
		}

		/*
		** make sure the match isn't just a pointer to the
		** current server
		*/
		get_option("server", &wordp);
		if (strcasecmp(wordp, serversite->ss_server) != 0)
		{
			/* found a match */
			altserver = strdup(serversite->ss_server);

			/* connect to the alternate server */
			if (server_init(&altph, altserver))
				return -1;

			usealtph = 1;
		}
	}
	if (altph == NULL)
		altph = ph;

	/* send query */
	i = ph_query(altph, selectors,
		     (realretfields ? realretfields : retfields), &entries);

	if (usealtph)
	{
		if (ph_close(altph, 0) == -1)
			nph_printf(1, "%s: ph_close(\"%s\"): %s\n",
				   cmdv[0], altserver, strerror(errno));
		free(altserver);
	}

	if (i == PH_ERR_TOOMANY)
	{
		nph_printf(1, "too many matches\n");
		return -2;
	}
	if (i == PH_ERR_NOMATCH)
	{
		nph_printf(1, "no matches to query\n");
		return -2;
	}
	if (i == PH_ERR_DATAERR)
	{
		nph_printf(1, "invalid query\n");
		return -2;
	}
	if (i == -1)
	{
		nph_printf(1, "ph_query(): %s\n", strerror(errno));
		return -1;
	}

	/* print out returned entries */
	nph_printf(1, "matched %d entr%s\n", i, (i == 1 ? "y" : "ies"));
	if (i != 0)
	{
		for (i = 0; entries[i] != NULL; i++)
		{
			nph_printf(0, "----------------------------------------"
				   "\n");
			nph_printf(0, "ENTRY #%d:\n", i + 1);
			print_entry(entries[i], retfields, redirect_flag);
		}
		nph_printf(0, "----------------------------------------\n");
		ph_free_entries(entries);
	}

	/* clean up */
	ph_free_selectors(selectors);
	if (retfields != NULL)
		free(retfields);
	if (realretfields != retfields)
		free(realretfields);

	if (usealtph)
		nph_printf(1, "resuming primary PH connection\n");

	return 0;
}


/* me command */
int
nph_me(int cmdc, char **cmdv)
{
	ph_entry *entry;
	struct ph_fieldselector selector[2];
	char *retfield[2];
	int i;

	if (cmdv[1] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}
	if (ph_whoami(ph) == NULL)
	{
		nph_printf(1, "not logged in\n");
		return -1;
	}

	/* construct query */
	selector[0].pfs_field = "alias";
	selector[0].pfs_operation = '=';
	selector[0].pfs_value = ph_whoami(ph);
	memset(&(selector[1]), 0, sizeof(struct ph_fieldselector));
	retfield[0] = "all";
	retfield[1] = NULL;

	/* send query */
	i = ph_query(ph, selector, retfield, &entry);
	if (i == -1)
	{
		perror("ph_query()");
		return -1;
	}
	if (i != 1)
	{
		nph_printf(1, "internal error: can't get entry for alias=%s\n",
			   ph_whoami(ph));
		return -1;
	}

	/* print entry */
	print_entry(entry[0], NULL, 0);

	/* clean up and return */
	ph_free_entries(entry);
	return 0;
}


