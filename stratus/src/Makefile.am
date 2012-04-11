CFLAGS += $(GLIB_CFLAGS) $(GIO_CFLAGS) $(OPENSSL_CFLAGS) $(LOGGING) -Wall -I./
LDFLAGS = -export-dynamic $(GLIB_LIBS) $(GIO_LIBS) $(OPENSSL_LIBS)

stratusincludedir=$(includedir)
stratusinclude_HEADERS = stratus.h

stratus_source_files = stratus_server.c \
			stratus_connection.c \
			stratus_request.c \
			stratus_response.c \
			stratus_handshake.c \
			stratus_utility.c

stratuslibdir = $(libdir)
stratuslib_LTLIBRARIES = libstratus.la
libstratus_la_LDFLAGS = $(LDFLAGS) -shared
libstratus_la_SOURCES = $(stratus_source_files)
libstratus_la_CFLAGS = $(CFLAGS) -fPIC

MAINTAINERCLEANFILES =	\
	Makefile	\
	Makefile.in	\
	.deps		\
	*~		\
	svn-commit*	
