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

gchar* stratus_get_http_code_string(gint http_code)
{
	switch(http_code)
	{
		case 100:
			return "HTTP/1.1 100 Continue\r\n";
		case 101:
			return "HTTP/1.1 101 Switching Protocols\r\n";
		case 200:
			return "HTTP/1.1 200 OK\r\n";
		case 201:
			return "HTTP/1.1 201 Created\r\n";
		case 202:
			return "HTTP/1.1 202 Accepted\r\n";
		case 203:
			return "HTTP/1.1 203 Non-Authoritative Information\r\n";
		case 204:
			return "HTTP/1.1 204 No Content\r\n";
		case 205:
			return "HTTP/1.1 205 Reset Content\r\n";
		case 206:
			return "HTTP/1.1 206 Partial Content\r\n";
		case 300:
			return "HTTP/1.1 300 Multiple Choices\r\n";
		case 301:
			return "HTTP/1.1 301 Moved Permanently\r\n";
		case 302:
			return "HTTP/1.1 302 Found\r\n";
		case 303:
			return "HTTP/1.1 303 See Other\r\n";
		case 304:
			return "HTTP/1.1 304 Not Modified\r\n";
		case 305:
			return "HTTP/1.1 305 Use Proxy\r\n";
		case 307:
			return "HTTP/1.1 307 Temporary Redirect\r\n";
		case 400:
			return "HTTP/1.1 400 Bad Request\r\n";
		case 401:
			return "HTTP/1.1 401 Unauthorized\r\n";
		case 402:
			return "HTTP/1.1 402 Payment Required\r\n";
		case 403:
			return "HTTP/1.1 403 Forbidden\r\n";
		case 404:
			return "HTTP/1.1 404 Not Found\r\n";
		case 405:
			return "HTTP/1.1 405 Method Not Allowed\r\n";
		case 406:
			return "HTTP/1.1 406 Not Acceptable\r\n";
		case 407:
			return "HTTP/1.1 407 Proxy Authentication Required\r\n";
		case 408:
			return "HTTP/1.1 408 Request Timeout\r\n";
		case 409:
			return "HTTP/1.1 409 Conflict\r\n";
		case 410:
			return "HTTP/1.1 410 Gone\r\n";
		case 411:
			return "HTTP/1.1 411 Length Required\r\n";
		case 412:
			return "HTTP/1.1 412 Precondition Failed\r\n";
		case 413:
			return "HTTP/1.1 413 Request Entity Too Large\r\n";
		case 414:
			return "HTTP/1.1 414 Request-URI Too Long\r\n";
		case 415:
			return "HTTP/1.1 415 Unsupported Media Type\r\n";
		case 416:
			return "HTTP/1.1 416 Requested Range Not Satisfiable\r\n";
		case 417:
			return "HTTP/1.1 417 Expectation Failed\r\n";
		case 500:
			return "HTTP/1.1 500 Internal Server Error\r\n";
		case 501:
			return "HTTP/1.1 501 Not Implemented\r\n";
		case 502:
			return "HTTP/1.1 502 Bad Gateway\r\n";
		case 503:
			return "HTTP/1.1 503 Service Unavailable\r\n";
		case 504:
			return "HTTP/1.1 504 Gateway Timeout\r\n";
		case 505:
			return "HTTP/1.1 505 HTTP Version Not Supported\r\n";
		default:
			return "HTTP/1.1 200 OK\r\n";
	}
}

gint stratus_connection_write_headers (StratusServer *server, gint connection_id, gint http_code, gchar **headers)
{
	STRATUS_LOG("{");
	StratusServerConnection *connection = NULL;
	GString *header_str = NULL;
	gint i = 0;

	printf("conn id %d\n", connection_id);
	if(!g_hash_table_lookup_extended(server->id_conn_hash, (gpointer)&connection_id, NULL, (gpointer*)&connection))
	{
		STRATUS_LOG("Connection not Found in this server \n");
		return 0;
	}

	if(connection == NULL || connection->connection_gio == NULL)
	{
		STRATUS_LOG("Connection Object or Channel not Present\n");
		return 0;
	}

	GIOStatus status = 0;
	GError *error = NULL;
	gsize bytes_written = 0;

	header_str = g_string_new(stratus_get_http_code_string(http_code));
	header_str = g_string_append(header_str, "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS, PUT, DELETE\r\n");

	connection->chunk_transfer = TRUE;
	for(i = 0; headers && headers[i] != NULL; i++)
	{
		g_string_append_printf(header_str, "%s\r\n", headers[i]);
		if(g_str_has_prefix(headers[i], "Content-Length:"))
		{
			connection->chunk_transfer = FALSE;
		}
	}

	if(connection->chunk_transfer)
	{
		header_str = g_string_append(header_str, "Transfer-Encoding: chunked\r\n");
	}
	header_str = g_string_append(header_str, "\r\n");

	STRATUS_LOG_V("1 Connneciotn pointer %p\n", connection);
	status = g_io_channel_write_chars (connection->connection_gio, header_str->str, header_str->len, &bytes_written, &error);

	connection->headers_sent = TRUE;

	if(status == G_IO_STATUS_ERROR)
	{
		if(error)
			{STRATUS_LOG_V("Error in writting data to the channel error = %s\n", error->message);}
		else
			{STRATUS_LOG("Error in writting data to the channel No error Set\n");}
	}
	else if(status != G_IO_STATUS_NORMAL)
	{
		STRATUS_LOG_V("Error in writting data to the channel status %d\n", status);
	}
	else
	{
		/** Only if write successeds then flush it*/
		status = g_io_channel_flush (connection->connection_gio, &error);
		if(status != G_IO_STATUS_NORMAL && status == G_IO_STATUS_ERROR)
			{STRATUS_LOG_V("Error in flushing data to the channel error = %s\n", error->message);}
		else if(status != G_IO_STATUS_NORMAL)
			{STRATUS_LOG_V("Error in flushing data to the channel status %d\n", status);}
	}

	if(bytes_written == header_str->len)
		bytes_written = 0;
	else
		bytes_written = -1;
	STRATUS_LOG("}");
	g_string_free(header_str, TRUE);
	return bytes_written;
}

static gboolean stratus_connection_gio_watch_ready_to_write_cb (GIOChannel *source, GIOCondition condition, gpointer userdata)
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
	STRATUS_LOG_V("source %p Connection pointer address = %p\n", source, connection);

	GIOStatus status = 0;
	GError *error = NULL;
	gsize bytes_written = 0;

	status = g_io_channel_write_chars (connection->connection_gio, connection->response->buffer + connection->response->send_size, connection->response->size - connection->response->send_size, &bytes_written, &error);

	if(status == G_IO_STATUS_ERROR)
	{
		if(error)
			{STRATUS_LOG_V("Error in writting data to the channel error = %s\n", error->message);}
		else
			{STRATUS_LOG("Error in writting data to the channel No error Set\n");}
	}
	else if(status != G_IO_STATUS_NORMAL)
	{
		STRATUS_LOG_V("Error in writting data to the channel status %d\n", status);
	}
	else
	{
		STRATUS_LOG_V("total number of bytes written = %d , total bytes were suppose to written %d\n", bytes_written, connection->response->size - connection->response->send_size);
		connection->response->send_size = connection->response->send_size + bytes_written;

		status = g_io_channel_flush (connection->connection_gio, &error);
		if(status != G_IO_STATUS_NORMAL && status == G_IO_STATUS_ERROR)
			{STRATUS_LOG_V("Error in flushing data to the channel error = %s\n", error->message);}
		else if(status != G_IO_STATUS_NORMAL)
			{STRATUS_LOG_V("Error in flushing data to the channel status %d\n", status);}

		if(connection->response->send_size == connection->response->size)
		{
			/** If close has been called then close it here else , let the close be called but we definitly dont need this callback again so return FALSE;*/
			if(connection->close_called == TRUE)
			{
				if(connection->chunk_transfer)
				{
					gchar *chunk_end = g_strdup("0\r\n\r\n");
					status = g_io_channel_write_chars (connection->connection_gio, chunk_end, strlen(chunk_end), &bytes_written, &error);
					g_free(chunk_end);
				}

				status = g_io_channel_flush (connection->connection_gio, &error);
				if(status != G_IO_STATUS_NORMAL && status == G_IO_STATUS_ERROR)
					{STRATUS_LOG_V("Error in flushing data to the channel error = %s\n", error->message);}
				else if(status != G_IO_STATUS_NORMAL)
					{STRATUS_LOG_V("Error in flushing data to the channel status %d\n", status);}

				g_hash_table_remove(server->connections, connection->connection_gio);
				g_hash_table_remove(server->id_conn_hash, &(connection->conn_id));
				stratus_server_connection_free(connection);
			}
			else
			{
				g_free(connection->response->buffer);
				connection->response->buffer = NULL;
				connection->response->size = 0;
				connection->response->send_size = 0;
			}
			return FALSE;
		}
		return TRUE;
	}

	STRATUS_LOG("}");
	return FALSE;
}

gint stratus_connection_write (StratusServer *server, gint connection_id, gchar *data, gsize size)
{
	STRATUS_LOG("{");
	StratusServerConnection *connection = NULL;
	gsize chunk_size_size = 0;
	gchar *chunk_size = NULL, *buffer_ptr = NULL;

	printf("conn id %d\n", connection_id);
	if(!g_hash_table_lookup_extended(server->id_conn_hash, (gpointer)&connection_id, NULL, (gpointer*)&connection))
	{
		STRATUS_LOG("Connection not Found in this server \n");
		return 0;
	}

	if(connection == NULL || connection->connection_gio == NULL)
	{
		STRATUS_LOG("Connection Object or Channel not Present\n");
		return 0;
	}

	if(connection->headers_sent == FALSE)
	{
		stratus_connection_write_headers (server, connection_id, 200, NULL);
	}

	STRATUS_LOG_V("1 Connneciotn pointer %p\n", connection);

	if(connection->chunk_transfer)
	{
		chunk_size = g_strdup_printf("%X\r\n", size);
		/** If it is transfer-encoding chunk then we need chunk size and also \r\n at the end of the chunk hence we need strlen(chunk_size) and +2 */
		chunk_size_size = strlen(chunk_size);
		connection->response->buffer = g_realloc(connection->response->buffer, connection->response->size + size + chunk_size_size + 2);
		buffer_ptr = connection->response->buffer + connection->response->size;
		/* copying the chunksize string*/
		g_memmove(buffer_ptr, chunk_size, chunk_size_size);
		buffer_ptr = buffer_ptr + chunk_size_size;
		/* copying the actual data */
		g_memmove(buffer_ptr, data, size);
		buffer_ptr = buffer_ptr + size;
		/* copying the trailing \r\n for chunked encoding*/
		g_memmove(buffer_ptr, "\r\n", 2);
		buffer_ptr = buffer_ptr + 2;
		g_free(chunk_size);
		connection->response->size = connection->response->size + size + chunk_size_size + 2;
	}
	else
	{ //Content -length transfer
		connection->response->buffer = g_realloc(connection->response->buffer, connection->response->size + size);
		buffer_ptr = connection->response->buffer + connection->response->size;
		/* copying the actual data */
		g_memmove(buffer_ptr, data, size);
		buffer_ptr = buffer_ptr + size;

		connection->response->size = connection->response->size + size;
	}

	connection->write_watch_id = g_io_add_watch(connection->connection_gio, G_IO_OUT, stratus_connection_gio_watch_ready_to_write_cb, server);

	STRATUS_LOG("}");
	return size;
}

gint stratus_connection_write_printf (StratusServer *server, gint connection_id, const gchar *format, ...)
{
	STRATUS_LOG("{");
/* NOTE: As anyways now cause of transfer encoding chunk we are creating a string out of the format and var args we can directly call the write API instead of doing vdprintf

	StratusServerConnection *connection = NULL;
	if(!g_hash_table_lookup_extended(server->id_conn_hash, (gpointer)&connection_id, NULL, (gpointer*)&connection))
	{
		STRATUS_LOG("Connection not Found in this server \n");
		return 0;
	}

	if(connection == NULL || connection->connection_gio == NULL)
	{
		STRATUS_LOG("Connection Object or Channel not Present\n");
		return 0;
	}

	GIOStatus status = 0;
	GError *error = NULL;


	STRATUS_LOG_V("1 Connneciotn pointer %p\n", connection);
*/
	va_list args;
	gchar *chunk = NULL;
	gsize bytes_written = 0;

	va_start(args, format);

//	bytes_written = vdprintf(connection->connection_socket_fd, format, args);
	bytes_written = g_vasprintf(&chunk, format, args);

	bytes_written = stratus_connection_write (server, connection_id, chunk, bytes_written);

	g_free(chunk);
	va_end(args);

/*	STRATUS_LOG_V("bytes written %d\n", bytes_written);
	if(errno)
		STRATUS_LOG_V("Error %s\n", g_strerror(errno));

	status = g_io_channel_flush (connection->connection_gio, &error);
	if(status != G_IO_STATUS_NORMAL && status == G_IO_STATUS_ERROR)
		{STRATUS_LOG_V("Error in flushing data to the channel error = %s\n", error->message);}
	else if(status != G_IO_STATUS_NORMAL)
		{STRATUS_LOG_V("Error in flushing data to the channel status %d\n", status);}
*/
	STRATUS_LOG("}");
	return bytes_written;
}
