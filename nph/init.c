/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  init.c - nph code to initialize a new server connection
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <errno.h>
#include <pwd.h>
#include <sys/param.h>
#include <netdb.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif


/* verify that a field exists on the server */
int
check_field(PH *newph, char *field)
{
	struct ph_fieldinfo *fieldinfo = NULL;

	if (ph_get_fieldinfo(newph, field, &fieldinfo) == PH_ERR_DATAERR)
	{
		nph_printf(1, "unknown field \"%s\"\n", field);
		return -1;
	}
	return 0;
}


/* verify that a list of fields exists on the server */
int
check_fields(PH *newph, char *string)
{
	char buf[NPH_BUF_SIZE];
	char *wordp, *nextp;

	strlcpy(buf, string, sizeof(buf));
	nextp = buf;
	while ((wordp = strsep(&nextp, " ")) != NULL)
	{
		if (*wordp == '\0')
			continue;

		if (check_field(newph, wordp) == -1)
			return -1;
	}

	return 0;
}


/* connect to PH server and initialize options */
int
server_init(PH **newph, char *server)
{
	int i, debug, ph_open_flags = PH_OPEN_DONTID;
	char *fields = NULL;
	char buf[NPH_BUF_SIZE];

	/* set flags */
	if (strchr(server, '/') != NULL)
		ph_open_flags |= PH_OPEN_LOCAL;
	else if (geteuid() == 0)
	{
		get_option("usereservedport", &i);
		if (i)
			ph_open_flags |= PH_OPEN_PRIVPORT;
	}
	get_option("debug", &debug);

	/* open connection */
	if (ph_open(newph, server, ph_open_flags,
		    (debug ? send_debug : NULL),
		    (debug ? recv_debug : NULL),
		    NULL) == -1)
	{
		nph_printf(1, "ph_open(): %s\n", strerror(errno));
		return -1;
	}
	nph_printf(1, "connected to PH server %s\n", server);

	/* send id */
	snprintf(buf, sizeof(buf), "nph-%s", PACKAGE_VERSION);
	if (ph_id(*newph, buf) == -1)
	{
		nph_printf(1, "ph_id(): %s\n", strerror(errno));
		return -1;
	}

	/* validate field list */
	get_option("returnfields", &fields);
	if (fields != NULL
	    && check_fields(*newph, fields))
	{
		nph_printf(1, "unsetting invalid \"returnfields\" option\n");
		set_option("returnfields", "");
	}

	return 0;
}


/* read the system-wide ph_server file */
char *
read_server_file(void)
{
	FILE *f;
	char buf[MAXHOSTNAMELEN + 5];

	f = fopen(SERVER_FILE, "r");
	if (f == NULL)
	{
		nph_printf(1, "cannot open server file %s: %s\n",
			   SERVER_FILE, strerror(errno));
		exit(1);
	}

	if (fgets(buf, sizeof(buf), f) == NULL)
	{
		nph_printf(1, "fgets(): %s\n", strerror(errno));
		fclose(f);
		exit(1);
	}

	fclose(f);

	/* strip newline */
	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	return strdup(buf);
}


/* change servers */
int
nph_connect(int cmdc, char **cmdv)
{
	PH *newph;

	if (cmdv[1] == NULL
	    || cmdv[2] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	if (server_init(&newph, cmdv[1]) != 0)
		return -1;

	/* close old connection */
	if (ph != NULL)
	{
		nph_printf(1, "disconnecting from server %s\n", ph_server);
		if (ph_close(ph, 0) == -1)
			nph_printf(1, "warning: ph_close(): %s\n",
				   strerror(errno));
	}

	/* save new handle */
	ph = newph;
	ph_server = strdup(cmdv[1]);
	got_servers = 0;

	return 0;
}


/* read an nphrc file */
int
read_nphrc(char *filename)
{
	struct passwd *pw;
	char nphrc[MAXPATHLEN];
	char *cp;
	int i;

	if (filename != NULL)
		return read_command_file(filename);

	cp = getenv("HOME");
	if (cp == NULL)
	{
		pw = getpwuid(getuid());
		if (pw == NULL)
		{
			perror("nph: cannot determine user's home directory: "
			       "getpwuid()");
			return -1;
		}
		cp = pw->pw_dir;
	}

	snprintf(nphrc, sizeof(nphrc), "%s/.nphrc.%s", cp, ph_server);
	i = read_command_file(nphrc);

	if (i == 1)
	{
		snprintf(nphrc, MAXPATHLEN, "%s/.nphrc", cp);
		i = read_command_file(nphrc);
	}

	if (i == 1)
	{
		snprintf(nphrc, MAXPATHLEN, "%s", SYSTEM_NPHRC);
		i = read_command_file(nphrc);
	}

	return i;
}


/* read commands from a file and execute them */
int
read_command_file(char *filename)
{
	char buf[NPH_BUF_SIZE];
	FILE *f;
	int line = 0;
	char *p;

	/* open file */
	f = fopen(filename, "r");
	if (f == NULL)
	{
		if (errno == ENOENT)
			return 1;
		nph_printf(1, "cannot open %s: %s\n", filename,
			   strerror(errno));
		return -1;
	}
	nph_printf(1, "reading file %s...\n", filename);

	/* parse each line of the config file */
	while (fgets(buf, sizeof(buf), f) != NULL)
	{
		line++;

		/* strip off newlines */
		if (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = '\0';

		/* strip off comments */
		if ((p = strchr(buf, '#')) != NULL)
			*p = '\0';

		/* handle blank lines */
		if (buf[0] == '\0')
			continue;

		/* run command */
		nph_command(buf);
	}

	fclose(f);
	return 0;
}


/* read commands from a file */
int
nph_source(int cmdc, char **cmdv)
{
	int i;

	if (cmdv[1] == NULL
	    || cmdv[2] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	i = read_command_file(cmdv[1]);
	if (i == 1)
		nph_printf(1, "cannot open %s: no such file or directory\n",
			   cmdv[1]);
	return i;
}


