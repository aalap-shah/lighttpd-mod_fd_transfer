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

static int counter = 0;
GMainLoop *loop = NULL;
GSList *connections_list = NULL;
StratusServer *server = NULL;

gboolean dumb_increment_handler(gpointer data)
{
	gint *conn_id = (gint*)data;
	gchar *temp = g_strdup_printf("%d",  ++counter);
	gchar *temp1 = g_strdup_printf("%c%s%c", 0x00, temp, 0xff);
	if(stratus_connection_write(server , *conn_id, temp, strlen(temp)) < strlen(temp))
	{
		printf("connection closed ...\n");
		g_free(temp);
		g_free(temp1);
		return FALSE;
	}
	g_free(temp);
	g_free(temp1);
	return TRUE;
}

void stratus_ws_handler_lmp(gint connection_id, STRATUS_CONN_STATE state, Request *request, ServerPacket *body, gpointer userdata)
{
	LOG()
	StratusServer *server = (StratusServer*)userdata;
	gint *conn_id2 = NULL;
	if(state == CONN_STATE_ESTABLISHED)
	{
		GSList *list = NULL, *list1 = NULL;
		for(list = connections_list; list ; list = list->next)
		{
		LOG()
			conn_id2 = (gint*)list->data;
			if(stratus_connection_write(server, *conn_id2, body->buffer, body->size) == 0)
			{
				list1 = list;
				printf("connection closed ...Need to remove this guy from list\n");
			}
		}
		connections_list = g_slist_delete_link(connections_list, list1);
	}
	else if(state == CONN_STATE_CLOSED)
	{
		stratus_connection_close(server, connection_id);
	}
	LOG()
}

void stratus_ws_handler_dip(gint connection_id, STRATUS_CONN_STATE state, Request *request, ServerPacket *body, gpointer userdata)
{
	LOG()
	StratusServer *server = (StratusServer*)userdata;
	if(state == CONN_STATE_ESTABLISHED)
	{
		counter = 0;
	}
	else
	{
		stratus_connection_close(server, connection_id);
	}
	LOG()
}

void stratus_ws_handler(gint connection_id, STRATUS_CONN_STATE state, Request *request, ServerPacket *body, gpointer userdata)
{
	LOG()
	StratusServer *server = (StratusServer*)userdata;
	gint *conn_id = NULL;
	conn_id = (gint*)g_malloc0(sizeof(gint));
	*conn_id = connection_id;

	if(state == CONN_STATE_ESTABLISHED)
	{
		StratusWebSocketHeader *ws_header = request->header.ws_header;
		if(g_strcmp0(ws_header->tokens[TOKEN_PROTOCOL].buffer, "dumb-increment-protocol") == 0)
		{
			LOG()
			stratus_server_add_connection_handler (server, connection_id, stratus_ws_handler_dip, server);
			g_timeout_add(300, dumb_increment_handler, conn_id);
		}
		else if(g_strcmp0(ws_header->tokens[TOKEN_PROTOCOL].buffer, "lws-mirror-protocol") == 0)
		{
		LOG()
			connections_list = g_slist_append(connections_list, conn_id);
			stratus_server_add_connection_handler (server, connection_id, stratus_ws_handler_lmp, server);
		}
	}
	else if(state == CONN_STATE_CLOSED)
	{
		stratus_connection_close(server, connection_id);
	}
	LOG()
}

gint main(gint argc, gchar **argv)
{
	LOG()

	printf("Usage: websocket-test-server [port]\nAfter the server starts you can load this html file in chrome browser to see the websocket example. Currently we support only 2 websocket procotols 'dumb increment' and 'lws mirror'. \nLoad http://localhost:port/usr/local/share/websocket-test.html\n");

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
		stratus_server_add_handler (server, CONN_PROTO_WS, stratus_ws_handler, server);
	}
	LOG()
	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
	g_free(server);
	return 0;
}
