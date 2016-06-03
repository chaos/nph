/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  siteinfo.c - libphclient code to parse siteinfo data
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


int
ph_is_siteinfo_cached(PH *ph)
{
	return (ph->ph_siteinfo != NULL ? 1 : 0);
}


/* ph_free_siteinfo_list() - free a siteinfo list. */
static void
ph_free_siteinfo_list(struct ph_siteinfo *si)
{
	int i;

	for (i = 0; si[i].ps_info != NULL; i++)
	{
#ifdef DEBUG
		printf("freeing siteinfo[%d]:  \"%s\" = \"%s\"\n", i,
		       (si[i].ps_info ? si[i].ps_info : ""),
		       (si[i].ps_data ? si[i].ps_data : ""));
#endif
		free(si[i].ps_info);
		free(si[i].ps_data);
	}

	free(si);
}


/*
** ph_retrieve_siteinfo() - read siteinfo from server.
** returns:
**	0				success
**	-1				error (sets errno)
*/
int
ph_retrieve_siteinfo(PH *ph)
{
	char *info, *setting;
	char buf[PH_BUF_SIZE];
	int code = -1, infonum = 0, i, save_errno;
	ph_memblock_t *blockp;
	struct ph_siteinfo *phsi;

	/* don't load if already cached */
	if (ph_is_siteinfo_cached(ph))
		return 0;

	if (ph_mmgr_malloc(ph->ph_mmgr, ph_MMGR_ARRAY,
			   0, sizeof(struct ph_siteinfo),
			   (ph_free_func_t)ph_free_siteinfo_list,
			   &blockp) == -1)
		return -1;
	phsi = (struct ph_siteinfo *)ph_mmgr_ptr(blockp);

	/* send command */
	if (ph_send_command(ph, "siteinfo") == -1)
		return -1;

	/* read responses */
	while (ph_get_response(ph, &code, buf, sizeof(buf)) == 0)
	{
		if (code < LR_OK && code > 0)
			continue;
		if (code == LR_OK)
		{
			ph->ph_siteinfo = phsi;
			ph_mmgr_free(blockp, 0);
			return 0;
		}

		if (ph_parse_response(buf, &i, &info, &setting) == -1)
		{
			save_errno = errno;
			ph_mmgr_free(blockp, 1);
			errno = save_errno;
			return -1;
		}

#ifdef DEBUG
		printf("code=%d, i=%d, info=\"%s\", setting=\"%s\", "
		       "infonum=%d\n", code, i, info, setting, infonum);
#endif

		if (ph_mmgr_array_add(blockp, infonum + 2) == -1)
		{
			save_errno = errno;
			ph_mmgr_free(blockp, 1);
			errno = save_errno;
			return -1;
		}
		phsi = (struct ph_siteinfo *)ph_mmgr_ptr(blockp);

		phsi[infonum].ps_info = strdup(info);
		phsi[infonum].ps_data = strdup(setting);
		if (phsi[infonum].ps_info == NULL
		    || phsi[infonum].ps_data == NULL)
		{
			save_errno = errno;
			ph_mmgr_free(blockp, 1);
			errno = save_errno;
			return -1;
		}

		infonum++;
	}

	save_errno = errno;
	ph_mmgr_free(blockp, 1);
	errno = save_errno;
	return -1;
}


/*
** ph_get_siteinfo() - return the value of a specific info field.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_DATAERR			unknown siteinfo field
*/
int
ph_get_siteinfo(PH *ph, char *info, char **data)
{
	int i;

	if (!ph_is_siteinfo_cached(ph) && ph_retrieve_siteinfo(ph) == -1)
		return -1;

	for (i = 0; ph->ph_siteinfo[i].ps_info != NULL; i++)
		if (strcasecmp(ph->ph_siteinfo[i].ps_info, info) == 0)
		{
			*data = ph->ph_siteinfo[i].ps_data;
			return 0;
		}

	return PH_ERR_DATAERR;
}


/*
** ph_siteinfo_iterate() - iterate through siteinfo list.
** returns:
**	1				data returned
**	0				no more data
**	-1				error (sets errno)
*/
int
ph_siteinfo_iterate(PH *ph, struct ph_siteinfo **siteinfo)
{
	if (!ph_is_siteinfo_cached(ph) && ph_retrieve_siteinfo(ph) == -1)
		return -1;

	/* first entry */
	if (*siteinfo == NULL)
	{
		*siteinfo = &(ph->ph_siteinfo[0]);
		return 1;
	}

	/* next entry */
	(*siteinfo)++;
	if ((*siteinfo)->ps_info != NULL)
		return 1;

	/* last entry */
	*siteinfo = NULL;
	return 0;
}


/*
** ph_free_siteinfo() - free the siteinfo list associated with ph.
*/
void
ph_free_siteinfo(PH *ph)
{
	if (ph->ph_siteinfo != NULL)
		ph_free_siteinfo_list(ph->ph_siteinfo);
	ph->ph_siteinfo = NULL;
}


