/*
 * Copyright (c) 2011, Motorola Mobility, Inc
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *  - Neither the name of Motorola Mobility nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */

#include<stdio.h>
#include<glib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <malloc.h>
#include <string.h>
#include <glib-object.h>

gint sfd = 0;

gint connect_to_server(gint port)
{
	gint socket_fd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr *srv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	socklen_t servlen;

	srv_addr = (struct sockaddr *) &serv_addr;

	/** We first try to connect to the backend to see if it is already present or not. If it is present then we do not need to fork that process*/
	if (-1 == (socket_fd = socket(AF_INET, SOCK_STREAM, 0))) {
		printf("1\n");
		return -1;
	}
	printf("sockfd %d\n", socket_fd);
	if (-1 == connect(socket_fd, srv_addr, sizeof(serv_addr))) {
		printf("2 error\n");
		perror("wtf");
		return -1;
	}
	return socket_fd;
}

gint send_to_server(gint sfd, void *buf, gint size)
{
	return send(sfd, buf, size, 0);
}

gchar *post_data = NULL;

gboolean idle_func(gpointer data)
{
//	send_to_server(sfd, post_data, strlen(post_data));
	printf("Buffer 2\n");

	gchar *buffer2 = (gchar*)g_malloc0(102400);
	recv(sfd, buffer2, 102400, 0);
	printf("Buffer 2 %s\n", buffer2);
}

int main()
{
	sfd = connect_to_server(80);
	post_data = g_strdup("{\"words\":\"a\", \"mimetypes\": [\"application/contacts\"] }\r\n\r\n");

//	gchar *buffer = g_strdup_printf("POST /services/google/search HTTP/1.1\r\nHost: localhost:1024\r\nConnection: keep-alive\r\nContent-Length: %d\r\nUser-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US)\r\n\r\n", strlen(post_data));
	gchar *buffer = g_strdup_printf("GET /services/google/contacts HTTP/1.1\r\nHost: localhost:1024\r\nConnection: keep-alive\r\nUser-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US)\r\n\r\n");
	send_to_server(sfd, buffer, strlen(buffer));

	g_timeout_add(1000, idle_func, NULL);
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
	return 0;
}
