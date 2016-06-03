/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  handle.c - libphclient code to initialize a PH handle
**
**  Mark D. Roth <roth@feep.net>
*/

#include <internal.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/param.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_ARPA_AIXRCMDS_H
# include <arpa/aixrcmds.h>
#endif


const char libphclient_version[] = PACKAGE_VERSION;

const int libphclient_is_thread_safe =
#if defined(HAVE_GETHOSTBYNAME_R) && defined(HAVE_GETSERVBYNAME_R) && defined(HAVE_GETPWUID_R)
					1;
#else
					0;
#endif


/*
** ph_open_local() - opens a pipe to a local qi process.
** returns: 0 on success, -1 on error (and sets errno)
*/
static int
ph_open_local(PH *ph, char *qi_path)
{
	pid_t pid;

	if (pipe(ph->ph_rfd) == -1)
		return -1;
	if (pipe(ph->ph_wfd) == -1)
	{
		close(ph->ph_rfd[0]);
		ph->ph_rfd[0] = -1;
		close(ph->ph_rfd[1]);
		ph->ph_rfd[1] = -1;
		return -1;
	}

	if ((pid = fork()) == -1)
	{
		close(ph->ph_rfd[0]);
		ph->ph_rfd[0] = -1;
		close(ph->ph_rfd[1]);
		ph->ph_rfd[1] = -1;
		close(ph->ph_wfd[0]);
		ph->ph_wfd[0] = -1;
		close(ph->ph_wfd[1]);
		ph->ph_wfd[1] = -1;
		return -1;
	}

	/* child sets its I/O to the pipe and exec's qi */
	if (pid == 0)
	{
		if (dup2(ph->ph_wfd[0], 0) == -1
		    || dup2(ph->ph_rfd[1], 1) == -1
		    || dup2(ph->ph_rfd[1], 2) == -1
		    || close(ph->ph_rfd[1]) == -1
		    || close(ph->ph_rfd[0]) == -1
		    || close(ph->ph_wfd[1]) == -1
		    || close(ph->ph_wfd[0]) == -1)
			_exit(1);

		if (strchr(qi_path, '/') != NULL)
			execl(qi_path, "qi", "-q", NULL);
		else
			execlp(qi_path, "qi", "-q", NULL);

		/* NOTREACHED */
		_exit(1);
	}

	/* parent closes the extra file descriptors and returns */
	close(ph->ph_wfd[0]);
	ph->ph_wfd[0] = -1;
	close(ph->ph_rfd[1]);
	ph->ph_rfd[1] = -1;

	return 0;
}


#if MAXPATHLEN > MAXHOSTNAMELEN
# define PH_SERVER_LEN MAXPATHLEN
#else
# define PH_SERVER_LEN MAXHOSTNAMELEN
#endif


/*
** ph_rresvport() - thread-safe alternative to rresvport()
*/
static int
ph_rresvport(int sock)
{
	struct sockaddr_in local_addr;

	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = INADDR_ANY;

	for (local_addr.sin_port = IPPORT_RESERVED - 1;
	     local_addr.sin_port > 0;
	     local_addr.sin_port--)
	{
		if (bind(sock, (struct sockaddr *)&local_addr,
			 sizeof(local_addr)) == 0)
			return 0;
	}

	errno = EADDRINUSE;
	return -1;
}


/*
** ph_open_remote() - opens a socket to a remote PH server.
** returns: 0 on success, -1 on error (and sets errno)
*/
static int
ph_open_remote(PH *ph, char *server, int flags)
{
	struct sockaddr_in addr;
	int i;
	char serverbuf[PH_SERVER_LEN];
	char *cp;
	struct hostent *hp;
#ifdef HAVE_GETHOSTBYNAME_R
	struct hostent hent;
	int h_errno_r;
#endif
	struct servent *sp;
#ifdef HAVE_GETSERVBYNAME_R
	struct servent sent;
#endif
#if defined(HAVE_GETHOSTBYNAME_R) || defined(HAVE_GETSERVBYNAME_R)
	char buf[10240];
#endif

	addr.sin_family = AF_INET;
	strlcpy(serverbuf, server, sizeof(serverbuf));

	/* set the server's port */
	if ((cp = strchr(server, ':')) != NULL)
	{
		if ((cp - server) < sizeof(serverbuf))
			serverbuf[cp - server] = '\0';
		addr.sin_port = htons(atoi(++cp));
	}
#ifdef HAVE_GETSERVBYNAME_R
	else if (getservbyname_r(PH_SERVICE, "tcp", &sent,
				 buf, sizeof(buf), &sp) == 0
		 || sp == NULL)
#else /* ! HAVE_GETSERVBYNAME_R */
	else if ((sp = getservbyname(PH_SERVICE, "tcp")) != NULL)
#endif /* HAVE_GETSERVBYNAME_R */
		addr.sin_port = sp->s_port;
	else
		addr.sin_port = htons(PH_PORT);

	/* lookup the server's address */
#ifdef HAVE_GETHOSTBYNAME_R
	if (gethostbyname_r(serverbuf, &hent, buf, sizeof(buf),
			    &hp, &h_errno_r) != 0
	    || hp == NULL)
#else /* ! HAVE_GETHOSTBYNAME_R */
	hp = gethostbyname(serverbuf);
	if (hp == NULL)
#endif /* HAVE_GETHOSTBYNAME_R */
	{
		errno = EINVAL;
		return -1;
	}
	for (i = 0; hp->h_addr_list[i] != NULL; i++)
	{
		memmove(&(addr.sin_addr.s_addr), hp->h_addr_list[i],
			hp->h_length);

		if ((ph->ph_rfd[0] = socket(PF_INET, SOCK_STREAM, 0)) == -1)
			return -1;

		/* bind to a reserved port if appropriate */
		if (BIT_ISSET(flags, PH_OPEN_PRIVPORT)
		    && ph_rresvport(ph->ph_rfd[0]) == -1)
			return -1;

		/* connect to server */
		if (connect(ph->ph_rfd[0], (struct sockaddr *)&addr,
			    sizeof(addr)) == -1)
		{
			close(ph->ph_rfd[0]);
			ph->ph_rfd[0] = -1;
			if (BIT_ISSET(flags, PH_OPEN_ROUNDROBIN))
				continue;
			return -1;
		}

		return 0;
	}

	return -1;
}


/*
** ph_open() - opens a connection to the PH server.
** returns: 0 on success, -1 on error (and sets errno)
*/
int
ph_open(PH **ph, char *server, int flags,
	ph_debug_func_t send_hook, ph_debug_func_t recv_hook,
	void *hook_data)
{
	int i;
	char buf[PH_BUF_SIZE];

	/* initialize PH data structure */
	*ph = (PH *)calloc(1, sizeof(PH));
	if (*ph == NULL)
		return -1;
	(*ph)->ph_rfd[0] = -1;
	(*ph)->ph_rfd[1] = -1;
	(*ph)->ph_wfd[0] = -1;
	(*ph)->ph_wfd[1] = -1;
	(*ph)->ph_sendhook = send_hook;
	(*ph)->ph_recvhook = recv_hook;
	(*ph)->ph_hook_data = hook_data;

	/* create network buffer */
	if (ph_buffer_new(&((*ph)->ph_buf), PH_BUF_SIZE,
			  ph_buffer_read, *ph) == -1)
		return -1;

	/* create mmgr handle */
	if (ph_mmgr_init(&((*ph)->ph_mmgr)) == -1)
		return -1;

	/* connect to the server */
	if (BIT_ISSET(flags, PH_OPEN_LOCAL))
		i = ph_open_local(*ph, server);
	else
		i = ph_open_remote(*ph, server, flags);

	if (i == 0
	    && !BIT_ISSET(flags, PH_OPEN_DONTID))
	{
		snprintf(buf, sizeof(buf), "libphclient-%s",
			 libphclient_version);
		if (ph_id(*ph, buf) == -1)
			return -1;
	}

	return i;
}


/*
** ph_close() - closes a connection to the PH server.
** returns: 0 on success, -1 on error (and sets errno)
*/
int
ph_close(PH *ph, int flags)
{
	int i, code;

	if (!BIT_ISSET(flags, PH_CLOSE_FAST))
	{
		if (ph_send_command(ph, "quit") == -1)
			return -1;
		while (ph_get_response(ph, &code, NULL, 0) != -1
		       && code != LR_OK);
	}

	if (ph->ph_wfd[1] >= 0)
	{
		if (close(ph->ph_wfd[1]) == -1)
			return -1;
		ph->ph_wfd[1] = -1;
	}
	if (close(ph->ph_rfd[0]) == -1)
		return -1;
	ph->ph_rfd[0] = -1;

	if (ph_is_fieldinfo_cached(ph))
		ph_free_fieldinfo(ph);
	if (ph_is_optionlist_cached(ph))
		ph_free_options(ph);
	if (ph_is_siteinfo_cached(ph))
		ph_free_siteinfo(ph);
	if (ph->ph_auth != NULL)
		ph_free_auth(ph->ph_auth);
	if (ph->ph_mmgr != NULL)
		ph_mmgr_cleanup(ph->ph_mmgr);
	free(ph);

	return 0;
}


/*
** ph_rfd() - return the read file descriptor associated with the PH handle.
*/
int
ph_rfd(PH *ph)
{
	return ph->ph_rfd[0];
}


/*
** ph_wfd() - return the write file descriptor associated with the PH handle.
*/
int
ph_wfd(PH *ph)
{
	return (ph->ph_wfd[1] >= 0
		? ph->ph_wfd[1]
		: ph->ph_rfd[0]);
}


