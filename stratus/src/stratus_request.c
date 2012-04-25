/*
 * Copyright (c) 2011, Motorola Mobility, Inc
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *  - Neither the name of Motorola Mobility nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */

#include "stratus.h"
#include "stratus_server.h"
#include "stratus_logging.h"


#ifdef LOGGING_FILE
/** Logging file pointer*/
extern FILE *fp;
#endif


#if 0
GET / HTTP/1.1
Upgrade: WebSocket
Connection: Upgrade
Host: 127.0.0.1:8888
Origin: http://127.0.0.1:8888
Sec-WebSocket-Protocol: dumb-increment-protocol
Sec-WebSocket-Key1: 1229  30[8   41   6
Sec-WebSocket-Key2: 1  3TB 6 8& 7957[5g0 O
#endif

gint stratus_get_fd(int sock, gint *fd, void *data, gint data_length)
{
	struct msghdr msghdr;
	struct iovec iovec_ptr;
	struct cmsghdr *cmsg;
	gchar cntrl_buff[1024];
	
	iovec_ptr.iov_base = data;
	iovec_ptr.iov_len = data_length;
	msghdr.msg_name = NULL;
	msghdr.msg_namelen = 0;
	msghdr.msg_iov = &iovec_ptr;
	msghdr.msg_iovlen = 1;
	msghdr.msg_flags = 0;
	msghdr.msg_control = cntrl_buff;
	msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);
	cmsg = CMSG_FIRSTHDR(&msghdr);
	cmsg->cmsg_len = msghdr.msg_controllen;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	((gint *)CMSG_DATA(cmsg))[0] = -1;
	
	if(recvmsg(sock, &msghdr, 0) < 0)
	{
		printf("msghdr error flag %x error %d error msg %s\n", msghdr.msg_flags, errno, strerror(errno));
		return -1;
	}
	*fd = ((gint *)CMSG_DATA(cmsg))[0];
	STRATUS_LOG_V("fd received %d\n", *fd);
	return 0;
}

void stratus_parse_request_header(StratusServer *server, StratusServerConnection *connection)
{
	STRATUS_LOG("{");
	gchar **msg = NULL, **tokens = NULL, **keys = NULL, **values = NULL, *temp_key = NULL, *temp_value = NULL;
	gint i = 0, key_id = 0;

	msg = g_strsplit(connection->data->buffer, "\r\n\r\n", -1);
	if(msg && msg[0])
	{
		/** Msg[0] will represent the header of the request. msg[1] would represent the 8 byte data in case of websocket*/
		connection->request = (Request*)g_malloc0(sizeof(Request));
		connection->header = (ServerPacket*)g_malloc0(sizeof(ServerPacket));
		connection->header->buffer = g_strdup(msg[0]);
		connection->header->size = strlen(msg[0]);

		keys = g_strsplit(msg[0], "\r\n", -1);
		if(keys)
		{
			if(keys[0])
			{
//				STRATUS_LOG_V("keys [0] %s\n", keys[0]);
				tokens = g_strsplit(keys[0], " ", -1);
				if(tokens && tokens[0] && tokens[1])
				{
//					STRATUS_LOG_V("tokens [0] %s\n", tokens[0]);
					if(g_strcmp0(tokens[0], "GET") == 0)
						connection->request->http_method = HTTP_METHOD_GET;
					else if(g_strcmp0(tokens[0], "POST") == 0)
						connection->request->http_method = HTTP_METHOD_POST;
					else if(g_strcmp0(tokens[0], "PUT") == 0)
						connection->request->http_method = HTTP_METHOD_PUT;
					else if(g_strcmp0(tokens[0], "DELETE") == 0)
						connection->request->http_method = HTTP_METHOD_DELETE;
					else if(g_strcmp0(tokens[0], "OPTIONS") == 0)
						connection->request->http_method = HTTP_METHOD_OPTIONS;
					else
						connection->request->http_method = HTTP_METHOD_UNKNOWN;
	
					connection->request->uri = g_strdup(tokens[1]);
					g_strfreev(tokens);
				}
				else
				{
					STRATUS_LOG("Request URL itself cant be empty");
					g_free(connection->request);
					connection->state = CONN_STATE_ERROR;
					return;
				}
			}

			/** By default we set the protocol to be HTTP in case of websocket it would be overwrittin by WS*/	
			connection->protocol = CONN_PROTO_HTTP;

			/** Start from 2nd row as the first row will not have any key value pairs. First row would represent the request query itself.*/
			for(i = 1; keys[i] && *(keys[i]) != '\0'; i++)
			{
//				STRATUS_LOG_V("keys [0] %s\n", keys[i]);
				values = g_strsplit(keys[i], ":", 2);
				if(values && values[0] && values[1])
				{
					if((*(values[0]) != '\0') && (g_strcmp0(g_strstrip(values[0]), "Content-Length") == 0))
					{
						connection->request->content_length = g_strtod(g_strstrip(values[1]), NULL);
					}
					else if((*(values[0]) != '\0') && (g_strcmp0(g_strstrip(values[0]), "Content-Type") == 0))
					{
						connection->request->content_type = g_strdup(g_strstrip(values[1]));
					}
					else if((*(values[0]) != '\0') && (g_strcmp0(g_strstrip(values[0]), "Origin") == 0))
					{
						connection->request->origin = g_strdup(g_strstrip(values[1]));
					}
					else if((*(values[0]) != '\0') && (g_strcmp0(g_strstrip(values[0]), "Host") == 0))
					{
						connection->request->host = g_strdup(g_strstrip(values[1]));
					}
					else if((*(values[0]) != '\0') && (g_strcmp0(g_strstrip(values[0]), "Connection") == 0))
					{
						connection->request->connection = g_strdup(g_strstrip(values[1]));
					}
					else if((*(values[0]) != '\0') && (g_strcmp0(g_strstrip(values[0]), "Upgrade") == 0))
					{
						connection->request->upgrade = g_strdup(g_strstrip(values[1]));
						if(i == 1)
						{
							/** indicates the first header hence switch the protocol to WS*/
							if(g_ascii_strcasecmp(connection->request->upgrade, "WebSocket") == 0)
							{
								connection->protocol = CONN_PROTO_WS;
							}
						}
					}
					else
					{
						if(connection->protocol == CONN_PROTO_WS)
						{
							if(connection->request->header.ws_header == NULL)
							{
								connection->request->header.ws_header = (StratusWebSocketHeader*)g_malloc0(sizeof(StratusWebSocketHeader));
							}
							temp_value = NULL;
							key_id = 0;
							if(*(values[0]) != '\0')
							{
								if(g_strcmp0(values[0],"Sec-WebSocket-Key1") == 0)
									key_id = TOKEN_KEY1;
								else if(g_strcmp0(values[0],"Sec-WebSocket-Key2") == 0)
									key_id = TOKEN_KEY2;
								else if(g_strcmp0(values[0],"Sec-WebSocket-Key") == 0)
									key_id = TOKEN_KEY;
								else if((g_strcmp0(values[0],"Sec-WebSocket-Protocol") == 0) || (g_strcmp0(values[0],"WebSocket-Protocol") == 0))
									key_id = TOKEN_PROTOCOL;
								else if(g_strcmp0(values[0],"Sec-WebSocket-Draft") == 0)
									key_id = TOKEN_DRAFT;
							}
							if(key_id > 0)
							{
								/** Specifically use g_strchung and not g_strstrip as the trailing white spaces may be part of the key*/
								if(*(values[1]) != '\0')
								{
									temp_value = g_strdup(g_strchug(values[1]));
									connection->request->header.ws_header->tokens[key_id].buffer = temp_value;
									connection->request->header.ws_header->tokens[key_id].size = strlen(temp_value);
								}
							}
						}
						else if(connection->protocol == CONN_PROTO_HTTP)
						{
							if(connection->request->header.http_header == NULL)
							{
								connection->request->header.http_header = (StratusHttpHeader*)g_malloc0(sizeof(StratusHttpHeader));
								connection->request->header.http_header->headers = g_hash_table_new(g_str_hash, g_str_equal);
							}
							temp_key = NULL;
							temp_value = NULL;
							if(*(values[0]) != '\0')
								temp_key = g_strdup(g_strstrip(values[0]));
							if(*(values[1]) != '\0')
								temp_value = g_strdup(g_strstrip(values[1]));
			
							if(temp_key)
								g_hash_table_insert(connection->request->header.http_header->headers, temp_key, temp_value);
						}
					}
					g_strfreev(values);
				}
				else
				{
					STRATUS_LOG("Request URL itself cant be empty");
					g_free(connection->request);
					connection->state = CONN_STATE_ERROR;
					return;
				}
			}
			
			/** Check if it is websocket then assign the websocket protocol*/
			if(connection->protocol == CONN_PROTO_WS)
			{
				/** Check if the "Sec-WebSocket-Key" part has come then I can decide that its a protocol version RFC 6455*/
				if(connection->request->header.ws_header->tokens[TOKEN_KEY].buffer)
				{
					connection->request->header.ws_header->handshake_protocol_id = 6455;
					connection->state = CONN_STATE_HTTP_HEADERS;
				}
				/** Check if the "Sec" part has come then I can decide that its a protocol version 76, and then only I should go ahead and try to decode the challenege otherwise just set the protocol version 75 and go ahead*/
				else if(connection->request->header.ws_header->tokens[TOKEN_KEY1].buffer && connection->request->header.ws_header->tokens[TOKEN_KEY2].buffer)
				{
					/** If All of the above keys are present then it would mean that protocol 76 could be present so check for challenege*/
					if(msg[1] && ((connection->data->size - strlen(msg[0]) - 4) == 8))
					{
						connection->request->header.ws_header->handshake_protocol_id = 76;
						/** Challenge is always 8 bytes*/
						connection->request->header.ws_header->tokens[TOKEN_CHALLENGE].buffer = (gchar*)g_malloc0(sizeof(gchar)*9);
						g_memmove(connection->request->header.ws_header->tokens[TOKEN_CHALLENGE].buffer, msg[1], 8);
						connection->request->header.ws_header->tokens[TOKEN_CHALLENGE].size = 8;
						connection->state = CONN_STATE_HTTP_HEADERS;
					}
					else
					{
						STRATUS_LOG("Error Looks like proper size header is NOT received1\n");
						g_strfreev(msg);
						return;
					}
				}
				else if(!(connection->request->header.ws_header->tokens[TOKEN_KEY1].buffer) && !(connection->request->header.ws_header->tokens[TOKEN_KEY2].buffer))
				{
					STRATUS_LOG("Protocol looks like 75\n");
					connection->request->header.ws_header->handshake_protocol_id = 75;
					connection->state = CONN_STATE_HTTP_HEADERS;
				}

				if(!connection->handler.callback)
				{
					connection->handler.callback = server->handler_list[CONN_PROTO_WS].callback;
					connection->handler.userdata = server->handler_list[CONN_PROTO_WS].userdata;
				}

				/** Remove this much data as it is processed but only if the protocol is decoded*/
				g_free(connection->data->buffer);
				connection->data->buffer = NULL;
				connection->data->size = 0;
			}
			else if(connection->protocol == CONN_PROTO_HTTP)
			{
				connection->state = CONN_STATE_HTTP_HEADERS;
				if(connection->body == NULL)
				{
					connection->body = (ServerPacket*)g_malloc0(sizeof(ServerPacket));
					connection->body->buffer = NULL;
					connection->body->size = 0;
				}
				if(msg[1] && *(msg[1]) != '\0')
				{
					connection->body->size = connection->data->size - connection->header->size - 4;
					connection->body->buffer = g_malloc0(connection->body->size + 1);
					g_memmove(connection->body->buffer, msg[1], connection->body->size);
				}

				if(connection->body->size == connection->request->content_length)
				{
					connection->state = CONN_STATE_ESTABLISHED;
				}

				if(!connection->handler.callback && server->handler_list[CONN_PROTO_HTTP].callback)
				{
					connection->handler.callback = server->handler_list[CONN_PROTO_HTTP].callback;
					connection->handler.userdata = server->handler_list[CONN_PROTO_HTTP].userdata;
				}

				/** Remove this much data as it is processed but only if the protocol is decoded*/
				g_free(connection->data->buffer);
				connection->data->buffer = NULL;
				connection->data->size = 0;
			}
		}
		g_strfreev(keys);
	}
	g_strfreev(msg);
	STRATUS_LOG("}");
}
