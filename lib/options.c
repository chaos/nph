/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  options.c - libphclient code to handle server options
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
ph_is_optionlist_cached(PH *ph)
{
	return (ph->ph_options != NULL ? 1 : 0);
}


/* ph_free_optionlist() - free option list. */
static void
ph_free_optionlist(struct ph_option *opt)
{
	int i;

	for (i = 0; opt[i].po_option != NULL; i++)
	{
		free(opt[i].po_option);
		free(opt[i].po_setting);
	}

	free(opt);
}


/*
** ph_retrieve_options() - obtain options from server.
**	0				success
**	-1				error (sets errno)
*/
int
ph_retrieve_options(PH *ph)
{
	int code, numopts = 0, save_errno;
	char *option, *setting;
	char buf[PH_BUF_SIZE];
	void *ptr;
	struct ph_option *opt;
	ph_memblock_t *blockp;

	/* don't load if already cached */
	if (ph_is_optionlist_cached(ph))
		return 0;

	if (ph_send_command(ph, "set") == -1)
		return -1;

	if (ph_mmgr_malloc(ph->ph_mmgr, ph_MMGR_ARRAY,
			   0, sizeof(struct ph_option),
			   (ph_free_func_t)ph_free_optionlist,
			   &blockp) == -1)
		return -1;
	opt = (struct ph_option *)ph_mmgr_ptr(blockp);

	while (ph_get_response(ph, &code, buf, sizeof(buf)) == 0)
	{
		if (code < LR_OK && code > 0)
			continue;
		if (code == LR_OK)
		{
			ph->ph_options = opt;
			ph_mmgr_free(blockp, 0);
			return 0;
		}
		if (code != -(LR_OK))
		{
			ph_mmgr_free(blockp, 1);
			errno = EINVAL;
			return -1;
		}

		if (ph_parse_response(buf, NULL, &option, &setting) == -1)
		{
			ph_mmgr_free(blockp, 1);
			errno = EINVAL;
			return -1;
		}

		if (ph_mmgr_array_add(blockp, numopts + 2) == -1)
		{
			save_errno = errno;
			ph_mmgr_free(blockp, 1);
			errno = save_errno;
			return -1;
		}
		opt = (struct ph_option *)ph_mmgr_ptr(blockp);

		opt[numopts].po_option = strdup(option);
		opt[numopts].po_setting = strdup(setting);
		if (opt[numopts].po_option == NULL
		    || opt[numopts].po_setting == NULL)
		{
			save_errno = errno;
			ph_mmgr_free(blockp, 1);
			errno = save_errno;
			return -1;
		}

		numopts++;
	}

	return 0;
}


/*
** ph_set_option() - set an option on the server.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_DATAERR			no such option
*/
int
ph_set_option(PH *ph, char *option, char *setting)
{
	int code, i;

	if (ph_send_command(ph, "set %s%s%s", option, (setting ? "=" : ""),
			    (setting ? setting : "")) == -1)
		return -1;

	do
	{
		if (ph_get_response(ph, &code, NULL, 0) == -1)
			return -1;
	}
	while (code < LR_OK);

	if (code == LR_OPTION)
		return PH_ERR_DATAERR;
	if (code != LR_OK)
	{
		errno = EINVAL;
		return -1;
	}

	/* if the option list is stored in the local PH handle, update it */
	if (ph_is_optionlist_cached(ph))
		for (i = 0; ph->ph_options[i].po_option != NULL; i++)
			if (strcasecmp(ph->ph_options[i].po_option,
				       option) == 0)
			{
				free(ph->ph_options[i].po_setting);
				ph->ph_options[i].po_setting = strdup(setting
								      ? setting
								      : "on");
				if (ph->ph_options[i].po_setting == NULL)
					return -1;
				break;
			}

	return 0;
}


/*
** ph_get_option() - obtain the current setting of a given option.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_DATAERR			no such option
*/
int
ph_get_option(PH *ph, char *option, char **setting)
{
	int i;

	if (!ph_is_optionlist_cached(ph)
	    && ph_retrieve_options(ph) == -1)
		return -1;

	for (i = 0; ph->ph_options[i].po_option != NULL; i++)
		if (strcasecmp(ph->ph_options[i].po_option, option) == 0)
		{
			*setting = ph->ph_options[i].po_setting;
			return 0;
		}

	return PH_ERR_DATAERR;
}


/*
** ph_option_iterate() - iterate through server option list.
** returns:
**	1				data returned
**	0				no more data
**	-1				error (sets errno)
*/
int
ph_option_iterate(PH *ph, struct ph_option **svropt)
{
	if (!ph_is_optionlist_cached(ph)
	    && ph_retrieve_options(ph) == -1)
		return -1;

	if (*svropt == NULL)
	{
		*svropt = &(ph->ph_options[0]);
		return 1;
	}

	/* get next element */
	(*svropt)++;
	if ((*svropt)->po_option != NULL)
		return 1;

	/* end of list */
	*svropt = NULL;
	return 0;
}


/*
** ph_free_options() - free option list associated with PH handle ph.
*/
void
ph_free_options(PH *ph)
{
	if (ph->ph_options != NULL)
		ph_free_optionlist(ph->ph_options);
	ph->ph_options = NULL;
}


