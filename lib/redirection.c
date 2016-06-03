/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  redirection.c - libphclient code to select entries from the server
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

#include <strings.h>


/*
** FIXME: can I generalize this further to help simplify ph_email_resolve() ?
*/
static int
_ph_get_alias_field(PH *ph, char *alias, char *field, char **valuep)
{
	int i, save_errno;
	char *getfields[2];
	struct ph_fieldselector query[2];
	ph_memblock_t *blockp;
	ph_entry *entries;

	/* construct query */
	query[0].pfs_field = "alias";
	query[0].pfs_operation = '=';
	query[0].pfs_value = alias;
	memset(&(query[1]), 0, sizeof(struct ph_fieldselector));
	getfields[0] = field;
	getfields[1] = NULL;

	if (ph_mmgr_malloc(ph->ph_mmgr, 0, 0, 0,
			   (ph_free_func_t)ph_free_entries,
			   &blockp) == -1)
		return -1;

	i = ph_query(ph, query, getfields,
		     (ph_entry **)&(ph_mmgr_ptr(blockp)));

	if (i <= 0)
	{
		save_errno = errno;
		ph_mmgr_free(blockp, 1);
		errno = save_errno;
		return i;
	}

	entries = (ph_entry *)ph_mmgr_ptr(blockp);

	if (strcasecmp(entries[0][0].pfv_field, field) != 0)
	{
		ph_mmgr_free(blockp, 1);
		errno = EINVAL;
		return -1;
	}

	if (entries[0][0].pfv_code != -(LR_OK)
	    || entries[0][0].pfv_value[0] == '\0')
	{
		ph_mmgr_free(blockp, 1);
		return PH_ERR_DATAERR;
	}

	if (valuep != NULL)
	{
		*valuep = strdup(entries[0][0].pfv_value);
		if (*valuep == NULL)
		{
			save_errno = errno;
			ph_mmgr_free(blockp, 1);
			errno = save_errno;
			return -1;
		}
	}

	ph_mmgr_free(blockp, 1);
	return 0;
}


/*
** ph_advertised_email() - returns alias-based email address for alias
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOMATCH			alias does not exist
**	PH_ERR_DATAERR			no email field
*/
int
ph_advertised_email(PH *ph, char *alias, int confirm_alias,
		    char **advertised_email)
{
	int i, save_errno;
	char *maildomain, *mailfield;
	void *ptr;

	/* check siteinfo for the field name */
	i = ph_get_siteinfo(ph, "mailfield", &mailfield);
	if (i == -1)
		return -1;
	if (i == PH_ERR_DATAERR)
	{
		errno = EINVAL;
		return -1;
	}

	/* shortcut */
	if (strcasecmp(mailfield, "alias") == 0 && !confirm_alias)
		*advertised_email = strdup(alias);
	else
	{
		i = _ph_get_alias_field(ph, alias, mailfield,
					advertised_email);
		if (i != 0)
			return i;
	}

	/* check siteinfo for the maildomain and append it if found */
	i = ph_get_siteinfo(ph, "maildomain", &maildomain);
	if (i == -1)
		return -1;
	if (i == 0)
	{
		ptr = realloc(*advertised_email,
			      strlen(*advertised_email) + strlen(maildomain) + 2);
		if (ptr == NULL)
		{
			save_errno = errno;
			free(*advertised_email);
			*advertised_email = NULL;
			errno = save_errno;
			return -1;
		}

		*advertised_email = (char *)ptr;
		sprintf(*advertised_email + strlen(*advertised_email), "@%s",
			maildomain);
	}

	return 0;
}


/* create a multiple-word name query */
static int
rewrite_name(char *field, char *user, struct ph_fieldselector **phf)
{
	char buf[PH_BUF_SIZE];
	int i, numfields;
	char *fieldp, *nextp;
	void *ptr;

	strlcpy(buf, user, sizeof(buf));
	for (i = 0; buf[i] != '\0'; i++)
		if (ispunct((int)buf[i]) && buf[i] != '*')
			buf[i] = ' ';

	/* if the name is all spaces, bail out */
	if (strspn(buf, " ") == strlen(buf))
		return PH_ERR_DATAERR;

	numfields = 0;
	nextp = buf;
	*phf = NULL;
	while ((fieldp = strsep(&nextp, " ")) != NULL)
	{
		if (*fieldp == '\0')
			continue;

		ptr = realloc(*phf, (numfields + 2) * sizeof(struct ph_fieldselector));
		if (ptr == NULL)
			return -1;
		*phf = (struct ph_fieldselector *)ptr;
		memset(&((*phf)[numfields]), 0,
		       2 * sizeof(struct ph_fieldselector));
		(*phf)[numfields].pfs_field = NULL;
		(*phf)[numfields].pfs_operation = '=';
		(*phf)[numfields].pfs_value = strdup(fieldp);
		if ((*phf)[numfields].pfs_value == NULL)
			return -1;
		numfields++;
	}

	return 0;
}


/* replace all punctuation characters in alias query with dashes */
static int
rewrite_alias(char *field, char *user, struct ph_fieldselector **phf)
{
	char buf[PH_BUF_SIZE];
	int i;

	strlcpy(buf, user, sizeof(buf));
	for (i = 0; buf[i] != '\0'; i++)
		if (ispunct((int)buf[i]))
			buf[i] = '-';

	*phf = (struct ph_fieldselector *)calloc(2, sizeof(struct ph_fieldselector));
	if (*phf == NULL)
		return -1;

	(*phf)[0].pfs_operation = '=';
	(*phf)[0].pfs_field = strdup(field);
	(*phf)[0].pfs_value = strdup(buf);
	if ((*phf)[0].pfs_field == NULL || (*phf)[0].pfs_value == NULL)
		return -1;

	return 0;
}


/* default action - look up user without rewriting */
static int
no_rewrite(char *field, char *user, struct ph_fieldselector **phf)
{
	*phf = (struct ph_fieldselector *)calloc(2, sizeof(struct ph_fieldselector));
	if (*phf == NULL)
		return -1;

	(*phf)[0].pfs_operation = '=';
	(*phf)[0].pfs_field = strdup(field);
	(*phf)[0].pfs_value = strdup(user);
	if ((*phf)[0].pfs_field == NULL || (*phf)[0].pfs_value == NULL)
		return -1;

	return 0;
}


typedef int (*rewritefunc_t)(char *, char *, struct ph_fieldselector **);

struct field_rewrite_map
{
	char *field;
	rewritefunc_t rewrite_funcs[3];
};
typedef struct field_rewrite_map field_rewrite_map_t;

static const field_rewrite_map_t rewrite_map[] = {
  { "alias",	{ rewrite_alias,	NULL } },
  { "name",	{ no_rewrite,		rewrite_name,	NULL } },
  { NULL,	{ no_rewrite,		NULL } }
};


/*
** ph_email_resolve() - returns the expanded email address for user
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOMATCH			no single match found
**	PH_ERR_DATAERR			no email field
*/
int
ph_email_resolve(PH *ph, char *user, char *fields, char **real_email)
{
	int i, save_errno;
	char *getfields[2];
	char *mailbox, *mailmatches;
	char buf[PH_BUF_SIZE];
	char *fieldp, *nextp;
	ph_memblock_t *qblockp, *eblockp;
	const field_rewrite_map_t *rewritep;
	const rewritefunc_t *rwfunc;
	struct ph_fieldselector *query;
	ph_entry *entries;

	/* check siteinfo for the mailbox field (usually "email") */
	i = ph_get_siteinfo(ph, "mailbox", &mailbox);
	if (i == -1)
		return -1;
	if (i == PH_ERR_DATAERR)
	{
		errno = EINVAL;
		return -1;
	}
	getfields[0] = mailbox;
	getfields[1] = NULL;
#ifdef DEBUG
	printf("ph_email_resolve(): mailbox=\"%s\"\n", mailbox);
#endif

	/* set list of fields to match */
	if (fields != NULL
	    && *fields != '\0')
		mailmatches = fields;
	else
	{
		i = ph_get_siteinfo(ph, "mailmatches", &mailmatches);
		if (i == -1)
			return -1;
		if (i == PH_ERR_DATAERR)
			mailmatches = PH_DEFAULT_MAILMATCHES;
	}
#ifdef DEBUG
	printf("ph_email_resolve(): mailmatches=\"%s\"\n", mailmatches);
#endif

	/* copy mailmatches to a temp buffer so we don't modify the original */
	strlcpy(buf, mailmatches, sizeof(buf));
	nextp = buf;

	/* iterate through the field list to find a match */
	while ((fieldp = strsep(&nextp, " :")) != NULL)
	{
		if (*fieldp == '\0')
			continue;

#ifdef DEBUG
		printf("ph_email_resolve(): trying field %s\n", fieldp);
#endif

		/* find the appropriate rewrite function(s) for the field */
		for (rewritep = rewrite_map;
		     rewritep->field != NULL;
		     rewritep++)
		{
#ifdef DEBUG
			printf("comparing \"%s\" and \"%s\"\n",
			       fieldp, rewritep->field);
#endif
			if (strcasecmp(fieldp, rewritep->field) == 0)
				break;
		}

		/* perform a query with each specified rewrite function */
		for (rwfunc = rewritep->rewrite_funcs;
		     *rwfunc != NULL;
		     rwfunc++)
		{
			if (ph_mmgr_malloc(ph->ph_mmgr, ph_MMGR_ARRAY, 0, 0,
					   (ph_free_func_t)ph_free_selectors,
					   &qblockp) == -1)
				return -1;

			/* do rewrite and perform query */
#ifdef DEBUG
			printf("calling rewrite function at 0x%lx (%s)\n",
			       *rwfunc,
			       (*rwfunc == rewrite_alias ? "rewrite_alias" :
				(*rwfunc == rewrite_name ? "rewrite_name" :
				 "no_rewrite")));
#endif
			i = (**rwfunc)(fieldp, user,
				       (struct ph_fieldselector **)&(ph_mmgr_ptr(qblockp)));
			switch (i)
			{
			case 0:
				break;

			case -1:
				save_errno = errno;
				ph_mmgr_free(qblockp, 1);
				errno = save_errno;
				return -1;

			default:
				/*
				** special case:
				** PH_ERR_DATAERR can be returned by
				** rewrite_name() when user name is
				** all punctuation chars or spaces
				*/
				ph_mmgr_free(qblockp, 1);
				return i;
			}

#ifdef DEBUG
			printf("rewrote \"%s\" to \"%s\"\n",
			       user,
			       ((struct ph_fieldselector *)(ph_mmgr_ptr(qblockp)))[0].pfs_value);
#endif

			query = (struct ph_fieldselector *)ph_mmgr_ptr(qblockp);

			if (ph_mmgr_malloc(ph->ph_mmgr, ph_MMGR_ARRAY, 0, 0,
					   (ph_free_func_t)ph_free_entries,
					   &eblockp) == -1)
				return -1;

#ifdef DEBUG
			puts("calling ph_query()");
#endif
			i = ph_query(ph, query, getfields,
				     (ph_entry **)&(ph_mmgr_ptr(eblockp)));

			/* free query */
#ifdef DEBUG
			puts("free()'ing query memory");
#endif
			ph_mmgr_free(qblockp, 1);

			/* if ph_query() returned an error above */
#ifdef DEBUG
			printf("ph_query() returned %d\n", i);
#endif
			if (i == -1 || i == PH_ERR_DATAERR)
			{
				save_errno = errno;
				ph_mmgr_free(eblockp, 1);
				errno = save_errno;
				return i;
			}

			entries = (ph_entry *)ph_mmgr_ptr(eblockp);

			/* one match found - return it */
			if (i == 1)
			{
				/* if the one match is an error... */
				if (entries[0][0].pfv_code != -(LR_OK))
				{
					ph_mmgr_free(eblockp, 1);
					return PH_ERR_DATAERR;
				}

				/*
				** terminate string at first whitespace or
				** comma (in case more than one email
				** address is listed)
				*/
				nextp = entries[0][0].pfv_value;
				while (*nextp != '\0'
				       && !isspace((int)*nextp)
				       && (*nextp != ','))
					nextp++;
				*nextp = '\0';

#ifdef DEBUG
				puts("returning match");
#endif
				*real_email = strdup(entries[0][0].pfv_value);
				if (*real_email == NULL)
				{
					save_errno = errno;
					ph_mmgr_free(eblockp, 1);
					errno = save_errno;
					return -1;
				}

				ph_mmgr_free(eblockp, 1);
				return 0;
			}

			/* more than one match found - clean up */
			ph_mmgr_free(eblockp, 1);
		}
	}

	/* no matches found */
	return PH_ERR_NOMATCH;
}


/*
** ph_advertised_www() - returns alias-based URL for alias
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOMATCH			alias does not exist
**	PH_ERR_DATAERR			no www field
*/
int
ph_advertised_www(PH *ph, char *alias, int confirm_alias,
		  char **advertised_url)
{
	char *wwwredirect;
	int i, code = 0, save_errno = 0;

	/* check siteinfo for the wwwredirect field */
	i = ph_get_siteinfo(ph, "wwwredirect", &wwwredirect);
	if (i == -1)
		return -1;
	if (i == PH_ERR_DATAERR)
		return ph_www_resolve(ph, alias, advertised_url);

	/* do a query to make sure that the alias exists */
	if (confirm_alias)
	{
		i = _ph_get_alias_field(ph, alias, "www", NULL);
		if (i != 0)
			return i;
	}

	*advertised_url = (char *)malloc(strlen(wwwredirect) + strlen(alias) + 1);
	if (*advertised_url == NULL)
		return -1;
	sprintf(*advertised_url, "%s%s", wwwredirect, alias);
	return 0;
}


/*
** ph_www_resolve() - returns the expanded URL for user
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOMATCH			no single entry matched
**	PH_ERR_DATAERR			no www field
*/
int
ph_www_resolve(PH *ph, char *user, char **real_url)
{
	return _ph_get_alias_field(ph, user, "www", real_url);
}


