CFLAGS += $(GLIB_CFLAGS) -I../src -Wall
LDFLAGS += -export-dynamic $(GLIB_LIBS) -L../src/.libs -lstratus

bin_PROGRAMS = static-file-server websocket-test-server http-code-server static-file-fd-server

static_file_server_SOURCES = static-file-server.c
websocket_test_server_SOURCES = websocket-test-server.c
http_code_server_SOURCES = http-code-server.c
static_file_fd_server_SOURCES = static-file-fd-server.c

websockettestdir = $(datadir)
websockettest_DATA = websocket-test.html

MAINTAINERCLEANFILES =  \
        Makefile        \
        Makefile.in     \
        .deps           \
        *~              \
        svn-commit*
