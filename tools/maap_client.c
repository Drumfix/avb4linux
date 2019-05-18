/******************************************************************************

  Copyright (c) 2012, Intel Corporation
  Copyright (c) 2014, Parrot SA
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

/*
 * simple MAAP client
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include <pthread.h>
#include <poll.h>

#include <maap_client.h>

/* global defines */

#define MAAP_PORT_DEFAULT 15364

static int sock_fd;

int send_maap_cmd(Maap_Cmd* cmd)
{
	struct sockaddr_in addr;
	socklen_t addr_len;

	if (sock_fd == -1)
		return -1;
	if (cmd == NULL)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MAAP_PORT_DEFAULT);
	inet_aton("127.0.0.1", &addr.sin_addr);
	addr_len = sizeof(addr);
	return send(sock_fd, cmd, sizeof(Maap_Cmd), 0);
}

int receive_maap_response(Maap_Notify* response)
{
	struct sockaddr_in addr;
	socklen_t addr_len;

	if (sock_fd == -1)
		return -1;
	if (response == NULL)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MAAP_PORT_DEFAULT);
	inet_aton("127.0.0.1", &addr.sin_addr);
	addr_len = sizeof(addr);
	return recv(sock_fd, response, sizeof(Maap_Notify), 0);
}

int maap_client_init(struct maap_record* ctx)
{
	struct sockaddr_in addr;
	int rc;

	struct addrinfo hints, *ai, *p;
	int ret;

	fd_set master, read_fds;
	int fdmax;

	char recvbuffer[200];
	int recvbytes;
	Maap_Cmd recvcmd;
	int exit_received = 0;

	/* Create a localhost socket. */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	if ((ret = getaddrinfo("localhost", "15364", &hints, &ai)) != 0) {
		printf("getaddrinfo failure %s", gai_strerror(ret));
		return -1;
	}

	for(p = ai; p != NULL; p = p->ai_next) {
                sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(sock_fd == -1) {
			continue;
		}
		ret = connect(sock_fd, p->ai_addr, p->ai_addrlen);
		if (ret == -1) {
			close(sock_fd);
			continue;
		} else {
			break;
		}
	}

	if (p == NULL) {
		printf("Unable to connect to the daemon, error %d (%s)", errno, strerror(errno));
		freeaddrinfo(ai);
		return -1;
	}

	freeaddrinfo(ai);

	Maap_Cmd cmd;
        Maap_Notify response;

        cmd.kind = MAAP_CMD_INIT;
        cmd.id = 0;
        cmd.start = ctx->base;
        cmd.count = ctx->count;

        errno = 0;

        rc = send_maap_cmd(&cmd);
        rc = receive_maap_response(&response);

        if (response.result != MAAP_NOTIFY_ERROR_NONE && 
            response.result != MAAP_NOTIFY_ERROR_ALREADY_INITIALIZED)
        {
           return -1;
        }

        cmd.kind = MAAP_CMD_RESERVE;
        cmd.id = 1;
        cmd.start = ctx->base;
        cmd.count = ctx->count;

        rc = send_maap_cmd(&cmd);
        rc = receive_maap_response(&response);

        if (response.result != MAAP_NOTIFY_ERROR_NONE && 
            response.result != MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE)
        {
           printf("error: %d\n", response.result);
           return -1;
        }

        cmd.start = ctx->base = response.start;
        cmd.count = ctx->count = response.count;

	return rc;

 out:	if (sock_fd != -1)
		close(sock_fd);
	sock_fd = -1;
	return -1;
}

int maap_acquire_adresses(struct maap_record* ctx)
{
}

int maap_release_adresses(struct maap_record* ctx)
{
}

