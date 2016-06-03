/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  auth.c - authentication code for libphclient
**
**  Mark D. Roth <roth@feep.net>
*/

#include <internal.h>

#include <errno.h>
#include <pwd.h>
#include <sys/types.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif


/* function prototypes */
typedef int (*ph_authfunc_t)(PH *, struct ph_auth *, char *, char *,
			     char *, size_t);

static int ph_passwd_auth(PH *, struct ph_auth *, char *, char *,
			  char *, size_t);
static int ph_email_auth(PH *, struct ph_auth *, char *, char *,
			 char *, size_t);


/* details of each auth type */
struct authtype
{
	int at_type;		/* auth type */
	char *at_logincmd;	/* server login command for this auth method */
	char *at_passcmd;	/* password or challenge response command */
	ph_authfunc_t at_authfunc;	/* function to modify auth token
					   (optional) */
	int at_denycode;	/* reply code sent from server to deny
				   authentication */
};
typedef struct authtype authtype_t;


static authtype_t authtypes[] = {
  { PH_AUTH_EMAIL,	"login",   "email",	ph_email_auth,	LR_NOEMAIL },
  { PH_AUTH_PASSWORD,	"login",   "answer",	ph_passwd_auth,	LR_ERROR },
  { PH_AUTH_CLEAR,	"login",   "clear",	NULL,		LR_ERROR },
  { 0,			NULL,      NULL,	NULL,		0 }
};


void
ph_free_auth(struct ph_auth *auth)
{
	if (auth->pa_alias != NULL)
		free(auth->pa_alias);
	if (auth->pa_authkey != NULL)
		free(auth->pa_authkey);
	free(auth);
}


/*
** ph_login() - login to the PH server.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			authentication denied
*/
int
ph_login(PH *ph, char *alias, int authtype, void *auth)
{
	int i, code, authslot;
	char response[PH_BUF_SIZE];
	char challenge[PH_BUF_SIZE];
	struct ph_auth *authdata;
	ph_memblock_t *blockp;

#ifdef DEBUG
	printf("ph_login(): alias=\"%s\", authtype=%d, auth=\"%s\")\n",
	       alias, authtype, auth);
#endif

	/* find authtype entry */
	for (authslot = 0; authtypes[authslot].at_type != 0; authslot++)
		if (authtypes[authslot].at_type == authtype)
			break;
	if (authtypes[authslot].at_type == 0)
	{
		errno = EINVAL;
		return -1;
	}

	/* old alias no longer valid */
	if (ph->ph_auth != NULL)
		ph_free_auth(ph->ph_auth);
	ph->ph_auth = NULL;

	/*
	** free cached field info, since hidden fields are visible to
	** different users
	*/
	ph_free_fieldinfo(ph);

	if (ph_mmgr_malloc(ph->ph_mmgr, 0, 1, sizeof(struct ph_auth),
			   (ph_free_func_t)ph_free_auth, &blockp) == -1)
		return -1;
	authdata = (struct ph_auth *)ph_mmgr_ptr(blockp);
	authdata->pa_alias = strdup(alias);
	if (authdata->pa_alias == NULL)
		return -1;
	authdata->pa_authtype = authtype;

	/* send login command */
	if (ph_send_command(ph, "%s %s", authtypes[authslot].at_logincmd,
			    alias) == -1)
		return -1;

	/* read challenge */
	do
	{
		if (ph_get_response(ph, &code, challenge,
				    sizeof(challenge)) == -1)
			return -1;
	}
	while (code < LR_OK);
	if (code != LR_LOGIN)
	{
		errno = EINVAL;
		return -1;
	}

	/* determine response based on auth method */
	if (authtypes[authslot].at_authfunc != NULL)
	{
		if ((*(authtypes[authslot].at_authfunc))(ph, authdata, auth,
							 challenge, response,
							 sizeof(response)) == -1)
			return -1;
	}
	else
		strlcpy(response, auth, sizeof(response));

	/* send response */
	i = ph_send_command(ph, "%s %s", authtypes[authslot].at_passcmd,
			    response);
	if (i == -1)
		return -1;
	do
	{
		if (ph_get_response(ph, &code, NULL, 0) == -1)
			return -1;
	}
	while (code < LR_OK);

	/* successful login or login failed */
	if (code == LR_OK)
	{
		ph->ph_auth = authdata;
		ph_mmgr_free(blockp, 0);
		return 0;
	}

	if (code == authtypes[authslot].at_denycode)
		return PH_ERR_NOTLOG;

	if (code == LR_TEMP)
		errno = EAGAIN;
	else
		errno = EINVAL;
	return -1;
}


/* plugin function for email authentication */
static int
ph_email_auth(PH *ph, struct ph_auth *auth, char *password, char *challenge,
	      char *response, size_t responselen)
{
	struct passwd *pw;
#ifdef HAVE_GETPWUID_R
	struct passwd pent;
	size_t bufsize;
	ph_memblock_t *blockp;

	bufsize = (size_t)sysconf(_SC_GETPW_R_SIZE_MAX);

	if (ph_mmgr_malloc(ph->ph_mmgr, 0, 1, bufsize, free, &blockp) == -1)
		return -1;

	getpwuid_r(getuid(), &pent, ph_mmgr_ptr(blockp), bufsize, &pw);
#else /* ! HAVE_GETPWUID_R */
	pw = getpwuid(getuid());
#endif /* HAVE_GETPWUID_R */
	if (pw == NULL)
	{
#ifdef HAVE_GETPWUID_R
		ph_mmgr_free(blockp, 1);
#endif /* HAVE_GETPWUID_R */
		return -1;
	}

	strlcpy(response, pw->pw_name, responselen);

#ifdef HAVE_GETPWUID_R
	ph_mmgr_free(blockp, 1);
#endif /* HAVE_GETPWUID_R */

	return 0;
}


/* structure and definitions used for encrypted password authentication */
#define _PH_PWD_ROTORSZ         256
#define _PH_PWD_MASK            0377
struct ph_pwcrypt
{
	int n1, n2;
	char t1[_PH_PWD_ROTORSZ], t2[_PH_PWD_ROTORSZ], t3[_PH_PWD_ROTORSZ];
};


/* plugin function for encrypted password authentication */
static int
ph_passwd_auth(PH *ph, struct ph_auth *auth, char *password, char *challenge,
	       char *response, size_t responselen)
{
	int i, k, ic, seed, temp;
	unsigned random;
	char buf[13], scratch[PH_BUF_SIZE];
	char *p, *q;
	struct ph_pwcrypt *php;

#ifdef DEBUG
	printf("ph_passwd_crypt(): challenge=\"%s\", password=\"%s\"\n",
	       challenge, (password ? password : "NULL"));
#endif

	if (auth->pa_authkey == NULL)
	{
		auth->pa_authkey = (void *)malloc(sizeof(struct ph_pwcrypt));
		if (auth->pa_authkey == NULL)
			return -1;
	}

	php = (struct ph_pwcrypt *)auth->pa_authkey;

	if (password != NULL)
	{
		/* initialize arrays */
		memset(auth->pa_authkey, 0, sizeof(struct ph_pwcrypt));
		strncpy(buf, crypt(password, password), 13);
		seed = 123;
		for (i = 0; i < 13; i++)
			seed = seed * buf[i] + i;
		for (i = 0; i < _PH_PWD_ROTORSZ; i++)
			php->t1[i] = i;
		for (i = 0; i < _PH_PWD_ROTORSZ; i++)
		{
			seed = 5 * seed + buf[i % 13];
			random = seed % 65521;
			k = _PH_PWD_ROTORSZ - 1 - i;
			ic = (random & _PH_PWD_MASK) % (k + 1);
			random >>= 8;
			temp = php->t1[k];
			php->t1[k] = php->t1[ic];
			php->t1[ic] = temp;
			if (php->t3[k] != 0)
				continue;
			ic = (random & _PH_PWD_MASK) % k;
			while (php->t3[ic] != 0)
				ic = (ic + 1) % k;
			php->t3[k] = ic;
			php->t3[ic] = k;
		}
		for (i = 0; i < _PH_PWD_ROTORSZ; i++)
			php->t2[php->t1[i] & _PH_PWD_MASK] = i;
	}

	/* decode challenge into scratch */
	for (p = challenge, q = scratch;
	     (q - scratch < sizeof(scratch)) && *p;
	     p++)
	{
		*q++ = php->t2[(php->t3[(php->t1[(*p + php->n1) & _PH_PWD_MASK] + php->n2) & _PH_PWD_MASK] - php->n2) & _PH_PWD_MASK] - php->n1;
		php->n1++;
		if (php->n1 == _PH_PWD_ROTORSZ)
		{
			php->n1 = 0;
			php->n2++;
			if (php->n2 == _PH_PWD_ROTORSZ)
				php->n2 = 0;
		}
	}

	/* encode as ASCII into response buffer */
	k = q - scratch;
	p = response;
	q = scratch;
	*p++ = (k & 077) + '#';
	for (; (p - response < responselen) && (q - scratch < k); q += 3)
	{
		*p++ = ((*q >> 2) & 077) + '#';
		*p++ = ((((*q << 4) & 060) | ((q[1] >> 4) & 017)) & 077) + '#';
		*p++ = ((((q[1] << 2) & 074) | ((q[2] >> 6) & 03)) & 077) + '#';
		*p++ = ((q[2] & 077) & 077) + '#';
	}
	*p = '\0';

	return 0;
}


/*
** ph_logout() - logout of the PH server.
** returns:
**      0                               success
**      -1                              error (sets errno)
**      PH_ERR_NOTLOG                       not logged in
*/
int
ph_logout(PH *ph)
{
	int code;

	if (ph->ph_auth == NULL)
		return PH_ERR_NOTLOG;

	/* old alias now invalid */
	ph_free_auth(ph->ph_auth);
	ph->ph_auth = NULL;

	/*
	** free cached field info, since hidden fields are visible to
	** different users
	*/
	ph_free_fieldinfo(ph);

	if (ph_send_command(ph, "logout") == -1)
		return -1;

	do
	{
		if (ph_get_response(ph, &code, NULL, 0) == -1)
			return -1;
	}
	while (code < LR_OK);

	if (code == LR_ERROR)
		return PH_ERR_NOTLOG;
	if (code == LR_OK)
		return 0;

	errno = EINVAL;
	return -1;
}


/*
** ph_suser() - assume another user's identity (heros only).
** returns:
**      0                               success
**      -1                              error (sets errno)
**      PH_ERR_NOTHERO                      not a hero
**      PH_ERR_NOTLOG                       not logged in
*/
int
ph_suser(PH *ph, char *alias)
{
	int code;

	if (ph->ph_auth == NULL)
		return PH_ERR_NOTLOG;

	if (ph_send_command(ph, "suser %s", alias) == -1)
		return -1;

	do
	{
		if (ph_get_response(ph, &code, NULL, 0) == -1)
			return -1;
	}
	while (code < LR_OK);

	if (code == LR_AUTH)
		return PH_ERR_NOTHERO;
	if (code == LR_OK)
	{
		/*
		** free cached field info, since hidden fields are visible to
		** different users
		*/
		ph_free_fieldinfo(ph);

		if (ph->ph_auth->pa_alias != NULL)
			free(ph->ph_auth->pa_alias);
		ph->ph_auth->pa_alias = strdup(alias);
		if (ph->ph_auth->pa_alias != NULL)
			return -1;
		return 0;
	}

	errno = EINVAL;
	return -1;
}


/*
** ph_passwd() - change the current user's PH password.
** returns:
**	0				success
**	-1				error (sets errno)
**	PH_ERR_NOTLOG			not logged in
**	(any value returned by ph_change())
*/
int
ph_passwd(PH *ph, char *newpass)
{
	struct ph_fieldselector selector[2];
	struct ph_fieldselector changes[2];
	char encrypted_passwd[PH_BUF_SIZE];
	int i;

#ifdef DEBUG
	printf("==> ph_passwd(newpass=\"%s\")\n", newpass);
#endif

	/* must be logged in to change password */
	if (ph->ph_auth == NULL)
		return PH_ERR_NOTLOG;

	selector[0].pfs_field = "alias";
	selector[0].pfs_operation = '=';
	selector[0].pfs_value = ph->ph_auth->pa_alias;
	memset(&(selector[1]), 0, sizeof(struct ph_fieldselector));

	/* generate the encrypted password */
	ph_passwd_auth(ph, ph->ph_auth, NULL, newpass,
		       encrypted_passwd, sizeof(encrypted_passwd));
#ifdef DEBUG
	printf("encrypted_passwd = \"%s\"\n", encrypted_passwd);
#endif
	changes[0].pfs_field = "password";
	changes[0].pfs_operation = '=';
	changes[0].pfs_value = encrypted_passwd;
	memset(&(changes[1]), 0, sizeof(struct ph_fieldselector));

#ifdef DEBUG
	puts("calling ph_change()...");
#endif

	i = ph_change(ph, selector, changes, 0);
	if (i == 1)
		return 0;

	return i;
}


/*
** ph_whoami() - determine the currently logged-in alias
** returns:
**      pointer to the current alias    success
**      NULL                            not logged in
*/
char *
ph_whoami(PH *ph)
{
	return (ph->ph_auth != NULL
		? ph->ph_auth->pa_alias
		: NULL);
}


