/*
 * Copyright (c) 2011, Motorola Mobility, Inc
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *  - Neither the name of Motorola Mobility nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */

#ifndef STRATUS_SERVER_H
#define STRATUS_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <sys/un.h>
#include <sys/time.h>

#include <glib.h>
#include <gio/gio.h>
#include <glib/gprintf.h>
#include <openssl/md5.h>

#include "stratus.h"

typedef struct _StratusServerHandler {
	StratusServerCallBack callback;
	gpointer userdata;
}StratusServerHandler;

struct _StratusServerConnection
{
	gint conn_id;
	GSocket *connection_socket;
	gint connection_socket_fd;
	GIOChannel *connection_gio;
	guint watch_id;
	StratusServerHandler handler;

	STRATUS_CONN_PROTOCOLS protocol;
	ServerPacket *data;
	ServerPacket *header;
	ServerPacket *body;

	STRATUS_CONN_STATE state;

	Request *request;
	gboolean headers_sent;
	gboolean chunk_transfer;

	ResponsePacket *response;
	guint write_watch_id;
	gboolean close_called;
};

#define INTERNET_SERVER 0
#define UNIX_SERVER 1

struct _StratusServer
{
	GSocket *server_socket;
	gint server_socket_fd;
	GIOChannel *server_gio;
	gint port;
	StratusServerHandler handler_list[CONN_PROTO_MAX]; //make 4 when HTTPS and WSS works.
	/** This hash table is used for maintaining IOChannel to Connections pointers*/
	GHashTable *connections;

	/** This hash table is used for maintaining uniquie ids to connection pointers*/
	GHashTable *id_conn_hash;
	gint id_count;

	/** This flag indicates what type of server this is. It could be internet server or unix server*/
	gint server_type;
};

/** stratus_connection.c */
StratusServerConnection* stratus_server_connection_init();

gboolean stratus_connection_gio_watch_cb(GIOChannel *source, GIOCondition condition, gpointer userdata);

gboolean stratus_connection_g_io_channel_read_to_end (StratusServerConnection* connection);

void stratus_server_connection_free(StratusServerConnection* connection);

/** stratus_request.c */

gint stratus_get_fd(int sock, gint *fd, void *data, gint data_length);

void stratus_parse_request_header(StratusServer *server, StratusServerConnection *connection);

/** stratus_utility.c */

const gchar* stratus_get_mime_type_for_file(gchar *file_path);

/** stratus_handshake.c */

void stratus_ws_handshake_handler(StratusServerConnection *connection);


#endif //STRATUS_SERVER_H
