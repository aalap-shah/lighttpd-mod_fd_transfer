/*
 * Copyright (c) 2011, Motorola Mobility, Inc
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *  - Neither the name of Motorola Mobility nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */

#include "stratus_server.h"
#include "stratus_logging.h"

#ifdef LOGGING_FILE
/** Logging file pointer*/
extern FILE *fp;
#endif

static gboolean convert_key(gchar *key, gulong *conv_key)
{
	STRATUS_LOG("{");
	gchar digits[25] = {0};
	gchar *ptr = NULL;
	gint digit_count = 0, space_count = 0;
	guint64 conv_key1 = 0;
	for(ptr = key; *ptr; ptr++)
	{
		if(g_ascii_isdigit(*ptr))
			digits[digit_count++] = *ptr;
		else if(*ptr == ' ')
			space_count++;
	}
	conv_key1 = g_ascii_strtoull(digits, NULL, 10);
	if(conv_key1 > 0 && space_count > 0)
	{
		*conv_key = conv_key1/space_count;
		return TRUE;
	}
	STRATUS_LOG("}");
	return FALSE;
}

void stratus_ws_handshake_handler(StratusServerConnection *connection)
{
	STRATUS_LOG("{");

	gchar *response = NULL;
	gulong key1 = 0, key2 = 0;
	gint i = 0;
	guchar string[16] = {0}, md5[17] = {0}, sha1[21] = {0};
	GIOStatus status = 0;
	GError *error = NULL;
	gsize bytes_written = 0;

	if(connection && connection->request->header.ws_header && connection->request->header.ws_header->handshake_protocol_id == 6455)
	{

		const guchar *concat_key = g_strdup_printf("%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", connection->request->header.ws_header->tokens[TOKEN_KEY].buffer);
		unsigned char *temp = SHA1(concat_key, strlen(concat_key), sha1);
		gchar *sha1_encoded  = g_base64_encode (sha1, 20);

		response = g_strdup_printf("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", sha1_encoded);
	
		STRATUS_LOG_V("key[%s]WebSocket Handshake Response = [%s]\n", concat_key, response);
	
		status = g_io_channel_write_chars (connection->connection_gio, response, strlen(response), &bytes_written, &error);
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
		g_free(response);
	}
	else if(connection && connection->request->header.ws_header && connection->request->header.ws_header->handshake_protocol_id == 75)
	{
		response = g_strdup_printf("HTTP/1.1 101 Web Socket Protocol Handshake\r\nUpgrade: WebSocket\r\nConnection: Upgrade\r\nWebSocket-Origin: %s\r\nWebSocket-Location: ws://%s%s\r\nWebSocket-Protocol: %s\r\n\r\n", connection->request->origin, connection->request->host, connection->request->uri, connection->request->header.ws_header->tokens[TOKEN_PROTOCOL].buffer);
	
		STRATUS_LOG_V("WebSocket Handshake Response = [%s]\n", response);
	
		status = g_io_channel_write_chars (connection->connection_gio, response, strlen(response), &bytes_written, &error);
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
		g_free(response);
	}
	else if(connection && connection->request->header.ws_header && connection->request->header.ws_header->handshake_protocol_id == 76)
	{
		response = g_strdup_printf("HTTP/1.1 101 WebSocket Protocol Handshake\r\nUpgrade: WebSocket\r\nConnection: Upgrade\r\nSec-WebSocket-Origin: %s\r\nSec-WebSocket-Location: ws://%s%s\r\nSec-WebSocket-Protocol: %s\r\n\r\n", connection->request->origin, connection->request->host, connection->request->uri, connection->request->header.ws_header->tokens[TOKEN_PROTOCOL].buffer);
	
		STRATUS_LOG_V("WebSocket Handshake Response = [%s]\n", response);
	
		/** convert the two keys into 32-bit integers */
		STRATUS_LOG_V("key1 [%s] key2 [%s]\n", connection->request->header.ws_header->tokens[TOKEN_KEY1].buffer, connection->request->header.ws_header->tokens[TOKEN_KEY2].buffer);
	
		if(convert_key(connection->request->header.ws_header->tokens[TOKEN_KEY1].buffer, &key1) && convert_key(connection->request->header.ws_header->tokens[TOKEN_KEY2].buffer, &key2))
		{
			STRATUS_LOG_V("converted key1 [%ld] key2 [%ld]\n", key1, key2);
	
			/** network byte order (MSB first) */
			string[0] = key1 >> 24;
			string[1] = key1 >> 16;
			string[2] = key1 >> 8;
			string[3] = key1;
			string[4] = key2 >> 24;
			string[5] = key2 >> 16;
			string[6] = key2 >> 8;
			string[7] = key2;
			g_memmove(&string[8], connection->request->header.ws_header->tokens[TOKEN_CHALLENGE].buffer, 8);
	
			MD5(string, 16, (guchar*)md5);
	
			STRATUS_LOG("MD5 string is in Hex [");
			for(i = 0; i < 16; i++)
				STRATUS_LOG_V("%x  ", md5[i]);
			STRATUS_LOG("]\n");
	
			/** Send Headers*/
			status = g_io_channel_write_chars (connection->connection_gio, response, strlen(response), &bytes_written, &error);
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

			/** Send md5 challenge*/
			status = g_io_channel_write_chars (connection->connection_gio, (gchar*)md5, 16, &bytes_written, &error);
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
		}
		g_free(response);
	}
	else
	{
		STRATUS_LOG("Error in handshake protocol unknown or not set\n");
	}

	/** Only if write successeds then flush it*/
	status = g_io_channel_flush (connection->connection_gio, &error);
	if(status != G_IO_STATUS_NORMAL && status == G_IO_STATUS_ERROR)
		{STRATUS_LOG_V("Error in flushing data to the channel error = %s\n", error->message);}
	else if(status != G_IO_STATUS_NORMAL)
		{STRATUS_LOG_V("Error in flushing data to the channel status %d\n", status);}

	STRATUS_LOG("}");
}

void stratus_ws_parse_frame(ServerPacket *body)
{
	guchar *buffer_ptr = body->buffer;
	gsize size = 0;
	gint i = 0;
	gchar *enc_data = NULL, *data = NULL;
	if(body->buffer && body->buffer != NULL && body->size > 0 && (gint)*buffer_ptr == 0x81)
	{
		/** NOTE: We only support frames with FIN (final frame) flag set + the opcode being TEXT frame Only*/
		buffer_ptr++;
		/** NOTE: Checking if the masking bit is set or not*/
		if((gint)*buffer_ptr || 0x80)
		{
			gchar maskKey[4] = {0};
			size = (gint)*buffer_ptr & 0x7F;
			if(size <= 125)
			{
				/** NOTE : Size is the actual size*/
				buffer_ptr++;
			}
			else if(size == 126)
			{
				buffer_ptr++;
				size = (( (gint)*buffer_ptr & 0xFF ) << 16) + ((gint)*(buffer_ptr + 1) & 0xFF);
				buffer_ptr = buffer_ptr + 2;
			}
			else if(size == 127)
			{
				buffer_ptr++;
				gint64 actual_size = (( (gint)*buffer_ptr & 0xFF ) << 56) + (((gint)*(buffer_ptr + 1) & 0xFF) << 48) + (((gint)*(buffer_ptr + 2) & 0xFF) << 40) + (((gint)*(buffer_ptr + 3) & 0xFF) << 32) + (((gint)*(buffer_ptr + 4) & 0xFF) << 24) + (((gint)*(buffer_ptr + 5) & 0xFF) << 16) + (((gint)*(buffer_ptr + 6) & 0xFF) << 8) + ((gint)*(buffer_ptr + 7) & 0xFF);
				buffer_ptr = buffer_ptr + 8;
				/** NOTE: We ignore the 64bit length and we always consider only 32bit length*/
				size = (gint) actual_size;
			}

			maskKey[0] = (gint)*(buffer_ptr) & 0xFF;
			maskKey[1] = (gint)*(buffer_ptr + 1) & 0xFF;
			maskKey[2] = (gint)*(buffer_ptr + 2) & 0xFF;
			maskKey[3] = (gint)*(buffer_ptr + 3) & 0xFF;

			buffer_ptr = buffer_ptr + 4;
			enc_data = g_malloc0(sizeof(gchar)*size + 1);
			data = g_malloc0(sizeof(gchar)*size + 1);
			g_memmove(enc_data, buffer_ptr, size);
			for(i = 0; i < size; i++)
			{
				data[i] = enc_data[i] ^ maskKey[i % 4];
			}
			g_free(body->buffer);
			body->buffer = data;
			body->size = size;
		}
	}
}
