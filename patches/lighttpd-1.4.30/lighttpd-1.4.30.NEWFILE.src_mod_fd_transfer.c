/*
 * Copyright (c) 2011, Motorola Mobility, Inc
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *  - Neither the name of Motorola Mobility nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */

#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

/**
 * this is a fd_transfer plugin for lighttpd. This plugin would specifically work only on linux systems 
 */
typedef struct {
	/* the key that is used to reference this value */
	buffer *service_name;

	/* config */
	/*
	 * Unix Domain Socket
	 *
	 * instead of TCP/IP we can use Unix Domain Sockets
	 * - more secure (you have fileperms to play with)
	 * - more control (on locally)
	 * - more speed (no extra overhead)
	 */
	buffer *unixsocket;

	/* if socket is local we can start the fd transfer
	 * process ourself
	 *
	 * bin-path is the path to the binary
	 */
	buffer *bin_path;

	/* bin-path is set bin-environment is taken to
	 * create the environement before starting the
	 * FDT process
	 *
	 */
	array *bin_env;

	int fdt_sfd;  /* Socket file descriptor*/

	pid_t pid;   /* PID of the spawned process (0 if not spawned locally) */

	enum {
		PROC_STATE_UNSET,    /* unset-phase */
		PROC_STATE_INIT,    /* init-phase */
		PROC_STATE_RUNNING,  /* alive */
		PROC_STATE_DIED,     /* marked as dead, should be restarted */
		PROC_STATE_KILLED    /* was killed as we don't have the load anymore */
	} state;

} fdt_server;

typedef struct {
	fdt_server **servers;

	size_t used;
	size_t size;
} fdt_services;

typedef struct {
	char **ptr;

	size_t size;
	size_t used;
} char_array;
/* plugin config for all request/connections */

typedef struct {
	fdt_services *services;
} plugin_config;

/* generic plugin data, shared between all connections */
typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf; /* this is only used as long as no handler_ctx is setup */
} plugin_data;

static fdt_server *fdt_server_init() {
	fdt_server *f;

	f = calloc(1, sizeof(*f));

	f->service_name = buffer_init();
	f->unixsocket = buffer_init();
	f->bin_path = buffer_init();
	f->bin_env = array_init();

	return f;
}

static void fdt_server_free(fdt_server *s) {
	if (!s) return;

	buffer_free(s->service_name);
	buffer_free(s->unixsocket);
	buffer_free(s->bin_path);
	array_free(s->bin_env);

	free(s);
}

static fdt_services *fdt_services_init() {
	fdt_services *f;

	f = calloc(1, sizeof(*f));

	return f;
}

static void fdt_services_free(fdt_services *f) {
	size_t i;

	if (!f) return;

	for (i = 0; i < f->used; i++) {
		fdt_server *fdt_s;
		fdt_s = f->servers[i];
		fdt_server_free(fdt_s);
	}
	free(f);
}

/* init the plugin data */
INIT_FUNC(mod_fd_transfer_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_fd_transfer_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;

		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];
			if (!s) continue;

			fdt_services_free(s->services);
			free(s);
		}
		free(p->config_storage);
	}
	free(p);

	return HANDLER_GO_ON;
}

static int env_add(char_array *env, const char *key, size_t key_len, const char *val, size_t val_len) {
	char *dst;
	size_t i;

	if (!key || !val) return -1;

	dst = malloc(key_len + val_len + 3);
	memcpy(dst, key, key_len);
	dst[key_len] = '=';
	memcpy(dst + key_len + 1, val, val_len);
	dst[key_len + 1 + val_len] = '\0';

	for (i = 0; i < env->used; i++) {
		if (0 == strncmp(dst, env->ptr[i], key_len + 1)) {
			/* don't care about free as we are in a forked child which is going to exec(...) */
			/* free(env->ptr[i]); */
			env->ptr[i] = dst;
			return 0;
		}
	}

	if (env->size == 0) {
		env->size = 16;
		env->ptr = malloc(env->size * sizeof(*env->ptr));
	} else if (env->size == env->used + 1) {
		env->size += 16;
		env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
	}

	env->ptr[env->used++] = dst;

	return 0;
}

static int parse_binpath(char_array *env, buffer *b) {
	char *start;
	size_t i;
	/* search for spaces */

	start = b->ptr;
	for (i = 0; i < b->used - 1; i++) {
		switch(b->ptr[i]) {
		case ' ':
		case '\t':
			/* a WS, stop here and copy the argument */

			if (env->size == 0) {
				env->size = 16;
				env->ptr = malloc(env->size * sizeof(*env->ptr));
			} else if (env->size == env->used) {
				env->size += 16;
				env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
			}

			b->ptr[i] = '\0';

			env->ptr[env->used++] = start;

			start = b->ptr + i + 1;
			break;
		default:
			break;
		}
	}

	if (env->size == 0) {
		env->size = 16;
		env->ptr = malloc(env->size * sizeof(*env->ptr));
	} else if (env->size == env->used) { /* we need one extra for the terminating NULL */
		env->size += 16;
		env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
	}

	/* the rest */
	env->ptr[env->used++] = start;

	if (env->size == 0) {
		env->size = 16;
		env->ptr = malloc(env->size * sizeof(*env->ptr));
	} else if (env->size == env->used) { /* we need one extra for the terminating NULL */
		env->size += 16;
		env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
	}

	/* terminate */
	env->ptr[env->used++] = NULL;

	return 0;
}

static void reset_signals(void) {
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_DFL);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_DFL);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_DFL);
#endif
	signal(SIGHUP, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
}

static int fdt_service_insert(fdt_services *service, buffer *key, fdt_server *s_server) {

	size_t i;

	for (i = 0; i < service->used; i++) {
		if (buffer_is_equal(key, service->servers[i]->service_name)) {
			break;
		}
	}

	if (i == service->used) {
		if (service->size == 0) {
			service->size = 8;
			service->servers = malloc(service->size * sizeof(*(service->servers)));
			assert(service->servers);
		} else if (service->used == service->size) {
			service->size += 8;
			service->servers = realloc(service->servers, service->size * sizeof(*(service->servers)));
			assert(service->servers);
		}
		service->servers[service->used++] = s_server;
	} else {
		service->servers[i] = s_server;
	}

	return 0;
}

static handler_t fdt_handle_fdevent(server *srv, void *ctx, int revents) {
	int status = 0;
	fdt_server *s_server = ctx;

	if (revents & FDEVENT_HUP) {
		/** In case the child process died then we would get a SIG_HUP from child on that socket and we would end up here. We need to read the child process's return status so then child proc can exit from process table. */
		switch(waitpid(s_server->pid, &status, WNOHANG)) {
		case 0:
			/* child is still alive */
			log_error_write(srv, __FILE__, __LINE__, "sd",
						"child is still alive ? WTF pid:", s_server->pid);
			break;
		case -1:
			break;
		default:
			/** In the default case we would just get the child's proc id return and we would stop the handler for HUP event*/
			fdevent_event_del(srv->ev, NULL, s_server->fdt_sfd);
			s_server->state = PROC_STATE_DIED;
			close(s_server->fdt_sfd);

			/* the child should not terminate at all */
			if (WIFEXITED(status)) {
				log_error_write(srv, __FILE__, __LINE__, "sdsd",
						"child exited, pid:", s_server->pid,
						"status:", WEXITSTATUS(status));
			} else if (WIFSIGNALED(status)) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"child signaled:",
						WTERMSIG(status));
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"child died somehow:",
						status);
			}
		}
	}
	return HANDLER_FINISHED;
}

static int fdt_spawn_connection(server *srv,
				 fdt_server *s_server) {

	int status;
	struct timeval tv = { 0, 100 * 1000 };
	struct sockaddr_un fdt_addr_un;
	struct sockaddr *fdt_addr;

	socklen_t servlen;

	if (buffer_is_empty(s_server->unixsocket)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
			"Socket path missing");
		return -1;
	}

	memset(&fdt_addr, 0, sizeof(fdt_addr));

	fdt_addr_un.sun_family = AF_UNIX;
	strcpy(fdt_addr_un.sun_path, s_server->unixsocket->ptr);

	servlen = s_server->unixsocket->used + sizeof(fdt_addr_un.sun_family);
	fdt_addr = (struct sockaddr *) &fdt_addr_un;

	/** We first try to connect to the backend to see if it is already present or not. If it is present then we do not need to fork that process*/
	if (-1 == (s_server->fdt_sfd = socket(AF_UNIX, SOCK_STREAM, 0))) {
		log_error_write(srv, __FILE__, __LINE__, "ss",
				"failed:", strerror(errno));
		return -1;
	}

	if (-1 == connect(s_server->fdt_sfd, fdt_addr, servlen)) {
		/* server is not up, spawn it  */
		log_error_write(srv, __FILE__, __LINE__, "sb",
			"spawning a new fdt proc, socket:", s_server->unixsocket);

		pid_t child;
		int val;

		if (errno != ENOENT &&
		    !buffer_is_empty(s_server->unixsocket)) {
			unlink(s_server->unixsocket->ptr);
		}

		close(s_server->fdt_sfd);

		/** here we create a socket bind to address and then go in listen state. Reason behind this is that the server process is ideally suppose to start and then listen on this socket but it so happens that there is a race condition that lighttpd does connect before the child could do listen so in that case connect fails 2nd time so the best way to do is do the listen in parent itself and then do fork and then after fork in child process that listen socket will still be open. While in parent process we can close that socket and  reopen it again and do connect*/
		/* reopen socket */
		if (-1 == (s_server->fdt_sfd = socket(AF_UNIX, SOCK_STREAM, 0))) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"socket failed:", strerror(errno));
			return -1;
		}

		val = 1;
		if (setsockopt(s_server->fdt_sfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"socketsockopt failed:", strerror(errno));
			return -1;
		}

		/* create socket */
		if (-1 == bind(s_server->fdt_sfd, fdt_addr, servlen)) {
			log_error_write(srv, __FILE__, __LINE__, "sbs",
				"bind failed for:",
				s_server->service_name,
				strerror(errno));
			return -1;
		}

		if (-1 == listen(s_server->fdt_sfd, 1024)) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"listen failed:", strerror(errno));
			return -1;
		}

		switch ((child = fork())) {
		case 0: {

			size_t i = 0;
			char *c;
			char_array env;
			char_array arg;

			/* create environment */
			env.ptr = NULL;
			env.size = 0;
			env.used = 0;

			arg.ptr = NULL;
			arg.size = 0;
			arg.used = 0;

/** build clean enviornment basically says what all environ variables do you want to copy from the environ of the lighttpd and pass it to child process. Currently we dont need this right now but we mihgt need it later on
			// build clean environment 
			if (s_server->bin_env_copy->used) {
				for (i = 0; i < s_server->bin_env_copy->used; i++) {
					data_string *ds = (data_string *)s_server->bin_env_copy->data[i];
					char *ge;

					if (NULL != (ge = getenv(ds->value->ptr))) {
						env_add(&env, CONST_BUF_LEN(ds->value), ge, strlen(ge));
					}
				}
			} else {
				for (i = 0; environ[i]; i++) {
					char *eq;

					if (NULL != (eq = strchr(environ[i], '='))) {
						env_add(&env, environ[i], eq - environ[i], eq+1, strlen(eq+1));
					}
				}
			}*/

			for (i = 0; environ[i]; i++) {
				char *eq;

				if (NULL != (eq = strchr(environ[i], '='))) {
					env_add(&env, environ[i], eq - environ[i], eq+1, strlen(eq+1));
				}
			}

			/* create environment */
			for (i = 0; i < s_server->bin_env->used; i++) {
				data_string *ds = (data_string *)s_server->bin_env->data[i];

				env_add(&env, CONST_BUF_LEN(ds->key), CONST_BUF_LEN(ds->value));
			}

			/** The socket file descirtor handle is passed in environmental variables to the client.*/
			char *env_key = calloc(1, sizeof(char)*10);
			char *env_value = calloc(1, sizeof(char)*4);

			memcpy(env_key, "SOCKET_FD", 9);
			sprintf(env_value,"%d", s_server->fdt_sfd);
			env_add(&env, env_key, 9, env_value, strlen(env_value));

/*			log_error_write(srv, __FILE__, __LINE__, "ss",
					"socket fd in child process is", env_value);*/
			free(env_key);
			free(env_value);

			env.ptr[env.used] = NULL;

			parse_binpath(&arg, s_server->bin_path);

			/* chdir into the base of the bin-path,
			 * search for the last / */
			if (NULL != (c = strrchr(arg.ptr[0], '/'))) {
				*c = '\0';

				/* change to the physical directory */
				if (-1 == chdir(arg.ptr[0])) {
					*c = '/';
					log_error_write(srv, __FILE__, __LINE__, "sss", "chdir failed:", strerror(errno), arg.ptr[0]);
				}
				*c = '/';
			}

			reset_signals();

			/* exec the fdt process */
			execve(arg.ptr[0], arg.ptr, env.ptr);

			log_error_write(srv, __FILE__, __LINE__, "sbs",
					"execve failed for:", s_server->bin_path, strerror(errno)); 

			exit(errno);

			break;
		}
		case -1:
			/* error */
			break;
		default:
			/* father */

			/* wait */
			select(0, NULL, NULL, NULL, &tv);

			switch (waitpid(child, &status, WNOHANG)) {
			case 0:
				/* child still running after timeout, good */
				break;
			case -1:
				/* no PID found ? should never happen */
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"pid not found:", strerror(errno));
				return -1;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"the fdt-transfer-backend", s_server->bin_path, "failed to start:");
				/* the child should not terminate at all */
				if (WIFEXITED(status)) {
					log_error_write(srv, __FILE__, __LINE__, "sdb",
							"child exited with status",
							WEXITSTATUS(status), s_server->bin_path);
				} else if (WIFSIGNALED(status)) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"terminated by signal:",
							WTERMSIG(status));

					if (WTERMSIG(status) == 11) {
						log_error_write(srv, __FILE__, __LINE__, "s",
								"to be exact: it segfaulted, crashed, died, ... you get the idea." );
					}
				} else {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"child died somehow:",
							status);
				}
				return -1;
			}

			/* register process */
			s_server->pid = child;

			close(s_server->fdt_sfd);

			/* reopen socket */
			if (-1 == (s_server->fdt_sfd = socket(AF_UNIX, SOCK_STREAM, 0))) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
					"socket failed:", strerror(errno));
				return -1;
			}
	
			if (-1 == connect(s_server->fdt_sfd, fdt_addr, servlen)) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
					"connect failed 2nd time This should never happen:", strerror(errno));
				return -1;
			}

			fdevent_register(srv->ev, s_server->fdt_sfd, fdt_handle_fdevent, s_server);
			if (-1 == fdevent_fcntl_set(srv->ev, s_server->fdt_sfd)) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"fcntl failed:", strerror(errno));
	
				return HANDLER_ERROR;
			}

			fdevent_event_set(srv->ev, NULL, s_server->fdt_sfd, FDEVENT_HUP);

			break;
		}
	} else {
		s_server->pid = 0;

		log_error_write(srv, __FILE__, __LINE__, "sb",
				"(debug) socket is already used; won't spawn:",
				s_server->service_name);
	}

	s_server->state = PROC_STATE_RUNNING;

	return 0;
}

/* handle plugin config and check values */
/**
fd_transfer.services = ( "/services/google/" =>
			(
			"socket" => "/tmp/fdt-google.socket",
			"bin-path" => "/usr/local/bin/google"
			),
		      "/services/music/" =>
			(
			"socket" => "/tmp/fdt-music.socket",
			"bin-path" => "/usr/local/bin/music"
			)
		    )
*/

SETDEFAULTS_FUNC(mod_fd_transfer_set_defaults) {
	plugin_data *p = p_d;
	data_unset *du;
	size_t i = 0;

	config_values_t cv[] = {
		{ "fd_transfer.services",             NULL, T_CONFIG_LOCAL, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                             NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;
		array *ca;

		s = calloc(1, sizeof(plugin_config));
		s->services    = fdt_services_init();

		cv[0].destination = s->services;

		p->config_storage[i] = s;
		ca = ((data_config *)srv->config_context->data[i])->value;

		if (0 != config_insert_values_global(srv, ca, cv)) {
			return HANDLER_ERROR;
		}

		if (NULL != (du = array_get_element(ca, "fd_transfer.services"))) {
			size_t j;
			data_array *da = (data_array *)du;

			if (du->type != TYPE_ARRAY) {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"unexpected type for key: ", "fd_transfer.server", "array of strings");

				return HANDLER_ERROR;
			}

			for (j = 0; j < da->value->used; j++) {
				data_array *da_server = (data_array *)da->value->data[j];

				if (da->value->data[j]->type != TYPE_ARRAY) {

					log_error_write(srv, __FILE__, __LINE__, "sssbs",
							"unexpected type for key: ", "fd_transfer.server",
							"[", da->value->data[j]->key, "](string)");

					return HANDLER_ERROR;
				}

				fdt_server *s_server;

				config_values_t fcv[] = {
					{ "socket",            NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
					{ "bin-path",          NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 1 */
					{ "bin-environment",   NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },        /* 2 */

					{ NULL,                NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
				};

				if (da_server->type != TYPE_ARRAY) {
					log_error_write(srv, __FILE__, __LINE__, "ssSBS",
							"unexpected type for key:",
							"fdt_transfer.server",
							"[", da_server->key, "](string)");

					return HANDLER_ERROR;
				}

				s_server = fdt_server_init();
				buffer_copy_string_buffer(s_server->service_name, da_server->key);

				fcv[0].destination = s_server->unixsocket;
				fcv[1].destination = s_server->bin_path;
				fcv[2].destination = s_server->bin_env;

				if (0 != config_insert_values_internal(srv, da_server->value, fcv)) {
					return HANDLER_ERROR;
				}

				log_error_write(srv, __FILE__, __LINE__, "sbsbsbs",
							"[socket-", s_server->unixsocket, "], [bin-path-", s_server->bin_path, "], [key-", da_server->key, "]");

				if (buffer_is_empty(s_server->unixsocket)) {
					log_error_write(srv, __FILE__, __LINE__, "sbsbs",
							"socket have to be set in:",
							da->key, "= (",
							da_server->key, " ( ...");

					return HANDLER_ERROR;
				}

				struct sockaddr_un un;

				if (s_server->unixsocket->used > sizeof(un.sun_path) - 2) {
					log_error_write(srv, __FILE__, __LINE__, "sbsbs",
							"unixsocket is too long in:",
							da->key, "= (",
							da_server->key, " ( ...");

					return HANDLER_ERROR;
				}

				s_server->state = PROC_STATE_INIT;

				/* if extension already exists, take it */
				fdt_service_insert(s->services, da_server->key, s_server);
			}
		}
	}
	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_fd_transfer_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(services);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("fd_transfer.service"))) {
				PATCH(services);
			}
		}
	}

	return 0;
}
#undef PATCH

int mod_fd_transfer_send_fd(int server_sock, const int fd, char *data)
{
	struct msghdr msghdr;
	struct iovec iovec_ptr;
	struct cmsghdr *cmsg;
	char msg_cntrl_buffer[82];
	
	iovec_ptr.iov_base = data;
	iovec_ptr.iov_len = strlen(data);
	msghdr.msg_name = NULL;
	msghdr.msg_namelen = 0;
	msghdr.msg_iov = &iovec_ptr;
	msghdr.msg_iovlen = 1;
	msghdr.msg_flags = 0;
	msghdr.msg_control = msg_cntrl_buffer;
	msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);
	cmsg = CMSG_FIRSTHDR(&msghdr);
	cmsg->cmsg_len = msghdr.msg_controllen;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
		((int *)CMSG_DATA(cmsg))[0] = fd;
	return(sendmsg(server_sock, &msghdr, 0) >= 0 ? 0 : -1);
}

static handler_t fdt_check_service(server *srv, connection *con, void *p_d) {
	plugin_data *p = p_d;
	size_t s_len;
	size_t k;
	buffer *fn;
	fdt_server *m_server = NULL;

	if (con->mode != DIRECT) return HANDLER_GO_ON;

	/* Possibly, we processed already this request */
	if (con->file_started == 1) return HANDLER_GO_ON;

	fn = con->request.uri;

	if (buffer_is_empty(fn)) return HANDLER_GO_ON;

	s_len = fn->used - 1;

	mod_fd_transfer_patch_connection(srv, con, p);

	/* check if service matches */
	for (k = 0; k < p->conf.services->used; k++) {
		size_t ct_len; /* length of the config entry */
		fdt_server *s_server = p->conf.services->servers[k];

		if (s_server->service_name->used == 0) continue;

		ct_len = s_server->service_name->used - 1;

		if ((ct_len <= s_len) && (0 == strncmp(fn->ptr, s_server->service_name->ptr, ct_len))) {
			/* check service in the form "/google/" */
			m_server = s_server;
			break;
		}
	}
	/* service doesn't match */
	if (NULL == m_server) {
		return HANDLER_GO_ON;
	}

	con->mode = p->id;

	if(m_server->state == PROC_STATE_INIT || m_server->state == PROC_STATE_DIED) {
		if(-1 == fdt_spawn_connection(srv, m_server)) {
			log_error_write(srv, __FILE__, __LINE__, "s", "Could not spawn connection");
			close(m_server->fdt_sfd);
			con->file_finished = 1;
			con->http_status = 500;
			return HANDLER_ERROR;
		}
	}

	chunk *c;
	chunkqueue *cq = con->read_queue;
	buffer *data = buffer_init();
	buffer_copy_string_buffer(data, con->request.request);

	for (c = cq->first; c; c = c->next) {
		buffer b;

		b.ptr = c->mem->ptr + c->offset;
		b.used = c->mem->used - c->offset;

		buffer_append_string_buffer(data, &b);

		/* the whole packet was copied */
		c->offset = c->mem->used - 1;
	}

//	log_error_write(srv, __FILE__, __LINE__, "sb", "Request Data - ", data);

	mod_fd_transfer_send_fd(m_server->fdt_sfd, con->fd, data->ptr);
	con->file_finished = 1;

	buffer_free(data);
	chunkqueue_remove_finished_chunks(cq);

	return HANDLER_FORWARD;
}

/* this function is called at dlopen() time and inits the callbacks */
int mod_fd_transfer_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("fd_transfer");

	p->init        = mod_fd_transfer_init;
	p->handle_request_raw  = fdt_check_service;
	p->set_defaults  = mod_fd_transfer_set_defaults;
	p->cleanup     = mod_fd_transfer_free;

	p->data        = NULL;

	return 0;
}
