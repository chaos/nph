/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  login.c - authentication code for nph
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <errno.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif


/* id */
int
nph_id(int cmdc, char **cmdv)
{
	char buf[NPH_BUF_SIZE];
	int i, buflen = 0;

	if (cmdv[1] == NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	for (i = 1; cmdv[i] != NULL; i++)
		buflen += snprintf(buf + buflen, sizeof(buf) - buflen, " %s",
				   cmdv[i]);
	if (ph_id(ph, buf + 1) == -1)
	{
		nph_printf(1, "ph_id(): %s\n", strerror(errno));
		return -1;
	}

	nph_printf(0, "sent id \"%s\"\n", buf + 1);
	return 0;
}


/* external */
int
nph_external(int cmdc, char **cmdv)
{
	if (cmdv[1] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	if (ph_external(ph) == -1)
	{
		nph_printf(1, "ph_external(): %s\n", strerror(errno));
		return -1;
	}

	nph_printf(0, "session is now external\n");
	return 0;
}


/* suser command */
int
nph_suser(int cmdc, char **cmdv)
{
	int i;

	if (cmdv[1] == NULL
	    || cmdv[2] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	i = ph_suser(ph, cmdv[1]);
	if (i == -1)
		nph_printf(1, "ph_suser(): %s\n", strerror(errno));
	else if (i == PH_ERR_NOTHERO || i == PH_ERR_NOTLOG)
		nph_printf(1, "permission denied\n");
	else
		nph_printf(0, "suser succeeded\n");

	return i;
}


/* login to server */
int
nph_login(int cmdc, char **cmdv)
{
	int i, auth_method;
	char *p;

	if (cmdv[1] == NULL
	    || cmdv[2] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	/* check auth_method configuration option */
	get_option("authtype", &auth_method);

	/* read password from user if needed */
	if (auth_method == PH_AUTH_EMAIL)
		p = NULL;
	else
		p = getpass("Password: ");

	/* login to server */
	i = ph_login(ph, cmdv[1], auth_method, p);
	if (i == -1)
		nph_printf(1, "ph_login(): %s\n", strerror(errno));
	else if (i == PH_ERR_NOTLOG)
		nph_printf(1, "login incorrect\n");
	else
		nph_printf(0, "login successful\n");

	return i;
}


/* logout of server */
int
nph_logout(int cmdc, char **cmdv)
{
	int i;

	if (cmdv[1] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	i = ph_logout(ph);
	if (i == -1)
		nph_printf(1, "ph_logout(): %s\n", strerror(errno));
	else if (i == PH_ERR_NOTLOG)
		nph_printf(1, "not logged in\n");
	else
		nph_printf(0, "logged out\n");

	return i;
}


/* change password */
int
nph_passwd(int cmdc, char **cmdv)
{
	char *pass, *p;
	int i;

	if (ph_whoami(ph) == NULL)
	{
		nph_printf(1, "not logged in\n");
		return -1;
	}
	if (cmdv[1] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	/* read new password twice */
	p = getpass("New password: ");
	if (p == NULL
	    || *p == '\0')
	{
		nph_printf(1, "invalid password\n");
		return -1;
	}
	pass = strdup(p);
	p = getpass("Confirm password: ");
	if (p == NULL
	    || *p == '\0')
	{
		nph_printf(1, "invalid password\n");
		return -1;
	}
	if (strcmp(pass, p) != 0)
	{
		nph_printf(1, "passwords do not match\n");
		return -1;
	}

	/* issue change command */
	i = ph_passwd(ph, pass);
	if (i == -1)
		nph_printf(1, "ph_passwd()i: %s\n", strerror(errno));
	else if (i == PH_ERR_NOTLOG)
		nph_printf(1, "not logged in\n");
	else
		nph_printf(1, "password updated\n");

	/* clean up and return */
	free(pass);
	return i;
}


