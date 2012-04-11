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

StratusServerConnection* stratus_server_connection_init()
{
	StratusServerConnection *connection = (StratusServerConnection*)g_malloc0(sizeof(StratusServerConnection));
	connection->header = (ServerPacket*)g_malloc0(sizeof(ServerPacket));
	connection->body = (ServerPacket*)g_malloc0(sizeof(ServerPacket));
	connection->data = (ServerPacket*)g_malloc0(sizeof(ServerPacket));
	connection->response = (ResponsePacket*)g_malloc0(sizeof(ResponsePacket));
	connection->response->size = 0;
	connection->response->send_size = 0;

	connection->close_called = FALSE;
	return connection;
}

void stratus_server_connection_free(StratusServerConnection* connection)
{
	GError *error = NULL;
	g_source_remove(connection->watch_id);
	g_io_channel_shutdown (connection->connection_gio, TRUE, &error);
	g_io_channel_unref(connection->connection_gio);
	if(error)
		STRATUS_LOG_V("IOChannel Shut down error %s\n", error->message);

	connection->connection_gio = NULL;

	if(connection->data)
	{
		g_free(connection->data->buffer);
		g_free(connection->data);
	}
	if(connection->header)
	{
		g_free(connection->header->buffer);
		g_free(connection->header);
	}
	if(connection->body)
	{
		g_free(connection->body->buffer);
		g_free(connection->body);
	}
	if(connection->response)
	{
		g_free(connection->response->buffer);
		g_free(connection->response);
	}
	g_free(connection);

	return;
}

gboolean stratus_connection_gio_watch_cb(GIOChannel *source, GIOCondition condition, gpointer userdata)
{
	STRATUS_LOG("{");
	StratusServer *server = (StratusServer*) userdata;
	StratusServerConnection *connection = NULL;

	if(!g_hash_table_lookup_extended(server->connections, source, NULL, (gpointer*)&connection))
	{
		STRATUS_LOG("Connection not Found in this server \n");
		//NOTE: Close the io_watch as connection is not present
		return FALSE;
	}
	STRATUS_LOG_V("source %p Connection pointer address = %p condition %d  G_IO_IN %d\n", source, connection, condition, G_IO_IN);

	switch(condition)
	{
		case G_IO_IN:
		case G_IO_PRI:
		/** If only reading from channel successeded , go ahead*/
		/** This function is only suppose to operate on data buffer and not on body or header*/
		if(stratus_connection_g_io_channel_read_to_end(connection))
		{
			STRATUS_LOG_V("Request [%s] protocol %d\n", connection->data->buffer, connection->protocol);
			/** If protocol is not yet assigned then only decode the protocol*/
			if(connection->protocol == CONN_PROTO_UNKNOWN)
			{
				/** Decode the protocol based on the handlers assigned so if only 1 handler assigned we literally dont deocde at all. Decode works based on what handlers are assigned*/
//				stratus_decode_protocol(server, connection);
				/** This function operates on data and converts that data to header and body. And free the data.*/
				stratus_parse_request_header(server, connection);
			}
			else
			{
				gchar *buffer = connection->body->buffer;
				connection->body->buffer = g_malloc0(connection->body->size + connection->data->size + 1);
				if(connection->body->size != 0)
				{
					g_memmove(connection->body->buffer, buffer, connection->body->size);
					g_free(buffer);
				}
				g_memmove(connection->body->buffer + connection->body->size, connection->data->buffer, connection->data->size);
				connection->body->size = connection->body->size + connection->data->size;

				connection->data->buffer = NULL;
				connection->data->size = 0;

				STRATUS_LOG_V("content ength %d body size %d\n",connection->request->content_length, connection->body->size);
				if((connection->protocol == CONN_PROTO_HTTP) && (connection->body->size >= connection->request->content_length))
				{
					connection->state = CONN_STATE_ESTABLISHED;
				}
			}
			
			if((connection->protocol == CONN_PROTO_WS) && (connection->state == CONN_STATE_HTTP_HEADERS))
			{
				/** Perform the handhsake if the state is headers received and then set the state to established*/
				stratus_ws_handshake_handler(connection);
				connection->state = CONN_STATE_ESTABLISHED;
				connection->headers_sent = TRUE;
				g_free(connection->data->buffer);
				connection->data->size = 0;
				connection->data->buffer = NULL;
			}
			STRATUS_LOG_V("protocol %d connection->state %d\n", connection->protocol, connection->state);

			/** Normal condition.. Call the handler callback.*/
			if(connection->handler.callback && connection->protocol == CONN_PROTO_WS && connection->state == CONN_STATE_ESTABLISHED)
			{
				(connection->handler.callback)(connection->conn_id, connection->state, connection->request, connection->body, connection->handler.userdata);
				g_free(connection->body->buffer);
				connection->body->size = 0;
				connection->body->buffer = NULL;
			}
			else if(connection->handler.callback && connection->protocol == CONN_PROTO_HTTP && connection->state == CONN_STATE_ESTABLISHED)
			{
				(connection->handler.callback)(connection->conn_id, connection->state, connection->request, connection->body, connection->handler.userdata);
			}
			else
			{
				STRATUS_LOG("Oops the callback isnt assinged for this protocol requests, either its a request for unknown protocol or either the protocol of this request is not yet decoded\n");
			}
		}
		else
		{
			/** If read to end failed it means some client must have directly closed the socket without informing us. Therefore call callback with CLOSED state to allow him to handle stuff*/
			(connection->handler.callback)(connection->conn_id, CONN_STATE_CLOSED, connection->request, connection->data, connection->handler.userdata);
			return FALSE;
		}
		break;
		case G_IO_HUP:
			STRATUS_LOG("I got socket Hang up\n");
			/** If HUP it means some client must have directly closed the socket without informing us. Therefore call connection close manually*/
			(connection->handler.callback)(connection->conn_id, CONN_STATE_CLOSED, connection->request, connection->data, connection->handler.userdata);
			return FALSE;
		break;
		case G_IO_ERR:
			STRATUS_LOG("I got socket Error\n");
			/** If HUP it means some client must have directly closed the socket without informing us. Therefore call connection close manually*/
			(connection->handler.callback)(connection->conn_id, CONN_STATE_CLOSED, connection->request, connection->data, connection->handler.userdata);
			return FALSE;
		break;
		case G_IO_NVAL:
			STRATUS_LOG("I got socket Invalid\n");
			/** If HUP it means some client must have directly closed the socket without informing us. Therefore call connection close manually*/
			(connection->handler.callback)(connection->conn_id, CONN_STATE_CLOSED, connection->request, connection->data, connection->handler.userdata);
			return FALSE;
		break;
		default:
			/** If Err it means some client must have directly closed the socket without informing us. Therefore call connection close manually*/
			(connection->handler.callback)(connection->conn_id, CONN_STATE_CLOSED, connection->request, connection->data, connection->handler.userdata);
			STRATUS_LOG("I got socket Error\n");
		break;
	}
	STRATUS_LOG("}");
	return TRUE;
}

gboolean stratus_connection_g_io_channel_read_to_end (StratusServerConnection* connection)
{
	STRATUS_LOG("{");

	ServerPacket *temp_data = NULL;
	GPtrArray *data_ptr_array = NULL;
	GError *error = NULL;
	gchar *buffer = NULL;
	gsize io_buffer_size = 0, read_size = 0, array_size = 0;
	gint status = 0, i = 0;

	data_ptr_array = g_ptr_array_new();
	io_buffer_size = g_io_channel_get_buffer_size(connection->connection_gio);
	STRATUS_LOG_V("io bufer size %d\n", io_buffer_size);

	/** NOTE : I tried using below function but it doesnt read data it always sends me resource busy.. I think cause its a socket so...If future generations make this function work then you can avoid the below do while logic
	status = g_io_channel_read_to_end (connection->connection_gio, &(buffer), &(size), &error); */
	do{
		temp_data = (ServerPacket*)g_malloc0(sizeof(ServerPacket));
		temp_data->buffer = g_malloc0(sizeof(gchar)*io_buffer_size);

		status = g_io_channel_read_chars (connection->connection_gio, temp_data->buffer, io_buffer_size, &read_size, &error);
		STRATUS_LOG_V("Data Read from the Connection size %d status %d content [%s]\n", read_size, status, temp_data->buffer);

		temp_data->size = read_size;
		if(status == G_IO_STATUS_NORMAL)
		{
			g_ptr_array_add(data_ptr_array, temp_data);
			array_size = array_size + read_size;
		}
		else if(status == G_IO_STATUS_AGAIN)
		{
			g_free(temp_data->buffer);
			g_free(temp_data);
//			g_ptr_array_free(data_ptr_array, FALSE);
		}
		else
		{
			if(status == G_IO_STATUS_ERROR)
				{STRATUS_LOG_V("Error in reading from the channel error = %s\n", error->message);}
			else
				{STRATUS_LOG_V("Error in reading from the channel status %d \n", status);}
			g_free(temp_data->buffer);
			g_free(temp_data);
//			g_ptr_array_free(data_ptr_array, TRUE);
			return FALSE;
		}
	}while(read_size == io_buffer_size && read_size != 0);

//	STRATUS_LOG_V("data buffer contents are [%s]size [%d]\n", connection->data->buffer, connection->data->size);
	buffer = connection->data->buffer;
	connection->data->buffer = g_malloc0(connection->data->size + array_size + 1);
	if(connection->data->size != 0)
	{
		g_memmove(connection->data->buffer, buffer, connection->data->size);
		g_free(buffer);
	}
	buffer = connection->data->buffer + connection->data->size;

	for(i = 0; i < data_ptr_array->len; i++)
	{
		temp_data = g_ptr_array_index(data_ptr_array, i);
		g_memmove(buffer, temp_data->buffer, temp_data->size);
		buffer = buffer + temp_data->size;
		connection->data->size = connection->data->size + temp_data->size;
		g_free(temp_data->buffer);
		g_free(temp_data);
	}
	STRATUS_LOG_V("data buffer contents are [%s] size [%d]\n", connection->data->buffer, connection->data->size);
	g_ptr_array_free(data_ptr_array, TRUE);
	STRATUS_LOG("}");
	return TRUE;
}

void stratus_server_add_connection_handler (StratusServer *server, gint connection_id, StratusServerCallBack callback, gpointer userdata)
{
	STRATUS_LOG("{");
	StratusServerConnection *connection = NULL;

	if(!g_hash_table_lookup_extended(server->id_conn_hash, (gpointer)&connection_id, NULL, (gpointer*)&connection))
	{
		STRATUS_LOG("Connection not Found in this server \n");
		return;
	}

	if(connection == NULL)
	{
		STRATUS_LOG("Invalid connection pointer\n");
		return;
	}
	connection->handler.callback = callback;
	connection->handler.userdata = userdata;
	STRATUS_LOG("}");
}

void stratus_connection_close (StratusServer *server, gint connection_id)
{
	STRATUS_LOG("{");
	GError *error = NULL;

	StratusServerConnection *connection = NULL;
	GIOStatus status = 0;
	gsize bytes_written = 0;
	gchar *chunk_end = NULL;

	if(!g_hash_table_lookup_extended(server->id_conn_hash, (gpointer)&connection_id, NULL, (gpointer*)&connection))
	{
		STRATUS_LOG("Connection not Found in this server \n");
		return;
	}
	STRATUS_LOG_V("Connection pointer %p connection id %d\n", connection, connection_id);

	connection->close_called = TRUE;
	if(connection->response->send_size == connection->response->size)
	{
		if(connection->chunk_transfer)
		{
			chunk_end = g_strdup("0\r\n\r\n");
			status = g_io_channel_write_chars (connection->connection_gio, chunk_end, strlen(chunk_end), &bytes_written, &error);
			g_free(chunk_end);
		}

		status = g_io_channel_flush (connection->connection_gio, &error);
		if(status != G_IO_STATUS_NORMAL && status == G_IO_STATUS_ERROR)
			{STRATUS_LOG_V("Error in flushing data to the channel error = %s\n", error->message);}
		else if(status != G_IO_STATUS_NORMAL)
			{STRATUS_LOG_V("Error in flushing data to the channel status %d\n", status);}
	
		g_hash_table_remove(server->connections, connection->connection_gio);
		g_hash_table_remove(server->id_conn_hash, &connection_id);
		stratus_server_connection_free(connection);

	}

	STRATUS_LOG("}");
}
