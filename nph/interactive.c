/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  interactive.c - command line interface for nph
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <errno.h>
#include <setjmp.h>

/* FIXME: IRIX's poll() is broken... */
#ifdef sgi
# undef HAVE_POLL
#endif

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif

#include <strings.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#else
# ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
# endif
#endif

#ifdef HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#endif
#ifdef HAVE_READLINE_HISTORY_H
# include <readline/history.h>
#endif


/* local convenience variable */
const char *prompt = "nph> ";


#ifdef HAVE_LIBREADLINE

/* readline hook to process the entered command */
static void
rl_docmd(char *line)
{
	char buf[NPH_BUF_SIZE];	/* space for an input line */

	/* EOF */
	if (line == NULL)
	{
		rl_callback_handler_remove();
		sighandler_state = HANDLER_DEFAULT;
		ph_close(ph, 0);
		exit(0);
	}

	/* blank line */
	if (*line == '\0')
	{
		free(line);
		return;
	}

	add_history(line);
	strlcpy(buf, line, sizeof(buf));
	free(line);

	/* do command */
	nph_command(buf);
	putchar('\n');
}


/* server option completion function */
static char *
rl_complete_set(char *text, int state)
{
	static struct ph_option *phopt;
	static int textlen;
	int debug;

	if (!state)
	{
		phopt = NULL;
		textlen = strlen(text);
		if (!ph_is_optionlist_cached(ph))
		{
			get_option("debug", &debug);
			if (debug)
			{
				putchar('\n');
				rl_on_new_line();
			}
			if (ph_retrieve_options(ph) == -1)
			{
				perror("ph_retrieve_options()");
				return NULL;
			}
			if (debug)
				rl_redisplay();
		}
	}

	while (ph_option_iterate(ph, &phopt) == 1)
		if (strncasecmp(phopt->po_option, text, textlen) == 0)
			return strdup(phopt->po_option);

	return NULL;
}


/* field completion function */
static char *
rl_complete_field(char *text, int state)
{
	static struct ph_fieldinfo *phfield;
	static int namecounter;
	static int textlen;
	int debug;

	if (!state)
	{
		phfield = NULL;
		textlen = strlen(text);
		if (!ph_is_fieldinfo_cached(ph))
		{
			get_option("debug", &debug);
			if (debug)
			{
				putchar('\n');
				rl_on_new_line();
			}
			if (ph_retrieve_fieldinfo(ph) == -1)
			{
				perror("ph_retrieve_fieldinfo()");
				return NULL;
			}
			if (debug)
				rl_redisplay();
#ifdef DEBUG
			printf("==> rl_complete_field(\"%s\")\n", text);
#endif
		}
	}
	else
		namecounter++;

	while (1)
	{
		if (phfield != NULL)
			for (;
			     phfield->pf_fnames[namecounter] != NULL;
			     namecounter++)
			{
#ifdef DEBUG
				printf("rl_complete_field(): "
				       "phfield->pf_fnames[%d] = \"%s\"\n",
				       namecounter,
				       phfield->pf_fnames[namecounter]);
#endif
				if (strncasecmp(phfield->pf_fnames[namecounter],
						text, textlen) == 0)
					return strdup(phfield->pf_fnames[namecounter]);
			}

		if (ph_fieldinfo_iterate(ph, &phfield) != 1)
			break;

		namecounter = 0;
	}

	return NULL;
}


/* command completion function (in commands.c) */
char *rl_complete_command(char *text, int state);

/* config option completion function (in conf.c) */
char *rl_complete_conf(char *text, int state);


/* completion hook */
static char **
rl_completion_hook(char *text, int start, int end)
{
	int leading_spaces;
	char **cpp;

	/* handle completion interrupts */
	if (sigsetjmp(complete_env, 1) != 0)
		return NULL;
	sighandler_state = HANDLER_COMPLETION;

	/*
	** inhibit normal filename completion.
	** (must be in the completion hook.)
	*/
	rl_attempted_completion_over = 1;

	leading_spaces = strspn(rl_line_buffer, " ");

	/* command completion at beginning of line or as argument to "help" */
	if (start == leading_spaces
	    || strncasecmp(rl_line_buffer + leading_spaces, "help ", 5) == 0)
	{
		rl_completion_append_character = ' ';
		return rl_completion_matches(text,
					     (rl_compentry_func_t *)rl_complete_command);
	}

	/* client option completion */
	if (strncasecmp(rl_line_buffer + leading_spaces, "option ", 7) == 0
	    || strncasecmp(rl_line_buffer + leading_spaces, "switch ", 7) == 0)
	{
		rl_completion_append_character = '=';
		return rl_completion_matches(text,
					     (rl_compentry_func_t *)rl_complete_conf);
	}

	/* server option completion */
	if (strncasecmp(rl_line_buffer + leading_spaces, "set ", 4) == 0)
	{
		rl_completion_append_character = '=';
		return rl_completion_matches(text,
					     (rl_compentry_func_t *)rl_complete_set);
	}

	/* file name completion */
	if (strncasecmp(rl_line_buffer + leading_spaces, "source ", 7) == 0)
	{
		rl_completion_append_character = ' ';
		return rl_completion_matches(text,
					     (rl_compentry_func_t *)rl_filename_completion_function);
	}

	/* field completion */
	if (strncasecmp(rl_line_buffer + leading_spaces, "fields ", 7) == 0 
	    || strncasecmp(rl_line_buffer + leading_spaces, "edit ", 5) == 0
	    || (strncasecmp(rl_line_buffer + leading_spaces, "query ", 6) == 0
	        && strstr(rl_line_buffer, " return ") != NULL))
		rl_completion_append_character = ' ';
	else
		rl_completion_append_character = '=';
	cpp = rl_completion_matches(text,
				    (rl_compentry_func_t *)rl_complete_field);
	if (cpp != NULL)
		return cpp;

	/* keyword completion */
	rl_completion_append_character = ' ';

	if (strncasecmp(rl_line_buffer + leading_spaces, "query ", 6) == 0
	    && strspn(rl_line_buffer + leading_spaces + 5, " ") < (size_t)(start - (leading_spaces + 5))
	    && strstr(rl_line_buffer, " return ") == NULL)
	{
		if (strncasecmp(text, "return", strlen(text)) == 0)
		{
			cpp = (char **)calloc(2, sizeof(char *));
			if (cpp != NULL)
				cpp[0] = strdup("return");
		}
		return cpp;
	}

	if (strncasecmp(rl_line_buffer + leading_spaces, "change ", 7) == 0
	    && strspn(rl_line_buffer + leading_spaces + 6, " ") < (size_t)(start - (leading_spaces + 6))
	    && strstr(rl_line_buffer, " make ") == NULL
	    && strstr(rl_line_buffer, " force ") == NULL)
	{
		if (strncasecmp(text, "make", strlen(text)) == 0)
		{
			cpp = (char **)calloc(2, sizeof(char *));
			if (cpp != NULL)
				cpp[0] = strdup("make");
			return cpp;
		}
		if (strncasecmp(text, "force", strlen(text)) == 0)
		{
			cpp = (char **)calloc(2, sizeof(char *));
			if (cpp != NULL)
				cpp[0] = strdup("force");
			return cpp;
		}
	}

	return NULL;
}

#endif /* HAVE_LIBREADLINE */


void
interactive(void)
{
	char cmdbuf[NPH_BUF_SIZE];
#ifdef HAVE_POLL
	struct pollfd pfd[2];
#else
	fd_set rfds, efds;
#endif

#ifdef HAVE_LIBREADLINE
	/* initialize readline */
	if (isatty(fileno(stdin)))
	{
		/* set readline callback handler for polling */
		rl_callback_handler_install(prompt, rl_docmd);

		/* set completion hook */
		rl_attempted_completion_function = (CPPFunction *)rl_completion_hook;
		rl_ignore_completion_duplicates = 1;

		/* set application name for readline init file */
		rl_readline_name = "nph";
	}
#endif /* HAVE_LIBREADLINE */

	while (1)
	{
		/* set state for signal handler */
		sighandler_state = HANDLER_INPUT;

		/* initialize file descriptor multiplexing */
#ifdef HAVE_POLL
		pfd[0].fd = fileno(stdin);
		pfd[0].events = POLLIN | POLLPRI;
		pfd[0].revents = 0;
		pfd[1].fd = ph_rfd(ph);
		pfd[1].events = POLLIN | POLLPRI | POLLERR;
		pfd[1].revents = 0;
#else /* ! HAVE_POLL */
		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(fileno(stdin), &rfds);
		FD_SET(ph_rfd(ph), &rfds);
		FD_SET(ph_rfd(ph), &efds);
#endif /* HAVE_POLL */

#ifndef HAVE_LIBREADLINE
		if (isatty(fileno(stdin)))
		{
			printf("%s", prompt);
			fflush(stdout);		/* prompt */
		}
#endif /* ! HAVE_LIBREADLINE */

#ifdef HAVE_POLL
		if (poll(pfd, 2, -1) == -1)
		{
			if (errno == EINTR)
				continue;
			perror("poll()");
			exit(1);
		}
		if (pfd[1].revents)
#else /* ! HAVE_POLL */
		if (select((fileno(stdin) > ph_rfd(ph)
			    ? fileno(stdin)
			    : ph_rfd(ph)) + 1,
			   &rfds, NULL, &efds, NULL) == -1)
		{
			if (errno == EINTR)
				continue;
			perror("select()");
			exit(1);
		}
		if (FD_ISSET(ph_rfd(ph), &rfds)
		    || FD_ISSET(ph_rfd(ph), &efds))
#endif /* HAVE_POLL */
		{
			puts("\nnph: server closed connection, exiting");
#ifdef HAVE_LIBREADLINE
			rl_callback_handler_remove();
#endif /* HAVE_LIBREADLINE */
			ph_close(ph, PH_CLOSE_FAST);
			exit(1);
		}

		/* command input available */
#ifdef HAVE_POLL
		if (pfd[0].revents)
#else /* ! HAVE_POLL */
		if (FD_ISSET(fileno(stdin), &rfds))
#endif /* HAVE_POLL */
		{
#ifdef HAVE_LIBREADLINE
			if (isatty(fileno(stdin)))
			{
				rl_callback_read_char();
				continue;
			}
#endif /* HAVE_LIBREADLINE */

			if (fgets(cmdbuf, sizeof(cmdbuf), stdin) == NULL)
			{
				if (ferror(stdin))
				{
					perror("fgets()");
					exit(1);
				}

				/* got EOF - exit normally */
				ph_close(ph, 0);
				exit(0);
			}

			/* strip trailing newline */
			if (cmdbuf[strlen(cmdbuf) - 1] == '\n')
				cmdbuf[strlen(cmdbuf) - 1] = '\0';

			/* skip empty lines */
			if (cmdbuf[0] == '\0')
				continue;

			nph_command(cmdbuf);
			putchar('\n');
		}
	}

	/* can't happen */
	fprintf(stderr,
		"nph: internal error: reached the end of interactive()\n");
	exit(1);
}


