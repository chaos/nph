/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  sighandler.c - nph signal handling code
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <errno.h>
#include <signal.h>
#include <setjmp.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#endif


/* global variables */
sigjmp_buf complete_env, command_env;
int sighandler_state = HANDLER_DEFAULT;


/* signal handler for SIGCONT */
static void
resume(int sig)
{
#ifdef HAVE_LIBREADLINE
	if (sighandler_state != HANDLER_COMMAND)
		rl_forced_update_display();
#endif /* HAVE_LIBREADLINE */
}


/* signal handler for SIGINT */
static void
interrupt(int sig)
{
	if (sighandler_state == HANDLER_DEFAULT)
	{
		fprintf(stderr, "\nnph: terminating on signal %d\n", sig);
		exit(1);
	}

	if (sighandler_state == HANDLER_INPUT)
		/*
		** interrupting a normal input line
		** restart input on next line
		*/
		putchar('\n');

	else if (sighandler_state == HANDLER_COMMAND)
		/*
		** interrupting a command
		** print a message before the next input line
		*/
		nph_printf(0, "\nnph: command interrupted\n");

#ifdef HAVE_LIBREADLINE
	/* reset tty state in all cases */
	rl_reset_after_signal();

	if (sighandler_state == HANDLER_COMPLETION)
	{
		/* interrupt completion - jump back to rl_completion_hook() */
		siglongjmp(complete_env, 1);

		/* can't happen */
		fprintf(stderr,
			"interrupt(): internal error: "
			"returned from siglongjmp()\n");
		exit(1);
	}

	/*
	** clear input buffer and issue a new prompt
	** (this is needed for both HANDLER_INPUT and HANDLER_COMMAND)
	*/
	rl_delete_text(0, rl_end);
	rl_point = rl_end = rl_done = 0;

	if (sighandler_state == HANDLER_INPUT)
		rl_forced_update_display();
#endif /* HAVE_LIBREADLINE */

	if (sighandler_state == HANDLER_COMMAND)
	{
		/* jump back to command loop */
		siglongjmp(command_env, 1);

		/* can't happen */
		fprintf(stderr,
			"interrupt(): internal error: "
			"returned from siglongjmp()\n");
		exit(1);
	}
}


void
set_signals(void)
{
	sighandler_state = HANDLER_DEFAULT;

	if (isatty(fileno(stdin)))
	{
#ifdef HAVE_SIGACTION
		struct sigaction sa;

		memset(&sa, 0, sizeof(struct sigaction));
		sa.sa_handler = interrupt;
		sa.sa_flags = SA_RESTART;
		if (sigaction(SIGINT, &sa, NULL) == -1)
		{
			perror("sigaction()");
			exit(1);
		}
		sa.sa_handler = resume;
		if (sigaction(SIGCONT, &sa, NULL) == -1)
		{
			perror("sigaction()");
			exit(1);
		}
#else /* ! HAVE_SIGACTION */
		if (signal(SIGINT, interrupt) == SIG_ERR)
		{
			perror("signal()");
			exit(1);
		}
		if (signal(SIGCONT, resume) == SIG_ERR)
		{
			perror("signal()");
			exit(1);
		}
#endif /* HAVE_SIGACTION */
	}
}


