Stratus- libStratus

	Introduction


LibStratus is very tiny library which acts as a basic HTTP/Web-socket based server which can accept connections over unix sockets listening over any port or over an already established connection with fd's transferred through 'open file descriptor transfer' concept. This library is designed with a goal to fit as a service side library for mod_fd_transfer based plugin architecture and their service processes. Along with its major purpose it is also very useful as basic HTTP/Web-socket server. Knowledge about Lighttpd's mod_fd_transfer and open file descriptor concepts are prerequisites for this document.
	Purpose and requirements of libStratus

Primary purpose of libStratus is to be the service side library for mod_fd_transfer compliant back-end service programs. This library basically would link to back-end service processes and allow those processes to accept requests from Lighttpd over mod_fd_transfer protocol. LibStratus also need to provide very minimal HTTP and web-socket protocol parsers. This library also need to allow the service process to have its own event loop and bind its fd accepting server to the process's event loop. This library also needs to be capable of handling multiple requests in parallel and allow the service process to respond to them out of order. Besides being the service side library for mod_fd_transfer based services it also acts as minimal HTTP/Web-socket server for other general purpose use. 
	
	Internals of libStratus and Architecture

LibStratus is a library which allows you to create a server which would accept requests over a unix socket. This Unix socket could either listen  for requests on any configured port or on pre-established connection and accept open Fd(s). LibStratus allows you to also bind this server created to the calling process's event loop. Currently only glib systems GMainLoop is supported as it relies on the IO-Channel concepts of glib.   LibStratus allows you to create three different types of server. A server running on a port, server running from a socket or server running from already opened fd. The first one is a simple INET server while the other two are UNIX servers. Currently the Unix server's are configured hard-coded to always look for fd transfer messages so as to be compliant to mod_fd_transfer based systems. While the INET server is configured to directly read requests over a port. We are very passionate about providing support for both types of request handling and both types of servers. These servers currently support requests of type HTTP and websocket protocol (75 http://tools.ietf.org/html/draft-hixie-thewebsocketprotocol-75 and 76 http://tools.ietf.org/html/draft-hixie-thewebsocketprotocol-76 only) as most browsers support web-socket protocol 75-76 currently.  LibStratus handles requests in asynchronous manner. So service process registers for a callback for requests and requests come to the process as a call to the callback function. LibStratus allows you to add specific handlers for specific protocol requests(HTTP/WS). On arrival of any request libStratus decodes the request's protocol and parses the appropriate header and then calls the user's callback. There is also a provision to add a separate callback for specific connection itself. This is quite useful for web-socket connections. LibStratus completely works on asynchronous mechanism by utilizing the giochannel's watch concept. It adds a watch on server socket as well as every connection socket and as this watch is tied to the user's gmain loop any request connection or data received event occurs asynchronously and in the main loop's context. This asynchronous behavior of LibStratus allows users to accept multiple connections at the same time and keep them alive for a long time and then respond to them on their own will at their own speed. This allows users to respond to requests asynchronously. 

Asynchronous response sending takes place with the help of unique request connection identifier. With every request LibStratus creates a unique identifier of the request and passes that in the user's callback. User can wish to cache this identifier. Once the user program is ready to respond to this request it may do so by calling APIs of LibStratus to write to that connection as a response and pass the same identifier for reference. All operations on that connection/request require that unique identifier. Users have to specifically close these connections/requests by calling close API provided.



	LibStratus data structures and API descriptions
Structures

1) typedef enum _STRATUS_CONN_PROTOCOLS {
	CONN_PROTO_UNKNOWN,
	CONN_PROTO_HTTP,
	CONN_PROTO_WS,
	CONN_PROTO_HTTPS,
	CONN_PROTO_WSS,
	CONN_PROTO_MAX
}STRATUS_CONN_PROTOCOLS;
These define the protocol of the connection. 

2) typedef enum _STRATUS_HTTP_METHOD {
	HTTP_METHOD_UNKNOWN,
	HTTP_METHOD_GET,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_DELETE,
	HTTP_METHOD_OPTIONS,
	HTTP_METHOD_MAX
}STRATUS_HTTP_METHOD;
These define the Http method for every request. Please make sure that the plugins look at these correctly.
3) typedef enum _STRATUS_CONN_STATE{
	CONN_STATE_UNKNOWN,
	CONN_STATE_HTTP_HEADERS,
	CONN_STATE_ESTABLISHED,
	CONN_STATE_WRITE_HEADERS,
	CONN_STATE_WRITE,
	CONN_STATE_CLOSED,
	CONN_STATE_ERROR
}STRATUS_CONN_STATE;
These define the connection state. The user callback will only be called when the connection state has reached ESTABLISHED.

4) typedef struct _ServerPacket{
		gchar *buffer;
		gssize size;
}ServerPacket;
This is a general packet structure, with buffer and size as it could be binary packet.

5) typedef struct _StratusWebSocketHeader
{
	gint handshake_protocol_id; /** It could be 76 which is the one with secure keys and 75 which is the one without any keys*/
	ServerPacket tokens[TOKEN_COUNT];
}StratusWebSocketHeader;
This structure represents the headers that are received in websocket protocol.

6) typedef struct _StratusHttpHeader
{
	GHashTable *headers;
}StratusHttpHeader;
This structure maintains the headers that received in Http requests.

7) typedef struct {
	/** HEADER */
	gchar uri;   /* uri of the request , /services/google/login*/
	STRATUS_HTTP_METHOD  http_method; /* http method GET/POST/PUT/DELETE*/
	gchar host; /* host header string if set in the request*/
	gchar *origin; /* Origin header string if set in the request*/
	gchar *connection; /* connection header string if set in the request */
	gchar *upgrade; /* Upgrade header string if set in request*/
	gchar *content_type; /* Content type string if set in request*/
	guint content_length; /* Content lenght */
	union {
		StratusWebSocketHeader *ws_header;
		StratusHttpHeader *http_header;
	}header; /*Either the request will be http or ws so depending on the the headers are set.*/
} Request;
This is the main request structure. This is passed as argument in the callback and has all the fields that represents the request.

API Definitions

1)
Description: Create Server with port number. It takes port number as input and returns the StratusServer object as return value. It creates inet type server.
Signature: StratusServer* stratus_create_server(guint port);

2)
Description: Create Server from Unix socket path. It takes socket path string as input and returns the StratusServer object as return value. It creates unix type server.
Signature: StratusServer* stratus_create_unix_server(gchar *socket_path);

3)
Description: Create Server with already opened file descriptor. It takes file descriptor as input and returns the StratusServer object as return value. It creates Unix type server.
Signature: StratusServer* stratus_create_unix_server_with_fd(gint fd);

4)
Description: This is basically the callback function declaration. So user must pass a function pointer of this type and every time a request comes this callback would get called. 
Parameters: 
connection_id
Unique identifier of this connection/request. All other operations are performed on this identifier.
State
represents the state of the request. Please look at  STRATUS_CONN_STATE enum to understand the details of each state. 
Request
This represents the request details. The structure is defined above.
Body
This represents the body part of the request if any. For websocket request all the websocket data would come in this variable. 
Userdata
This is the callback data passes while registering the callback.

Signature: typedef void (*StratusServerCallBack)(gint connection_id,  STRATUS_CONN_STATE state, Request *request, ServerPacket *body, gpointer userdata);

5)
Description: This function allows you to add a callback function for requests of different protocols. 
Parameters:
Server
StratusServer that was returned in create object.
Protocol
Protocol for which you want to add the callback. (HTTP/WS)
callback
Callback function to be called on every request.
Userdata
Extra data pointer to pass to callback.

Signature: void stratus_server_add_handler (StratusServer* server, STRATUS_CONN_PROTOCOLS protocol, StratusServerCallBack callback, gpointer userdata);

6)
Description: This API allows you to add specific handler for a connection. Use ful in case of websocket.
Parameters:
Server
StratusServer that was returned in create object.
Connection_id 
unique connection identifier for this connection.
callback
Callback function to be called on every request.
Userdata
Extra data pointer to pass to callback.

Signature: void stratus_server_add_connection_handler (StratusServer* server, gint connection_id, StratusServerCallBack callback, gpointer userdata);

7) 
Description: This API allows you to add headers to the response. Headers include the http code of the response as well as other headers. This API should be called only once per connection. Only once you can send the headers to HTTP requests. This API also allows you to send different Http response codes also. 
Parameters:
Server
StratusServer that was returned in create object.
Connection_id 
unique connection identifier for this connection.
http_code
http code for the response. Ex: 200 , 404 etc.
Headers
Array of gchar * header strings. Can also be NULL if no extra headers passed. (In our case we would be sending application/json in most of our plugin's responses)
Returns
returns 0 on SUCCESS and -1 on ERROR

Signature: gint stratus_connection_write_headers (StratusServer *server, gint connection_id, gint http_code, gchar **headers);


8)
Description: This API allows you to write data response back to the request. This API also takes binary data response hence the size needs to be passed. If this API is called without calling the write headers API then it would by default send 200 ok response. 
Parameters: 
Server
StratusServer that was returned in create object.
Connection_id
unique connection identifier for this connection.
Data
data response.
Size
Size of the data response.
Returns
number of bytes written.

Signature: gint stratus_connection_write (StratusServer *server, gint connection_id, gchar *data, gsize size);


9)
Description: This API is similar to above API but it allows you to pass variable argument list and format string. So all the %s %d %c %p etc fields work here. It has similar prototype as that of printf. This API is defined so that our current plugins can directly replace their printfs with this function and add first 2 arguments. As this API takes variable argument list and format string it does not work with binary data.
Parameters: 
Server
StratusServer that was returned in create object.
Connection_id
unique connection identifier for this connection.
Format
string format.
...
Variable argument list.
Returns
Number of bytes written

Signature: gint stratus_connection_write_printf (StratusServer *server, gint connection_id, const gchar *format, ...);

10) 
Description: This API allows you to close any connection. 
Parameters: 
Server
StratusServer that was returned in create object.
Connection_id
unique connection identifier for this connection.

Signature: void stratus_connection_close (StratusServer *server, gint connection_id);

These API does take care of adding CORS headers, except for pre-flight request. Cause in that case we may wish to do some processing. Pre-flight requests needs to be handled by plugin.

License: BSD
Copyright (c) 2011, Motorola Mobility, Inc

All Rights Reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
Neither the name of Motorola Mobility nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


