/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  commands.c - nph command vector code
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

#ifdef HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#endif


typedef int (*cmd_func_t)(int, char **);

struct command
{
	char *command;
	int pager;
	cmd_func_t function;
	char *helptext;
};

const struct command cmds[] = {
  { "connect",	0,	nph_connect,
		"connect host[:port]\tConnect to a new PH server" },
  { "source",	0,	nph_source,
		"source file\t\tRead commands from a file" },
  {".",		0,	nph_source,
		". file\t\t\t(same as source command)" },
  { "<",	0,	nph_source,
		"< file\t\t\t(same as source command)" },
  { "id",	0,	nph_id,
		"id text\t\t\tIdentify yourself for the server's logs" },
  { "external",	0,	nph_external,
		"external\t\tSet session to be non-local" },
  { "suser",	0,	nph_suser,
		"suser alias\t\tBecome user alias (heros only)" },
  { "login",	0,	nph_login,
		"login alias\t\tLog in to the server" },
  { "logout",	0,	nph_logout,
		"logout\t\t\tLog out of the server" },
  { "passwd",	0,	nph_passwd,
		"passwd\t\t\tChange your PH password" },
  { "password",	0,	nph_passwd,
		"password\t\t(same as passwd command)" },
  { "option",	0,	nph_option,
		"option [opt[=val]]\tShow or set nph client options" },
  { "switch",	0,	nph_option,
		"switch [opt[=val]]\t(same as option command)" },
  { "help",	0,	nph_help,
		"help [command]\t\tShow help for nph commands" },
  { "?",	0,	nph_help,
		"?\t\t\t(same as help command)" },
  { "quit",	0,	nph_quit,
		"quit\t\t\tEnd PH session" },
  { "bye",	0,	nph_quit,
		"bye\t\t\t(same as quit command)" },
  { "exit",	0,	nph_quit,
		"exit\t\t\t(same as quit command)" },
  { "stop",	0,	nph_quit,
		"stop\t\t\t(same as quit command)" },
  { "whoami",	0,	nph_whoami,
		"whoami\t\t\tDisplays the currently logged-in username" },
  { "status",	1,	nph_status,
		"status\t\t\tView the server's message of the day" },
  { "motd",	1,	nph_status,
		"motd\t\t\t(same as status command)" },
  { "serveradd", 0,	nph_serveradd,
		"serveradd server [sitename]\n\t\t\t"
		"Add a server to the list of known servers" },
  { "listservers", 1,	nph_listservers,
		"listservers [domain]\tView list of other known PH servers" },
  { "set",	0,	nph_set,
		"set [option[=setting]]\tShow or set server options" },
  { "siteinfo",	0,	nph_siteinfo,
		"siteinfo\t\tRetrieve site-specific settings from server" },
  { "fields",	1,	nph_fields,
		"fields [field]\t\tView the fields supported by the server" },
  { "edit",	0,	nph_edit,
		"edit field [alias]\tEdit a given field (once logged in)" },
  { "change",	0,	nph_change,
		"change selectors... make|force changes...\n\t\t\t"
		"Change data on server" },
  { "make",	0,	nph_change,
		"make changes...\t\t"
		"Change data of currently logged-in user on server" },
  { "delete",	0,	nph_delete,
		"delete selectors...\tDelete entries on server (heros only)" },
  { "add",	0,	nph_add,
		"add fields...\t\tAdd an entry on the server (heros only)" },
  { "me",	1,	nph_me,
		"me\t\t\tView your PH entry (once logged in)" },
  { "query",	1,	nph_query,
		"query selectors [return fields...] [@domain]\n\t\t\t"
		"Retrieve data from the server" },
  { "ph",	1,	nph_query,
		"ph selectors [return fields...] [@domain]\n\t\t\t"
		"(same as query command)" },
#if 0
  { "select",	0,	nph_select,
		"select [selectors]\t\t\t"
		"Select entries for further processing" },
#endif
  { "resolve_email", 0,	nph_resolvemail,
		"resolve_email [keyfields=field1:field2:...] user\n\t\t\t"
		"Resolve email address for user" },
  { "public_email", 0,	nph_publicemail,
		"public_email alias\t"
		"Determine the advertised email address for alias" },
  { "resolve_www", 0,	nph_resolvewww,
		"resolve_www user\tResolve URL for user" },
  { "public_www", 0,	nph_publicwww,
		"public_www alias\tDetermine the advertised URL for alias" },
  { NULL,	0,	NULL,
		NULL }
};


/* parse a command vector and call the appropriate function */
int
nph_command(char *command)
{
	char *p, *q;
	char *wordp, *nextp;
	char **cmdv = NULL;
	int i, j, cmdc = 0;
	int redirected = 0;

#ifdef DEBUG
	printf("==> nph_command(\"%s\")\n", command);
#endif

	/* now parse actual command */
	nextp = command;
	do
	{
		/* skip leading spaces */
		wordp = nextp + strspn(nextp, " ");
		if (*nextp == '\0')
			break;

		/* if wordp starts with '>', output to a file */
		if (wordp[0] == '>')
		{
			q = wordp + 1;

			/* append if ">>" */
			if (wordp[1] == '>')
			{
				i = 1;
				q++;
			}
			else
				i = 0;

			/* open the file */
			q += strspn(q, " ");
			if (output_file(q, i) == -1)
				return -1;

			/* done parsing command */
			redirected = 1;
			break;
		}

		/* if wordp starts with '|', pipe to program */
		if (wordp[0] == '|')
		{
			q = wordp + 1;
			q += strspn(q, " ");

			if (output_pipe(q) == -1)
				return -1;

			/* done parsing command */
			redirected = 1;
			break;
		}

		/* find a non-escaped quote */
		p = strchr(wordp, '"');
		if (p != NULL
		    && p != wordp
		    && *(p - 1) == '\\')
			p = NULL;

		/* find a non-escaped space */
		q = strchr(wordp, ' ');
		if (q != NULL
		    && *(q - 1) == '\\')
			q = NULL;

		/*
		** quoted string - word ends at first non-escaped space
		** after next non-escaped quote
		*/
		if (p != NULL
		    && (p - q) < 0)
		{
			for (q = strchr(p + 1, '"');
			     q != NULL;
			     q = strchr(q + 1, '"'))
				if (*(q - 1) != '\\')
					break;
			if (q == NULL)
			{
				nph_printf(1, "parse error - unmatched '\"'\n");
				return -1;
			}
			for (q = strchr(q + 1, ' ');
			     q != NULL;
			     q = strchr(q + 1, ' '))
				if (*(q - 1) != '\\')
					break;
		}

		/* set the word delimiter */
		if (q != NULL)
			*q++ = '\0';
		nextp = q;

		cmdv = (char **)realloc(cmdv, (cmdc + 2) * sizeof(char *));
		cmdv[cmdc] = wordp;
		cmdv[++cmdc] = NULL;
	}
	while (nextp != NULL);

#ifdef DEBUG
	for (i = 0; i < cmdc; i++)
		printf("cmdv[%d] = \"%s\"\n", i, cmdv[i]);
#endif

	/* find command function */
	for (i = 0; cmds[i].command != NULL; i++)
		if (strcasecmp(cmds[i].command, cmdv[0]) == 0)
			break;

	/*
	** if the command wasn't redirected and is set for paging,
	** use the pager
	*/
	get_option("usepager", &j);
	if (j && !redirected && cmds[i].pager)
	{
		get_option("pager", &q);

		if (output_pipe(q) == -1)
			return -1;
	}

	/* call command */
	if (cmds[i].function != NULL)
	{
		if (!sigsetjmp(command_env, 1))
		{
			sighandler_state = HANDLER_COMMAND;
			i = (*(cmds[i].function))(cmdc, cmdv);
		}
	}
	else
	{
		nph_printf(1, "unknown command \"%s\"\n", cmdv[0]);
		i = -1;
	}

	/* reset output */
	reset_output(0);
	free(cmdv);
	return i;
}


/* prints the help text for command */
int
nph_help(int cmdc, char **cmdv)
{
	int i;

	/* if no argument, print all help */
	if (cmdv[1] == NULL)
	{
		for (i = 0; cmds[i].helptext != NULL; i++)
			nph_printf(0, "%s\n", cmds[i].helptext);
		return 0;
	}

	/* print help for the specified command only */
	if (cmdv[2] != NULL)
	{
		nph_printf(1, "help: too many arguments\n");
		print_help("help");
	}
	else
		print_help(cmdv[1]);

	return 0;
}


/* prints the help text for command */
void
print_help(char *command)
{
	int i;

	for (i = 0; cmds[i].command != NULL; i++)
		if (strcasecmp(cmds[i].command, command) == 0)
			break;

	if (cmds[i].helptext != NULL)
		nph_printf(0, "%s\n", cmds[i].helptext);
	else
		nph_printf(1, "unknown command \"%s\"\n", command);
}


#ifdef HAVE_LIBREADLINE

/* command completion function */
char *
rl_complete_command(char *text, int state)
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

	for (; cmds[i].command != NULL; i++)
		if (strncasecmp(cmds[i].command, text, textlen) == 0)
			return strdup(cmds[i].command);

	return NULL;
}

#endif /* HAVE_LIBREADLINE */


/* quit */
int
nph_quit(int cmdc, char **cmdv)
{
	if (cmdv[1] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

#ifdef HAVE_LIBREADLINE
	rl_callback_handler_remove();
#endif

	sighandler_state = HANDLER_DEFAULT;

	if (ph_close(ph, 0) == -1)
	{
		nph_printf(1, "ph_close(): %s\n", strerror(errno));
		exit(1);
	}
	exit(0);
}


/* whoami */
int
nph_whoami(int cmdc, char **cmdv)
{
	char *user;

	if (cmdv[1] != NULL)
	{
		print_help(cmdv[0]);
		return -1;
	}

	user = ph_whoami(ph);
	if (user == NULL)
		user = "(anonymous)";

	nph_printf(0, "user %s on server %s\n", user, ph_server);
	return 0;
}


