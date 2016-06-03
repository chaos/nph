/*
**  Copyright 2000-2004 University of Illinois Board of Trustees
**  Copyright 2000-2004 Mark D. Roth
**  All rights reserved.
**
**  output.c - nph code to display server data
**
**  Mark D. Roth <roth@feep.net>
*/

#include <nph.h>

#include <errno.h>

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
# include <stdarg.h>
#else
# ifdef HAVE_VARARGS_H
#  include <varargs.h>
# endif
#endif

#include <strings.h>


/* values for outtype */
#define OUT_DEFAULT	0	/* stdout */
#define OUT_FILE	1	/* file redirection */
#define OUT_PIPE	2	/* pipe redirection */

/* local variables */
static FILE *outf = NULL;
static int outtype = -1;


/* output to a pipe */
int
output_pipe(char *prog)
{
	FILE *f;

	f = popen(prog, "w");
	if (f == NULL)
	{
		perror("popen()");
		return -1;
	}

	outtype = OUT_PIPE;
	outf = f;
	return 0;
}


/* send output to a file (optionally appending) */
int
output_file(char *filename, int append)
{
	FILE *f;

	f = fopen(filename, (append ? "a" : "w"));
	if (f == NULL)
	{
		perror("fopen()");
		return -1;
	}

	outtype = OUT_FILE;
	outf = f;
	return 0;
}


/* reset output file */
void
reset_output(int initialize)
{
	if (!initialize)
	{
		if (outtype == OUT_FILE)
			fclose(outf);
		else if (outtype == OUT_PIPE)
			pclose(outf);
	}

	outtype = OUT_DEFAULT;
	outf = stdout;
}


/* print a message */
int
#ifdef STDC_HEADERS
nph_printf(int err, char *fmt, ...)
#else
nph_printf(int err, char *fmt, int va_alist)
#endif
{
	size_t i = 0;
	va_list args;

	if (err)
		fprintf(outf, "nph: ");

#ifdef STDC_HEADERS
	va_start(args, fmt);
#else
	va_start(args);
#endif
	i += vfprintf(outf, fmt, args);
	va_end(args);

	return i;
}


/* print a field */
static void
print_field(struct ph_fieldvalue *field, int width)
{
	int newline_flag = 0;
	char *wordp, *nextp;
	char buf[NPH_BUF_SIZE];

	/* special cases */
	if (field->pfv_code != -(LR_OK))
	{
		if (field->pfv_code == -(LR_AINFO))
			wordp = "field suppressed";
		else if (field->pfv_code == -(LR_ABSENT))
			wordp = "field not present";
		else if (field->pfv_code == -(LR_ISCRYPT))
			wordp = "encrypted";
		else
			wordp = "unknown response code";

		snprintf(buf, sizeof(buf), "[ %s", field->pfv_field);
		nph_printf(0, "%*s : %s ]\n", width + 2, buf, wordp);
		return;
	}

	nph_printf(0, "  %*s : ", width, field->pfv_field);
	nextp = field->pfv_value;
	while ((wordp = strsep(&nextp, "\n")) != NULL)
	{
		if (newline_flag)
			nph_printf(0, "  %*s : ", width, "");
		nph_printf(0, "%s\n", wordp);
		newline_flag = 1;
	}
}


/*
** returns 1 if one of the aliases of result_name is listed in showfields,
** 0 if not listed, or -1 on error from ph_get_fieldinfo()
*/
int
field_match(char *result_name, char *showfields[])
{
	struct ph_fieldinfo *phinfo = NULL;
	int i, j;

	/* check for no specified return fields or for "all" */
	if (showfields == NULL)
		return 1;
	for (i = 0; showfields[i] != NULL; i++)
		if (strcasecmp(showfields[i], "all") == 0)
			return 1;

	/* get info for current field */
	if (ph_get_fieldinfo(ph, result_name, &phinfo) == -1)
	{
		nph_printf(1, "ph_get_fieldinfo(): %s\n", strerror(errno));
		return -1;
	}

	/* check against all aliases */
	for (i = 0; showfields[i] != NULL; i++)
	{
		/*
		** if ph_get_fieldinfo() returned PH_ERR_DATAERR, phinfo will
		** be NULL.  this happens for hidden fields, since they don't
		** show up in the "fields" output, so we just compare to the
		** name specified by the user.
		*/
		if (phinfo == NULL)
		{
			if (strcasecmp(showfields[i], result_name) == 0)
				return 1;
			continue;
		}

		for (j = 0; phinfo->pf_fnames[j] != NULL; j++)
			if (strcasecmp(showfields[i],
				       phinfo->pf_fnames[j]) == 0)
				return 1;
	}

	/* no match */
	return 0;
}


/* print an entry */
void
print_entry(ph_entry entry, char *showfields[], int show_redirection)
{
	int i, j;
	int alias = -1, width = 16;
	char *advertised_email, *advertised_www;

	/* determine the maximum field width */
	for (j = 0; entry[j].pfv_field != NULL; j++)
	{
		/*
		** FIXME: we should really call ph_get_fieldinfo() and check
		** the length of all field aliases...
		*/
		i = strlen(entry[j].pfv_field);
		if (i > width)
			width = i;
	}

	/* loop through the fields in the entry */
	for (j = 0; entry[j].pfv_field != NULL; j++)
	{
		/* save index of "alias" field */
		if (show_redirection
		    && strcasecmp(entry[j].pfv_field, "alias") == 0)
			alias = j;

		/* check if we should print this entry */
		i = field_match(entry[j].pfv_field, showfields);
		if (i == -1)
			return;
		if (i == 0)
			continue;

		/*
		** if it's the email or www field and show_redirection is set,
		** show the PH-redirected value
		*/
		if (show_redirection)
		{
			if (strcasecmp(entry[j].pfv_field, "email") == 0)
			{
				/* make sure we know the alias */
				if (alias == -1)
				{
					for (alias = j + 1;
					     entry[alias].pfv_field;
					     alias++)
						if (strcasecmp(entry[alias].pfv_field,
							       "alias") == 0)
							break;

					/*
					** can't find alias field,
					** print normally
					*/
					if (entry[alias].pfv_field == NULL)
					{
						alias = -1;
						print_field(&entry[j], width);
						continue;
					}
				}

				if (ph_advertised_email(ph,
							entry[alias].pfv_value,
							0, &advertised_email) == -1)
				{
					nph_printf(1, "ph_advertised_email(): "
						   "%s\n", strerror(errno));
					return;
				}
				nph_printf(0, "  %*s : %s\n", width, "email",
					   advertised_email);
				free(advertised_email);
				continue;
			}

			if (strcasecmp(entry[j].pfv_field, "www") == 0)
			{
				/* make sure we know the alias */
				if (alias == -1)
				{
					for (alias = j + 1;
					     entry[alias].pfv_field;
					     alias++)
						if (strcasecmp(entry[alias].pfv_field,
							       "alias") == 0)
							break;

					/*
					** can't find alias field,
					** print normally
					*/
					if (entry[alias].pfv_field == NULL)
					{
						alias = -1;
						print_field(&entry[j], width);
						continue;
					}
				}

				if (ph_advertised_www(ph,
						      entry[alias].pfv_value,
						      0, &advertised_www) != -1)
				{
					nph_printf(0, "  %*s : %s\n", width,
						   "www", advertised_www);
					free(advertised_www);
					continue;
				}

				/*
				** if ph_advertised_www() returns an error,
				** fall through
				*/
			}
		}

		/* otherwise (show_redirection not set), print normally */
		print_field(&entry[j], width);
	}
}


void
send_debug(void *dummy, char *text)
{
	nph_printf(0, ">>> %s\n", text);
}


void
recv_debug(void *dummy, char *text)
{
	nph_printf(0, "<<< %s\n", text);
}


