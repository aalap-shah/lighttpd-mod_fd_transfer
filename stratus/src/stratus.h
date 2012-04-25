/*
 * Copyright (c) 2011, Motorola Mobility, Inc
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *  - Neither the name of Motorola Mobility nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */

#ifndef STRATUS_H
#define STRATUS_H

#include <glib.h>

typedef struct _StratusServerConnection StratusServerConnection;
typedef struct _StratusServer StratusServer;

typedef enum _STRATUS_CONN_PROTOCOLS {
	CONN_PROTO_UNKNOWN,
	CONN_PROTO_HTTP,
	CONN_PROTO_WS,
	CONN_PROTO_HTTPS,
	CONN_PROTO_WSS,
	CONN_PROTO_MAX
}STRATUS_CONN_PROTOCOLS;

typedef enum _STRATUS_HTTP_METHOD {
	HTTP_METHOD_UNKNOWN,
	HTTP_METHOD_GET,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_DELETE,
	HTTP_METHOD_OPTIONS,
	HTTP_METHOD_MAX
}STRATUS_HTTP_METHOD;

typedef enum _STRATUS_TOKEN_INDICES {
	TOKEN_UNKNOWN,
	TOKEN_KEY,
	TOKEN_KEY1,
	TOKEN_KEY2,
	TOKEN_PROTOCOL,
	TOKEN_DRAFT,
	TOKEN_CHALLENGE,
	TOKEN_COUNT
}STRATUS_TOKEN_INDICES;

typedef enum _STRATUS_CONN_STATE{
	CONN_STATE_UNKNOWN,
	CONN_STATE_HTTP_HEADERS,
	CONN_STATE_ESTABLISHED,
	CONN_STATE_WRITE_HEADERS,
	CONN_STATE_WRITE,
	CONN_STATE_CLOSED,
	CONN_STATE_ERROR
}STRATUS_CONN_STATE;

typedef struct _ServerPacket{
		gchar *buffer;
		gssize size;
}ServerPacket;

typedef struct _ResponsePacket{
		gchar *buffer;
		gssize size;
		gssize send_size;
}ResponsePacket;

typedef struct _StratusWebSocketHeader
{
	gint handshake_protocol_id; /** It could be 76 which is the one with secure keys and 75 which is the one without any keys*/
	ServerPacket tokens[TOKEN_COUNT];
}StratusWebSocketHeader;

typedef struct _StratusHttpHeader
{
	GHashTable *headers;
}StratusHttpHeader;

typedef struct {
	/** HEADER */
	gchar *uri;
	STRATUS_HTTP_METHOD  http_method;
	gchar *host;
	gchar *origin;
	gchar *connection;
	gchar *upgrade;
	gchar *content_type;
	guint content_length;

	union {
		StratusWebSocketHeader *ws_header;
		StratusHttpHeader *http_header;
	}header;
} Request;

typedef void (*StratusServerCallBack)(gint connection_id,  STRATUS_CONN_STATE state, Request *request, ServerPacket *body, gpointer userdata);

StratusServer* stratus_create_server(guint port);

StratusServer* stratus_create_unix_server(gchar *socket_path);

StratusServer* stratus_create_unix_server_with_fd(gint fd);

void stratus_server_add_handler (StratusServer* server, STRATUS_CONN_PROTOCOLS protocol, StratusServerCallBack callback, gpointer userdata);

void stratus_server_add_connection_handler (StratusServer* server, gint connection_id, StratusServerCallBack callback, gpointer userdata);

gint stratus_connection_write_headers (StratusServer *server, gint connection_id, gint http_code, gchar **headers);

gint stratus_connection_write (StratusServer *server, gint connection_id, gchar *data, gsize size);

gint stratus_connection_write_printf (StratusServer *server, gint connection_id, const gchar *format, ...);

void stratus_connection_close (StratusServer *server, gint connection_id);

/** ******************************* Extra Utility Functions Exposed *****************************/
void stratus_static_file_handler(gint connection_id, STRATUS_CONN_STATE state, Request *request, ServerPacket *body, gpointer userdata);

#endif
