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

void stratus_static_file_handler(gint connection_id, STRATUS_CONN_STATE state, Request *request, ServerPacket *body, gpointer userdata)
{
	STRATUS_LOG("{");
	printf("Conn id %d\n", connection_id);
	StratusServer *server = (StratusServer*)userdata;
	if(state == CONN_STATE_ESTABLISHED)
	{
		gchar **response_headers = NULL;
		GError *error = NULL;
		gchar *contents = NULL;
		gsize size = 0;
	
		if(body)
			STRATUS_LOG_V("Body size = %d\n%s\n", body->size, body->buffer);
		if(request)
			STRATUS_LOG_V("Request Query = [%s] protocol = %d\n", request->uri, request->http_method);
	
		if(g_file_get_contents(request->uri, &contents, &size, &error))
		{
			response_headers = (gchar**)g_malloc0(sizeof(gchar*)*2);
			response_headers[0] = g_strdup_printf("Content-Type: %s", stratus_get_mime_type_for_file(request->uri));
			response_headers[1] = g_strdup_printf("Content-Length: %u", size);
	
			STRATUS_LOG_V("response_headers\n%s\n%s\n", response_headers[0], response_headers[1]);
	
			/** Send Headers*/
			if(stratus_connection_write_headers(server, connection_id, 200, response_headers) != 0)
				STRATUS_LOG("Error Less number of bytes were written\n");
	
			if(stratus_connection_write(server, connection_id, contents, size) != size)
				STRATUS_LOG("Error Less number of bytes were written\n");
	
			g_free(contents);
		}
		else
		{
			if(stratus_connection_write_headers(server, connection_id, 404, NULL) != 0)
				STRATUS_LOG("Error Less number of bytes were written\n");
		}
	}
	/** In all cases of state I close the connection*/
	stratus_connection_close(server, connection_id);

	STRATUS_LOG("}");
}

/*
static void stratus_free_ws_header(StratusWebSocketHeader *ws_header)
{
	gint i = 0;
	for(i = 0; i < TOKEN_COUNT; i++)
	{
		if(ws_header->tokens[i].buffer)
			g_free(ws_header->tokens[i].buffer);
	}
	g_free(ws_header);
}
*/

const gchar* stratus_get_mime_type_for_file(gchar *file_path)
{
	if(g_str_has_suffix(file_path, ".pdf"))
		return "application/pdf";
	else if(g_str_has_suffix(file_path, ".sig"))
		return "application/pgp-signature";
	else if(g_str_has_suffix(file_path, ".spl"))
		return "application/futuresplash";
	else if(g_str_has_suffix(file_path, ".class"))
		return "application/octet-stream";
	else if(g_str_has_suffix(file_path, ".ps"))
		return "application/postscript";
	else if(g_str_has_suffix(file_path, ".torrent"))
		return "application/x-bittorrent";
	else if(g_str_has_suffix(file_path, ".dvi"))
		return "application/x-dvi";
	else if(g_str_has_suffix(file_path, ".gz"))
		return "application/x-gzip";
	else if(g_str_has_suffix(file_path, ".pac"))
		return "application/x-ns-proxy-autoconfig";
	else if(g_str_has_suffix(file_path, ".swf"))
		return "application/x-shockwave-flash";
	else if(g_str_has_suffix(file_path, ".tar.gz"))
		return "application/x-tgz";
	else if(g_str_has_suffix(file_path, ".tgz"))
		return "application/x-tgz";
	else if(g_str_has_suffix(file_path, "tar"))
		return "application/x-tar";
	else if(g_str_has_suffix(file_path, "zip"))
		return "application/zip";
	else if(g_str_has_suffix(file_path, "mp3"))
		return "audio/mpeg";
	else if(g_str_has_suffix(file_path, "m3u"))
		return "audio/x-mpegurl";
	else if(g_str_has_suffix(file_path, "wma"))
		return "audio/x-ms-wma";
	else if(g_str_has_suffix(file_path, "wax"))
		return "audio/x-ms-wax";
	else if(g_str_has_suffix(file_path, "ogg"))
		return "application/ogg";
	else if(g_str_has_suffix(file_path, "wav"))
		return "audio/x-wav";
	else if(g_str_has_suffix(file_path, "gif"))
		return "image/gif";
	else if(g_str_has_suffix(file_path, "jpg"))
		return "image/jpeg";
	else if(g_str_has_suffix(file_path, "jpeg"))
		return "image/jpeg";
	else if(g_str_has_suffix(file_path, "png"))
		return "image/png";
	else if(g_str_has_suffix(file_path, "xbm"))
		return "image/x-xbitmap";
	else if(g_str_has_suffix(file_path, "xpm"))
		return "image/x-xpixmap";
	else if(g_str_has_suffix(file_path, "xwd"))
		return "image/x-xwindowdump";
	else if(g_str_has_suffix(file_path, "css"))
		return "text/css";
	else if(g_str_has_suffix(file_path, "html"))
		return "text/html";
	else if(g_str_has_suffix(file_path, "htm"))
		return "text/html";
	else if(g_str_has_suffix(file_path, "js"))
		return "text/javascript";
	else if(g_str_has_suffix(file_path, "asc"))
		return "text/plain";
	else if(g_str_has_suffix(file_path, "c"))
		return "text/plain";
	else if(g_str_has_suffix(file_path, "cpp"))
		return "text/plain";
	else if(g_str_has_suffix(file_path, "log"))
		return "text/plain";
	else if(g_str_has_suffix(file_path, "conf"))
		return "text/plain";
	else if(g_str_has_suffix(file_path, "text"))
		return "text/plain";
	else if(g_str_has_suffix(file_path, "txt"))
		return "text/plain";
	else if(g_str_has_suffix(file_path, "dtd"))
		return "text/xml";
	else if(g_str_has_suffix(file_path, "xml"))
		return "text/xml";
	else if(g_str_has_suffix(file_path, "mpeg"))
		return "video/mpeg";
	else if(g_str_has_suffix(file_path, "mpg"))
		return "video/mpeg";
	else if(g_str_has_suffix(file_path, "mov"))
		return "video/quicktime";
	else if(g_str_has_suffix(file_path, "qt"))
		return "video/quicktime";
	else if(g_str_has_suffix(file_path, "avi"))
		return "video/x-msvideo";
	else if(g_str_has_suffix(file_path, "asf"))
		return "video/x-ms-asf";
	else if(g_str_has_suffix(file_path, "asx"))
		return "video/x-ms-asf";
	else if(g_str_has_suffix(file_path, "wmv"))
		return "video/x-ms-wmv";
	else if(g_str_has_suffix(file_path, "bz2"))
		return "application/x-bzip";
	else if(g_str_has_suffix(file_path, "manifest"))
		return "text/cache-manifest";
	else if(g_str_has_suffix(file_path, "tbz"))
		return "application/x-bzip-compressed-tar";
	else if(g_str_has_suffix(file_path, "tar.bz2"))
		return "application/x-bzip-compressed-tar";
	else
		return "text/plain";
}
