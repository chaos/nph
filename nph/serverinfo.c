/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  serverinfo.c - nph code to handle references to other PH servers
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <errno.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif


/* add entry to list of known PH servers */
int
nph_serveradd(int cmdc, char **cmdv)
{
	char buf[NPH_BUF_SIZE];

	if (cmdc < 2 || cmdc > 3)
	{
		print_help(cmdv[0]);
		return -1;
	}

	if (cmdv[2] != NULL)
		/* FIXME: not part of libphclient public API! */
		ph_dequote_value(cmdv[2], buf, sizeof(buf));
	else
		buf[0] = '\0';

	return ph_serverlist_add(&serverlist, cmdv[1], buf);
}


/* view list of known PH servers */
int
nph_listservers(int cmdc, char **cmdv)
{
	int i;
	struct ph_serversite *serversite = NULL;

	if (cmdc > 3)
	{
		print_help(cmdv[0]);
		return -1;
	}

	if (!got_servers && ph_serverlist_merge(ph, &serverlist) == -1)
	{
		nph_printf(1, "%s: ph_serverlist_merge(): %s\n", cmdv[0],
			   strerror(errno));
		return -1;
	}
	got_servers = 1;

	nph_printf(0, "\n%-20s %s\n", "SERVER", "SITE");
	while ((i = ph_serverlist_iterate(&serverlist, cmdv[1],
					  NULL, &serversite)) != 0)
	{
		if (i == -1)
		{
			nph_printf(1, "%s: ph_serverlist_iterate(): %s\n",
				   cmdv[0], strerror(errno));
			return -1;
		}
		nph_printf(0, "%-20s %s\n", serversite->ss_server,
			   serversite->ss_site);
	}

	return 0;
}


/* status */
int
nph_status(int cmdc, char **cmdv)
{
	char *p;

	if (cmdv[1] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	if (ph_status(ph, &p) == -1)
	{
		nph_printf(1, "ph_status(): %s\n", strerror(errno));
		return -1;
	}

	if (p != NULL)
	{
		nph_printf(0, "\nSTATUS INFORMATION:\n\n%s", p);
		free(p);
	}
	else
		nph_printf(1, "%s: no status information available\n",
			   cmdv[0]);

	return 0;
}


/* show or set server options */
int
nph_set(int cmdc, char **cmdv)
{
	char *p;
	int i, cmdi;
	struct ph_option *phop = NULL;

	/* if options were given, set them */
	if (cmdv[1] != NULL)
	{
		for (cmdi = 1; cmdv[cmdi] != NULL; cmdi++)
		{
			if ((p = strchr(cmdv[cmdi], '=')) != NULL)
				*p++ = '\0';
			i = ph_set_option(ph, cmdv[cmdi], p);
			if (i == -1)
			{
				nph_printf(1, "ph_set_option(): %s\n",
					   strerror(errno));
				return -1;
			}
			else if (i == PH_ERR_DATAERR)
			{
				nph_printf(1, "unknown option \"%s\"\n",
					   cmdv[cmdi]);
				return 1;
			}
			nph_printf(0, "set %s=%s\n", cmdv[cmdi],
				   (p != NULL ? p : "on"));
		}

		return 0;
	}

	/* no options given - display current settings */
	nph_printf(0, "\n%-20.20s\t%s\n", "OPTION", "SETTING");
	while ((i = ph_option_iterate(ph, &phop)) == 1)
		nph_printf(0, "%-20.20s\t%s\n",
			   phop->po_option, phop->po_setting);
	if (i == -1)
		nph_printf(1, "ph_option_iterate(): %s\n", strerror(errno));

	return i;
}


/* print out siteinfo data */
int
nph_siteinfo(int cmdc, char **cmdv)
{
	int i;
	struct ph_siteinfo *siteinfo = NULL;

	if (cmdv[1] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	nph_printf(0, "\n%-20.20s\t%s\n", "INFO", "SETTING");
	while ((i = ph_siteinfo_iterate(ph, &siteinfo)) == 1)
		nph_printf(0, "%-20.20s\t%s\n", siteinfo->ps_info,
			   siteinfo->ps_data);
	if (i == -1)
		nph_printf(1, "ph_siteinfo_iterate(): %s\n", strerror(errno));

	return i;
}


/* display server field information */
int
nph_fields(int cmdc, char **cmdv)
{
	char buf[NPH_BUF_SIZE];
	int i, cmdi;
	struct ph_fieldinfo *fieldinfo = NULL;

	/* display field information */
	nph_printf(0, "\n%-20.20s\t%2s\t%4s\t%s\n\t\t\t%s\n\t\t\t%s\n",
		   "FIELD", "ID", "MAX", "ATTRIBUTES", "DESCRIPTION",
		   "ALIASES");

	/* if arguments were specified, show those fields only */
	if (cmdv[1] != NULL)
	{
		for (cmdi = 1; cmdv[cmdi] != NULL; cmdi++)
		{
			if ((i = ph_get_fieldinfo(ph, cmdv[cmdi],
						  &fieldinfo)) == -1)
			{
				nph_printf(1, "ph_get_fieldinfo(): %s\n",
					   strerror(errno));
				return -1;
			}
			if (i == 1)
				nph_printf(0, "%-20.20s\t*** no such field\n",
					   cmdv[cmdi]);
			else
			{
				ph_decode_field_attributes(fieldinfo->pf_attrib,
							   buf, sizeof(buf));
				nph_printf(0, "%-20.20s\t%2d\t%4d\t%s\n"
					   "\t\t\t%s\n\t\t\t",
					   fieldinfo->pf_fnames[0],
					   fieldinfo->pf_id,
					   fieldinfo->pf_max_size,
					   buf, fieldinfo->pf_description);
				for (i = 1;
				     fieldinfo->pf_fnames[i] != NULL;
				     i++)
					nph_printf(0, "%s ",
						   fieldinfo->pf_fnames[i]);
				nph_printf(0, "\n");
			}
		}
		return 0;
	}

	/* otherwise, show all fields */
	while (ph_fieldinfo_iterate(ph, &fieldinfo) == 1)
	{
		ph_decode_field_attributes(fieldinfo->pf_attrib, buf,
					   sizeof(buf));
		nph_printf(0, "%-20.20s\t%2d\t%4d\t%s\n\t\t\t%s\n\t\t\t",
			   fieldinfo->pf_fnames[0], fieldinfo->pf_id,
			   fieldinfo->pf_max_size, buf,
			   fieldinfo->pf_description);
		for (i = 1; fieldinfo->pf_fnames[i]; i++)
			nph_printf(0, "%s ", fieldinfo->pf_fnames[i]);
		nph_printf(0, "\n");
	}

	return 0;
}


