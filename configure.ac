dnl ### Normal initialization. ######################################
AC_INIT([nph], [1.2.3])
AC_PREREQ([2.57])
AC_CONFIG_AUX_DIR([autoconf])
AC_CONFIG_HEADERS([config.h])
AC_COPYRIGHT([[
Copyright (c) 2000-2004 University of Illinois Board of Trustees
Copyright (c) 2000-2004 Mark D. Roth
All rights reserved.
]])
AC_CONFIG_SRCDIR([lib/phclient.h])
ENCAP_PKG([], [postinstall-encap], [postinstall-no-encap])


dnl ### Set some option defaults. ###################################
if test -z "$CFLAGS"; then
  CFLAGS="-O"
fi
MKDIR="mkdir -p -m 755"
AC_SUBST([MKDIR])


dnl ### Check for compiler et al. ###################################
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PATH_PROG([MORE], [more])
AC_DEFINE_UNQUOTED([MORE], ["${ac_cv_path_MORE}"], [Path to more.])
AC_PATH_PROG([VI], [vi])
AC_DEFINE_UNQUOTED([VI], ["${ac_cv_path_VI}"], [Path to vi.])


dnl ### Compiler characteristics. ##################################
AC_AIX
AC_C_CONST


dnl ### Checks for header files. ###################################
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/aixrcmds.h crypt.h paths.h unistd.h sys/poll.h])
if test "$ac_cv_header_sys_poll_h" != "yes"; then
  AC_CHECK_HEADERS([sys/select.h])
fi


dnl ### Checks for needed functions. ###############################
PSG_MODULE([compat])
COMPAT_FUNC_GETHOSTBYNAME_R
COMPAT_FUNC_GETPWUID_R
COMPAT_FUNC_GETSERVBYNAME_R
AC_CHECK_FUNCS([poll sigaction])
COMPAT_FUNC_SNPRINTF
COMPAT_FUNC_STRDUP
COMPAT_FUNC_STRLCAT
COMPAT_FUNC_STRLCPY
COMPAT_FUNC_STRSEP


dnl ### Check for libraries. #######################################
AC_CHECK_LIB([socket], [socket])
AC_CHECK_LIB([nsl], [gethostbyname])
AC_CHECK_LIB([crypt], [crypt])


dnl ### Optional features. #########################################
AC_ARG_ENABLE([readline],
  [  --disable-readline      Do not use the readline library in the nph client])
if test "$enable_readline" != "no"; then
  PSG_LIB_READLINE
fi

AC_ARG_ENABLE([setuid-nph],
  [  --enable-setuid-nph     Install nph setuid-root (for email authentication)])
if test "$enable_setuid_nph" = "yes"; then
  NPH_MODE=4555
else
  NPH_MODE=755
fi
AC_SUBST([NPH_MODE])

AC_ARG_WITH([mailmatches],
  [  --with-mailmatches=LIST  Set default mailmatch fields],
  [],
  [with_mailmatches="alias:callsign"]
)
AC_DEFINE_UNQUOTED([PH_DEFAULT_MAILMATCHES], ["${with_mailmatches}"],
	[Default mailmatch fields])


dnl ### Create output files. #######################################
PSG_MODULE([buffer], [ph])
PSG_MODULE([mmgr], [ph])
AC_CONFIG_FILES([Makefile lib/Makefile nph/Makefile doc/Makefile])
AC_CONFIG_FILES([postinstall], [chmod +x postinstall])
AC_OUTPUT

