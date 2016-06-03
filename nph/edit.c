/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  edit.c - nph code to edit server data
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <errno.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif


/* use an external editor to edit the given text */
static int
edit_value(char *oldval, char *buf, size_t bufsize)
{
	int i, fd;
	char *editor, *cp;
	char path[MAXPATHLEN];
	char cmd[NPH_BUF_SIZE];
	char c;

	/* create temp file */
	cp = getenv("TMPDIR");
	if (cp == NULL)
		cp = "/tmp";
	snprintf(path, sizeof(path), "%s/nph-XXXXXX", cp);
	fd = mkstemp(path);
	if (fd == -1)
	{
		fprintf(stderr, "nph: edit_value(): mkstemp(): %s\n",
			strerror(errno));
		return -1;
	}

	/* write the current value out to the temp file */
	if (write(fd, oldval, strlen(oldval)) == -1)
	{
		perror("write()");
		close(fd);
		unlink(path);
		return -1;
	}
	if (close(fd) == -1)
	{
		perror("close()");
		unlink(path);
		return -1;
	}

	/* determine which editor to invoke */
	get_option("editor", &editor);

	while (1)
	{
		/* invoke the editor */
		snprintf(cmd, sizeof(cmd), "%s %s", editor, path);
		if (system(cmd))
		{
			perror("system()");
			unlink(path);
			return -1;
		}

		/* continue with the change if confirmedits isn't on */
		get_option("confirmedits", &i);
		if (i == 0)
			break;

		do
		{
			nph_printf(1,
				   "confirm change ([Y]es, [N]o, [E]dit): ");
			fgets(cmd, sizeof(cmd), stdin);
			c = tolower(cmd[0]);
		}
		while (c != 'y' && c != 'n' && c != 'e');

		/* procede with change */
		if (c == 'y')
			break;

		/* edit again */
		if (c == 'e')
			continue;

		/* user typed 'n' */
		unlink(path);
		return -1;
	}

	/* re-read the tmpfile */
	fd = open(path, O_RDONLY);
	if (fd == -1)
	{
		perror("open()");
		unlink(path);
		return -1;
	}
	if ((i = read(fd, buf, bufsize)) == -1)
	{
		perror("read()");
		unlink(path);
		return -1;
	}
	buf[i] = '\0';
	if (close(fd) == -1)
	{
		perror("close()");
		unlink(path);
		return -1;
	}
	if (unlink(path) == -1)
	{
		perror("unlink()");
		return -1;
	}

	return 0;
}


/* edit command */
int
nph_edit(int cmdc, char **cmdv)
{
	int i;
	char buf[NPH_BUF_SIZE];
	ph_entry *entry;
	struct ph_fieldselector selector[2];
	struct ph_fieldselector change[2];
	char *retfields[2];
	char *user;

	if (cmdv[1] == NULL)
	{
		fprintf(stderr, "edit: field parameter missing\n");
		return -1;
	}

	user = ph_whoami(ph);
	if (user == NULL)
	{
		fprintf(stderr, "not logged in\n");
		return 1;
	}

	/* create the query */
	selector[0].pfs_field = "alias";
	selector[0].pfs_operation = '=';
	selector[0].pfs_value = (cmdv[2] ? cmdv[2] : user);
	memset(&(selector[1]), 0, sizeof(struct ph_fieldselector));
	retfields[0] = cmdv[1];
	retfields[1] = NULL;

	/* send the query */
	i = ph_query(ph, selector, retfields, &entry);
	if (i == -1)
	{
		perror("ph_query()");
		return -1;
	}
	if (i != 1)
	{
		fprintf(stderr,
			"internal error: can't get alias=%s field %s\n",
			ph_whoami(ph), cmdv[1]);
		ph_free_entries(entry);
		return -1;
	}

	/* let the user edit the value */
	if (edit_value(entry[0][0].pfv_value, buf, sizeof(buf)))
	{
		ph_free_entries(entry);
		return -1;
	}

	/* create the change */
	change[0].pfs_field = cmdv[1];
	change[0].pfs_operation = '=';
	change[0].pfs_value = buf;
	memset(&(change[1]), 0, sizeof(struct ph_fieldselector));

	/* send change command */
	i = ph_change(ph, selector, change, 0);
	switch (i)
	{
	case -1:
		nph_printf(1, "ph_change(): %s\n", strerror(errno));
		break;

	case PH_ERR_NOTLOG:
		/*
		** can't happen, because we check for this above,
		** but we'll handle it, just in case
		*/
		nph_printf(1, "not logged in\n");
		break;

	case PH_ERR_READONLY:
		nph_printf(1, "server is in read-only mode\n");
		break;

	case PH_ERR_DATAERR:
		nph_printf(1, "illegal value specified\n");
		break;

	default:
		break;
	}

	/* clean up and return */
	ph_free_entries(entry);
	return 0;
}


