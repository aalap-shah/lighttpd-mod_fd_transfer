#dnl Process this file with autoconf to produce a configure script.
AC_INIT()
AM_CONFIG_HEADER(config.h)

PACKAGE=stratus
VERSION=0.1

AM_INIT_AUTOMAKE($PACKAGE,$VERSION)

AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_INSTALL
AC_PROG_LIBTOOL

PKG_CHECK_MODULES(GLIB, glib-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GIO, gio-2.0)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

PKG_CHECK_MODULES(OPENSSL, openssl)
AC_SUBST(OPENSSL_CFLAGS)
AC_SUBST(OPENSSL_LIBS)

AC_SUBST(LOGGING)
AC_ARG_ENABLE(logging,AC_HELP_STRING([--enable-logging=@<:@none/file/stdout@:>@],[specify logging level @<:@default=none@:>@]),,enable_logging="none")

if test "x$enable_logging" = "xnone"; then
	LOGGING="-DLOGGING_NONE"
elif test "x$enable_logging" = "xfile"; then
	LOGGING="-DLOGGING_FILE"
elif test "x$enable_logging" = "xstdout"; then
	LOGGING="-DLOGGING_STDOUT"
else
	LOGGING="-DLOGGING_NONE"
fi

echo $LOGGING
					    
AC_OUTPUT([
stratus.pc
Makefile
src/Makefile
test/Makefile
])
