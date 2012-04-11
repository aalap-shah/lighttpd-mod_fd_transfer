/*
 * Copyright (c) 2011, Motorola Mobility, Inc
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *  - Neither the name of Motorola Mobility nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include "stratus.h"

#define LOG() printf("%s:: %s :: %d\n", __FILE__, __FUNCTION__, __LINE__);

GMainLoop *loop = NULL;
StratusServer *server = NULL;

gint main(gint argc, gchar **argv)
{
	printf("Usage: static-file-fd-server [socket_path]\n This server first checks if the socket path is provided in parameters or not if not provided then it trys to look for SOCKET_FD environment variable and tries to directly start reading FD transfer messages\n This test example can be used with lighttpd's mod_fd_transfer\nJust add this configuration to lighttpd's conf file and you are good to go.\nfd_transfer.services = (\"/userdata/\" => (\"socket\" => \"/tmp/fdt-test.socket\",\"bin-path\" => \"/usr/local/bin/static-file-fd-server\"))\n");
	g_type_init();
	loop = g_main_loop_new(NULL, FALSE);

	if(argc == 2)
	{
		gchar *socket_path = g_strdup(argv[1]);
		printf("socket_path %s\n", socket_path);
		server = stratus_create_unix_server(socket_path);
	}
	else
	{
		gchar *temp = g_strdup(g_getenv("SOCKET_FD"));
		if(temp != NULL)
		{
			gint fd = g_strtod(temp, NULL);
			server = stratus_create_unix_server_with_fd(fd);
		}
		else
		{
			gchar *socket_path = g_strdup("/tmp/fdt-test.socket");
			server = stratus_create_unix_server(socket_path);
		}
	}
	if(server)
	{
		stratus_server_add_handler (server, CONN_PROTO_HTTP, stratus_static_file_handler, server);
	}
	else
	{
		printf("Server could not be created check stratus log files\n");
	}
	g_main_loop_run(loop);
	g_free(server);
	return 0;
}
