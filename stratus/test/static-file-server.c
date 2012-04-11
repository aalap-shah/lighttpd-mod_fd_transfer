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
	LOG()
	/** Assigning default port*/
	gint port = 80;
	g_type_init();
	if(argc == 2)
	{
		/** Over writting the port if passed in arguments*/
		port = (gint)g_ascii_strtoll (argv[1], NULL, 10);
		printf("port identified to be %d\n", port);
	}
	server = stratus_create_server(port);
	if(server)
	{
		stratus_server_add_handler (server, CONN_PROTO_HTTP, stratus_static_file_handler, server);
	}
	LOG()
	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
	g_free(server);
	return 0;
}
