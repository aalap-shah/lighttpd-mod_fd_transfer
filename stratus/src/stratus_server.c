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
FILE *fp = NULL;
#endif

static gboolean stratus_server_gio_watch_cb (GIOChannel *source, GIOCondition condition, gpointer userdata)
{
	STRATUS_LOG("{");
	StratusServer *server = (StratusServer*) userdata;
	GError *error = NULL;
	GIOStatus status = 0;
	
	StratusServerConnection *connection = stratus_server_connection_init();

	connection->connection_socket = g_socket_accept(server->server_socket, NULL, &error);
	if(!connection->connection_socket)
	{
		STRATUS_LOG_V("Error While accepting a connection Error = %s\n", error->message);
		g_free(connection);
		return TRUE;
	}

	connection->connection_socket_fd = g_socket_get_fd(connection->connection_socket);
	connection->connection_gio = NULL;
	connection->connection_gio = g_io_channel_unix_new(connection->connection_socket_fd);

	status = g_io_channel_set_encoding(connection->connection_gio, NULL, &error);
	if(status != G_IO_STATUS_NORMAL)
	{
		if(status == G_IO_STATUS_ERROR)
		{
			STRATUS_LOG_V("Error in setting the encoding of the channel error = %s\n", error->message);
		}
		else
		{
			STRATUS_LOG_V("Error in setting the encoding of the channel status %d\n", status);
		}
		return TRUE;
	}

	/** IO_IN is for input to read , IO_PRI - priority data to read , IO_HUP - hung up if socket got closed*/
	connection->watch_id = g_io_add_watch(connection->connection_gio, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_ERR|G_IO_NVAL, stratus_connection_gio_watch_cb, server);

	/** Add the GIOChannel to the hash table so that we can receive the structure in io_channel callback*/
	g_hash_table_insert(server->connections, connection->connection_gio, connection);
	/** Add the GIOChannel to the hash table so that we can receive the structure in io_channel callback*/
	connection->conn_id = server->id_count++;
	g_hash_table_insert(server->id_conn_hash, (gpointer)&(connection->conn_id), connection);

	STRATUS_LOG_V("Connection socket fd = %d gio channel pointer address = %p conn_id %d\n", connection->connection_socket_fd, connection->connection_gio, connection->conn_id);

	STRATUS_LOG("}");
	return TRUE;
}

static gboolean stratus_unix_server_accept_gio_watch_cb (GIOChannel *source, GIOCondition condition, gpointer userdata)
{
	STRATUS_LOG("{");
	StratusServer *server = (StratusServer*) userdata;
	GError *error = NULL;
	GIOStatus status = 0;

	gint fd;
	gchar *buff = (gchar *)g_malloc0(4096);

	status = stratus_get_fd(server->server_socket_fd, &fd, buff, 4095);
	if(status == -1)
	{
		return TRUE;
	}
	StratusServerConnection *connection = stratus_server_connection_init();
	STRATUS_LOG_V("buffer received - %s\n", buff);

	connection->data->buffer = buff;
	connection->data->size = strlen(buff);

	connection->connection_socket_fd = fd;
	connection->connection_gio = NULL;
	connection->connection_gio = g_io_channel_unix_new(fd);

	status = g_io_channel_set_encoding(connection->connection_gio, NULL, &error);
	if(status != G_IO_STATUS_NORMAL)
	{
		if(status == G_IO_STATUS_ERROR)
		{
			STRATUS_LOG_V("Error in setting the encoding of the channel error = %s\n", error->message);
		}
		else
		{
			STRATUS_LOG_V("Error in setting the encoding of the channel status %d\n", status);
		}
		return TRUE;
	}

	/** IO_IN is for input to read , IO_PRI - priority data to read , IO_HUP - hung up if socket got closed*/
	connection->watch_id = g_io_add_watch(connection->connection_gio, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_ERR|G_IO_NVAL, stratus_connection_gio_watch_cb, server);

	/** Add the GIOChannel to the hash table so that we can receive the structure in io_channel callback*/
	g_hash_table_insert(server->connections, connection->connection_gio, connection);
	/** Add the GIOChannel to the hash table so that we can receive the structure in io_channel callback*/
	connection->conn_id = server->id_count++;
	g_hash_table_insert(server->id_conn_hash, (gpointer)&(connection->conn_id), connection);

	STRATUS_LOG_V("Connection socket fd = %d gio channel pointer address = %p conn_id %d\n", connection->connection_socket_fd, connection->connection_gio, connection->conn_id);

	stratus_connection_gio_watch_cb(connection->connection_gio, G_IO_IN, server);

	STRATUS_LOG("}");
	return TRUE;
}

static gboolean stratus_unix_server_gio_watch_cb (GIOChannel *source, GIOCondition condition, gpointer userdata)
{
	STRATUS_LOG("{");
	StratusServer *server = (StratusServer*) userdata;
	GError *error = NULL;
	server->server_socket_fd = accept(server->server_socket_fd, NULL, 0);
	STRATUS_LOG_V("new server->server_socket_fd %d\n", server->server_socket_fd);
	if(server->server_socket_fd == -1)
	{
		STRATUS_LOG_V("new server->server_socket_fd error %s\n", strerror(errno));
	}
	/** currently keep the accpet immediately later on we will have that in idle loop with giochannel. Also currently using native accept as the g_socket_accept didnt work with fd transfer*/
	/** I guess we can over ride the server_socket_fd*/

	/** Not sure if we should re-fetch the socket from the fd */
#if 1
	server->server_socket = g_socket_new_from_fd(server->server_socket_fd, &error);
	if(server->server_socket == NULL)
	{
		STRATUS_LOG_V("Error while fetching the GSocket back from fd error %s\n", error->message);
	}
#endif
	server->server_gio = g_io_channel_unix_new(server->server_socket_fd);
	g_io_add_watch (server->server_gio, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_ERR|G_IO_NVAL, stratus_unix_server_accept_gio_watch_cb, server);

	return FALSE;
}

/** *******************************Exposed Main APIs *********************************************/

void stratus_server_add_handler (StratusServer* server, STRATUS_CONN_PROTOCOLS protocol, StratusServerCallBack callback, gpointer userdata)
{
	STRATUS_LOG("{");
	if(server == NULL || protocol < 0 || protocol > CONN_PROTO_MAX)
	{
		STRATUS_LOG("Invalid Server or Invalid Protocol\n");
		return;
	}
	server->handler_list[protocol].callback = callback;
	server->handler_list[protocol].userdata = userdata;
	STRATUS_LOG("}");
}

StratusServer* stratus_create_server(guint port)
{
	STRATUS_LOG("{");
	StratusServer *server = (StratusServer*)g_malloc0(sizeof(StratusServer));
	GSocketAddress *address = NULL;
	GError *error = NULL;
	gboolean status = FALSE;
	struct sockaddr_in serv_addr;
	
	server->server_socket = NULL;
	server->server_socket = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, 0, &error);

	STRATUS_LOG("Server Socket Created\n");
	if(!(server->server_socket))
		STRATUS_LOG_V("Server Socket Creation failed error %s\n", error->message);
		
	server->connections = g_hash_table_new(g_direct_hash, g_direct_equal);
	server->id_conn_hash = g_hash_table_new(g_int_hash, g_int_equal);
	server->id_count = 0;
	server->port = port;
	server->server_type = INTERNET_SERVER;
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	/** FIXME: Do we need to free the address struct ?*/
	address = g_socket_address_new_from_native ((gpointer)&serv_addr,sizeof(serv_addr));

	status = g_socket_bind (server->server_socket, address, TRUE, &error);
	STRATUS_LOG("Server Socket bind to the address\n");
	if(status)
	{
		status = g_socket_listen (server->server_socket, &error);
		STRATUS_LOG("Server Socket Starting to listen\n");
		if(status)
		{
			STRATUS_LOG("Converted to GIOChannel for easy accessing\n");
			server->server_socket_fd = g_socket_get_fd(server->server_socket);
			server->server_gio = g_io_channel_unix_new(server->server_socket_fd);
			g_io_add_watch (server->server_gio, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_ERR|G_IO_NVAL, stratus_server_gio_watch_cb, server);
		}
		else
		{
			STRATUS_LOG_V("Socket listen Error = %s\n", error->message);
			g_free(server);
			server = NULL;
		}
	}
	else
	{
		STRATUS_LOG_V("Socket Bind Error = %s\n", error->message);
		g_free(server);
		server = NULL;
	}
	STRATUS_LOG("}");
	return server;
}

StratusServer* stratus_create_unix_server(gchar *socket_path)
{
	STRATUS_LOG("{");
	StratusServer *server = (StratusServer*)g_malloc0(sizeof(StratusServer));
	GSocketAddress *address = NULL;
	GError *error = NULL;
	gboolean status = FALSE;
	struct sockaddr_un serv_addr;

	server->server_socket = NULL;
	server->server_socket = g_socket_new (G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, 0, &error);
	STRATUS_LOG("Server Socket Created\n");
	if(!(server->server_socket))
		STRATUS_LOG_V("Server Socket Creation failed error %s\n", error->message);
		
	server->connections = g_hash_table_new(g_direct_hash, g_direct_equal);
	server->id_conn_hash = g_hash_table_new(g_int_hash, g_int_equal);
	server->id_count = 0;
	server->server_type = UNIX_SERVER;

	memset(&serv_addr, 0, sizeof(struct sockaddr_un));
			/* Clear structure */
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, socket_path, strlen(socket_path));

	/** FIXME: Do we need to free the address struct ?*/
	address = g_socket_address_new_from_native ((gpointer)&serv_addr,sizeof(serv_addr));

	status = g_socket_bind (server->server_socket, address, TRUE, &error);
	STRATUS_LOG("Server Socket bind to the address\n");
	if(status)
	{
		status = g_socket_listen (server->server_socket, &error);
		STRATUS_LOG("Server Socket Starting to listen\n");
		if(status)
		{
			STRATUS_LOG("Converted to GIOChannel for easy accessing\n");
			server->server_socket_fd = g_socket_get_fd(server->server_socket);

			/** currently keep the accpet immediately later on we will have that in idle loop with giochannel. Also currently using native accept as the g_socket_accept didnt work with fd transfer. We eventually decided to keep the accept call in idle otherwise it gets blocked at accept*/

			/** Not sure if we should re-fetch the socket from the fd */
#if 0
			server->server_socket = g_socket_new_from_fd(server->server_socket_fd, &error);
			if(server->server_socket == NULL)
			{
				STRATUS_LOG_V("Error while fetching the GSocket back from fd error %s\n", error->message);
			}
#endif
			server->server_gio = g_io_channel_unix_new(server->server_socket_fd);
			g_io_add_watch (server->server_gio, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_ERR|G_IO_NVAL, stratus_unix_server_gio_watch_cb, server);
		}
		else
		{
			STRATUS_LOG_V("Socket listen Error = %s\n", error->message);
			g_free(server);
			server = NULL;
		}

	}
	else
	{
		STRATUS_LOG_V("Socket Bind Error = %s\n", error->message);
		g_free(server);
		server = NULL;
	}

	STRATUS_LOG("}");
	return server;
}

StratusServer* stratus_create_unix_server_with_fd(gint fd)
{
	STRATUS_LOG("{");
	StratusServer *server = (StratusServer*)g_malloc0(sizeof(StratusServer));
	GError *error = NULL;

	server->server_socket = NULL;
	server->server_socket = g_socket_new_from_fd (fd, &error);
	if(!(server->server_socket))
		STRATUS_LOG_V("Server Socket Creation failed error %s\n", error->message);
		
	server->connections = g_hash_table_new(g_direct_hash, g_direct_equal);
	server->id_conn_hash = g_hash_table_new(g_int_hash, g_int_equal);
	server->id_count = 0;
	server->server_type = UNIX_SERVER;

	/** Not sure if we should re-fetch the socket from the fd */
	server->server_socket_fd = g_socket_get_fd(server->server_socket);

	server->server_gio = g_io_channel_unix_new(server->server_socket_fd);
	g_io_add_watch (server->server_gio, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_ERR|G_IO_NVAL, stratus_unix_server_gio_watch_cb, server);

	STRATUS_LOG("}");
	return server;
}
