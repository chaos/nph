/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  conf.c - nph client option code
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <errno.h>
#include <pwd.h>
#include <sys/param.h>
#include <netdb.h>
#include <ctype.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif

#include <strings.h>

#ifdef HAVE_PATHS_H
# include <paths.h>
#endif

#ifndef _PATH_VI
# define _PATH_VI	VI
#endif

#ifndef _PATH_MORE
# define _PATH_MORE	MORE
#endif


/* plugin prototypes */

#if 0
static int set_numeric(int, char *);
static int get_numeric(int, void *);
#endif

static int set_boolean(int, char *);
static int get_boolean(int, void *);

static int set_string(int, char *);
static int get_string(int, void *);

static int set_authmethod(int, char *);
static int get_authmethod(int, void *);

static int get_prog(int, void *);

static int set_debug(int, char *);

static int set_fields(int, char *);


typedef int (*nph_set_opt_func_t)(int, char *);
typedef int (*nph_get_opt_func_t)(int, void *);

struct nph_option
{
	char *option;
	int freeflag;
	char *str;
	nph_set_opt_func_t setval;
	nph_get_opt_func_t getval;
};

static struct nph_option nphopts[] = {
  { "authtype",		0,	"password",
				set_authmethod,		get_authmethod },
  { "canonicaladdrs",	0,	"off",
				set_boolean,		get_boolean },
  { "confirmedits",	0,	"off",
				set_boolean,		get_boolean },
  { "debug",		0,	"off",
				set_debug,		get_boolean },
  { "defaultfield",	0,	"",
				set_fields,		get_string },
  { "editor",		0,	"$EDITOR",
				set_string,		get_prog },
  { "pager",		0,	"$PAGER",
				set_string,		get_prog },
  { "returnfields",	0,	"",
				set_fields,		get_string },
  { "usepager",		0,	"on",
				set_boolean,		get_boolean },
  { "usereservedport",	0,	"off",
				set_boolean,		get_boolean },
  { NULL,		0,	NULL,
				NULL,			NULL }
};


#ifdef HAVE_LIBREADLINE

/* option completion function */
char *
rl_complete_conf(char *text, int state)
{
	static int i;
	static int textlen;

	if (!state)
	{
		i = 0;
		textlen = strlen(text);
	}
	else
		i++;

	for (; nphopts[i].option != NULL; i++)
		if (strncasecmp(nphopts[i].option, text, textlen) == 0)
			return strdup(nphopts[i].option);

	return NULL;
}

#endif /* HAVE_LIBREADLINE */


/* internal function - find a given option index */
static int
find_option(char *name)
{
	int i;

	for (i = 0; nphopts[i].option != NULL; i++)
		if (strcasecmp(nphopts[i].option, name) == 0)
			break;

	return i;
}


/* set/get the setting for an option */

int
get_option(char *name, void *value)
{
	int i;

	i = find_option(name);
	if (nphopts[i].option == NULL)
		return -1;

	return (*(nphopts[i].getval))(i, value);
}

int
set_option(char *name, char *value)
{
	int i;

	i = find_option(name);
	if (nphopts[i].option == NULL)
		return -1;

	return (*(nphopts[i].setval))(i, value);
}


/* string functions */

static int
get_string(int i, void *ptr)
{
	char **string = (char **)ptr;

	*string = nphopts[i].str;
	return 0;
}


static int
set_string(int i, char *string)
{
	if (nphopts[i].freeflag)
		free(nphopts[i].str);
	nphopts[i].str = strdup(string);
	nphopts[i].freeflag = 1;
	return 0;
}


#if 0

/* numeric functions */

static int
set_numeric(int i, char *number)
{
	char *cp;

	for (cp = number; *cp != '\0'; cp++)
		if (!isdigit((int)*cp))
		{
			nph_printf(1, "invalid value for numeric option "
				   "\"%s\"\n", nphopts[i].option);
			return -1;
		}

	return set_string(i, number);
}


static int
get_numeric(int i, void *ptr)
{
	int *number = (int *)ptr;

	*number = atoi(nphopts[i].str);
	return 0;
}

#endif


/* boolean functions */

static int
set_boolean(int i, char *val)
{
	if (strcasecmp(val, "on") != 0
	    && strcasecmp(val, "off") != 0)
	{
		nph_printf(1, "invalid value for boolean option \"%s\"\n",
			   nphopts[i].option);
		return -1;
	}

	return set_string(i, val);
}

static int
get_boolean(int i, void *ptr)
{
	int *val = (int *)ptr;

	if (strcasecmp(nphopts[i].str, "on") == 0)
		*val = 1;
	else if (strcasecmp(nphopts[i].str, "off") == 0)
		*val = 0;
	else
		return -1;

	return 0;
}


static int
set_fields(int i, char *string)
{
	if (ph != NULL
	    && string != NULL
	    && *string != '\0')
	{
		if (strcasecmp(nphopts[i].option, "returnfields") == 0)
		{
			if (check_fields(ph, string) == -1)
				return -1;
		}
		else if (check_field(ph, string) == -1)
			return -1;
	}

	return set_string(i, string);
}


static int
get_prog(int i, void *ptr)
{
	char **string = (char **)ptr;

	/* if the value is set, use it */
	if (nphopts[i].str != NULL)
	{
		/* if value starts with '$', get environment variable */
		if (nphopts[i].str[0] == '$')
		{
			*string = getenv(nphopts[i].str + 1);
			if (*string != NULL)
				return 0;
			/* otherwise, fall through to defaults */
		}
		else
		{
			*string = nphopts[i].str;
			return 0;
		}

	}

	/* otherwise, use default */
	if (strcasecmp(nphopts[i].option, "editor") == 0)
		*string = _PATH_VI;
	else if (strcasecmp(nphopts[i].option, "pager") == 0)
		*string = _PATH_MORE;

	return 0;
}


static int
set_debug(int i, char *val)
{
	int j;

	j = set_boolean(i, val);

	if (ph != NULL
	    && j != -1)
	{
		if (strcasecmp(val, "on") == 0)
		{
			ph_set_sendhook(ph, send_debug);
			ph_set_recvhook(ph, recv_debug);
		}
		else
		{
			ph_set_sendhook(ph, NULL);
			ph_set_recvhook(ph, NULL);
		}
	}

	return j;
}


static int
set_authmethod(int i, char *method)
{
	int j;
	const char *methods[] = {
		"email", "password", "passwd", "clear", NULL
	};

	for (j = 0; methods[j] != NULL; j++)
		if (strcasecmp(methods[j], method) == 0)
		{
			if (strcasecmp(method, "clear") == 0)
				nph_printf(0, "WARNING: the \"clear\" "
					   "authentication method is insecure "
					   "and should not be used!\n");

			nphopts[i].str = strdup(method);
			nphopts[i].freeflag = 1;
			return 0;
		}

	nph_printf(1, "unknown authtype \"%s\"\n", method);
	return -1;
}


static int
get_authmethod(int i, void *ptr)
{
	int *method = (int *)ptr;

	if (strcasecmp(nphopts[i].str, "email") == 0)
		*method = PH_AUTH_EMAIL;
	else if (strcasecmp(nphopts[i].str, "password") == 0
		 || strcasecmp(nphopts[i].str, "passwd") == 0)
		*method = PH_AUTH_PASSWORD;
	else if (strcasecmp(nphopts[i].str, "clear") == 0)
		*method = PH_AUTH_CLEAR;
	else
		return -1;

	return 0;
}


/* command for option configuration */
int
nph_option(int cmdc, char **cmdv)
{
	int i = 0;
	char *setting = NULL;
	char buf[NPH_BUF_SIZE];

	switch (cmdc)
	{
	case 4:
		if (strcmp(cmdv[2], "=") == 0)
			setting = cmdv[3];
		else
			i = 1;
		break;

	case 3:
		if (cmdv[1][strlen(cmdv[1]) - 1] == '=')
		{
			setting = cmdv[2];
			cmdv[1][strlen(cmdv[1]) - 1] = '\0';
		}
		else if (cmdv[2][0] == '=')
			setting = cmdv[2] + 1;
		else

	default:
		i = 1;
		/* FALLTHROUGH */

	case 1:
		break;

	case 2:
		setting = strchr(cmdv[1], '=');
		if (setting != NULL)
			*setting++ = '\0';
		break;
	}

	if (i != 0)
	{
		nph_printf(1, "%s: invalid argument\n", cmdv[0]);
		print_help(cmdv[0]);
		return -1;
	}

	/* no setting listed - print current setting(s) */
	if (cmdv[1] == NULL)
	{
		nph_printf(0, "nph options:\n\n");
		for (i = 0; nphopts[i].option != NULL; i++)
			nph_printf(0, "%20.20s = %s\n", nphopts[i].option,
				   nphopts[i].str);
		return 0;
	}

	/* argument listed - find corresponding option */
	if (setting != NULL)
		/* FIXME: not part of libphclient public API! */
		ph_dequote_value(setting, buf, sizeof(buf));
	i = find_option(cmdv[1]);
	if (nphopts[i].option == NULL)
	{
		nph_printf(1, "unknown option \"%s\"\n", cmdv[1]);
		return -1;
	}

	/* set the option if a value was specified */
	if (setting != NULL)
		return (*(nphopts[i].setval))(i, buf);

	/* otherwise print the value of the option */
	nph_printf(0, "nph options:\n\n");
	nph_printf(0, "%20.20s = %s\n", nphopts[i].option, nphopts[i].str);

	return 0;
}


