/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  nph.c - main driver program for nph
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <errno.h>
#include <sys/param.h>
#include <netdb.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif


/* global variables */
PH *ph = NULL;
char *ph_server = NULL;
struct ph_serverlist serverlist;
short got_servers;


static void
usage(void)
{
	printf("Usage: nph [options] [selectors [return field1 ...]]\n");
	printf("Options: [-s server[:port]] [-f rcfile] [-F fields] [-cCdmrR]\n");
	exit(1);
}


int
main(int argc, char *argv[])
{
	int i, readconfig = 1;
	char *nphrc = NULL;

	/* initialize */
	reset_output(1);
	set_signals();

	/* parse commandline options */
	while ((i = getopt(argc, argv, "cCdf:F:hmrRs:V")) != -1)
	{
		switch (i)
		{
		case 'c':
			set_option("confirmedits", "on");
			break;
		case 'C':
			readconfig = 0;
			break;
		case 'f':
			nphrc = strdup(optarg);
			break;
		case 'd':
			set_option("debug", "on");
			break;
		case 'F':
			set_option("returnfields", optarg);
			break;
		case 'm':
			set_option("usepager", "off");
			break;
		case 'r':
			set_option("canonicaladdrs", "off");
			break;
		case 'R':
			set_option("usereservedport", "on");
			break;
		case 's':
			if (ph_server != NULL)
				free(ph_server);
			ph_server = strdup(optarg);
			break;
		case 'V':
			printf("nph %s by Mark D. Roth <roth@feep.net>\n",
			       PACKAGE_VERSION);
			break;
		case 'h':
		default:
			usage();
		}
	}

	/* determine server */
	if (ph_server == NULL)
		ph_server = getenv("PH_SERVER");
	if (ph_server == NULL)
		ph_server = read_server_file();

	/* connect to server */
	if (server_init(&ph, ph_server) != 0)
		exit(1);

	/* drop privs */
	if (geteuid() == 0
	    && setuid(getuid()) != 0)
	{
		perror("setuid()");
		ph_close(ph, 0);
		exit(1);
	}

	/* query listed on commandline - disable pager */
	if (optind < argc)
		set_option("usepager", "off");

	/* read the user's rc file */
	if (readconfig)
		read_nphrc(nphrc);
	if (nphrc != NULL)
		free(nphrc);

	/*
	** query listed on commandline.
	** just do the query and exit, don't be interactive
	*/
	if (optind < argc)
	{
#ifdef DEBUG
		for (i = optind; i < argc; i++)
			printf("argv[%d] = \"%s\"\n", i, argv[i]);
#endif

		/* do query and exit */
		i = nph_query(argc - optind + 1, &(argv[optind - 1]));
		ph_close(ph, 0);
		exit(i);
	}

	/* otherwise, go into interactive mode */
	putchar('\n');
	interactive();

	/* can't happen */
	fprintf(stderr, "nph: internal error: reached end of main()\n");
	exit(1);
}


