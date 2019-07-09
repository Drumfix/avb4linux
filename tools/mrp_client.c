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

#include "mrp_client.h"

/* global variables */

volatile int mrp_okay;
volatile int mrp_error = 0;
pthread_t monitor_thread;
pthread_attr_t monitor_attr;


/*
 * private
 */

static int m_sock = -1;
static talker_cb_t m_talker_cb = NULL;
static listener_cb_t m_listener_cb = NULL;
static domain_cb_t m_domain_cb = NULL;

int mrp_client_init(talker_cb_t talker_cb,
                    listener_cb_t listener_cb,
                    domain_cb_t domain_cb)
{
	m_talker_cb = talker_cb;
	m_listener_cb = listener_cb;
	m_domain_cb = domain_cb;

	return 0;
}

int send_mrp_msg(char *notify_data, int notify_len)
{
	struct sockaddr_in addr;
	socklen_t addr_len;

	if (m_sock == -1)
		return -1;
	if (notify_data == NULL)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MRPD_PORT_DEFAULT);
	inet_aton("127.0.0.1", &addr.sin_addr);
	addr_len = sizeof(addr);
	return sendto(m_sock, notify_data, notify_len, 0,
			 (struct sockaddr *)&addr, addr_len);
}

int process_mrp_msg(char *buf, int buflen)
{
	struct mrp_talker_ctx talker_ctx;
	struct mrp_listener_ctx listener_ctx;
	struct mrp_domain_ctx domain_ctx;

	/*
	 * 1st character indicates application
	 * [MVS] - MAC, VLAN or STREAM
	 */
	int i, j, k, l;
        unsigned int id;
	unsigned int substate;

	k = 0;
 next_line:if (k >= buflen)
		return 0;

    printf("%s\n", buf);

	switch (buf[k]) {
	case 'E':
		printf("%s from mrpd\n", buf);
		fflush(stdout);
		mrp_error = 1;
		break;
	case 'O':
		mrp_okay = 1;
		break;
	case 'M':
	case 'V':
		printf("%s unhandled from mrpd\n", buf);
		fflush(stdout);

		/* unhandled for now */
		break;
	case 'L':

		/* parse a listener attribute */
		i = k;
		while (buf[i] != 'D')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%d", &substate);
		while (buf[i] != 'S')
			i++;
		i += 2;		/* skip the ':' */
		for (j = 0; j < 8; j++) {
			sscanf(&(buf[i + 2 * j]), "%02x", &id);
			listener_ctx.stream_id[j] = (unsigned char)id;
		} 
                printf
		    ("FOUND STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
		     (unsigned char)listener_ctx.stream_id[0], 
                     (unsigned char)listener_ctx.stream_id[1],
		     (unsigned char)listener_ctx.stream_id[2], 
                     (unsigned char)listener_ctx.stream_id[3],
		     (unsigned char)listener_ctx.stream_id[4], 
                     (unsigned char)listener_ctx.stream_id[5],
		     (unsigned char)listener_ctx.stream_id[6], 
                     (unsigned char)listener_ctx.stream_id[7]);
		while (buf[i] != 'R')
			i++;
		i += 2;		/* skip the ':' */
		for (j = 0; j < 6; j++) {
			sscanf(&(buf[i + 2 * j]), "%02x", &id);
			listener_ctx.listener[j] = (unsigned char)id;
		} 

		switch (substate) {
		case 0:
			printf("with state ignore\n");
			break;
		case 1:
			printf("with state askfailed\n");
			break;
		case 2:
			printf("with state ready\n");
			break;
		case 3:
			printf("with state readyfail\n");
			break;
		default:
			printf("with state UNKNOWN (%d)\n", substate);
			break;
		}
		if (substate == MSRP_LISTENER_READY)
		{
                	listener_ctx.mode = MRP_JOIN;
                }
		else
		{
			listener_ctx.mode = MRP_LEAVE;
		}

		fflush(stdout);
		m_listener_cb(listener_ctx);

		/* try to find a newline ... */
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if (i == buflen)
			return 0;
		if (buf[i] == '\0')
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'D':
		i = k + 4;

		/* save the domain attribute */
		sscanf(&(buf[i]), "%d", &domain_ctx.id);
		while (buf[i] != 'P')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%d", &domain_ctx.priority);
		while (buf[i] != 'V')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%d", &domain_ctx.vid);

		m_domain_cb(domain_ctx);

		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if ((i == buflen) || (buf[i] == '\0'))
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'T':

		i = k;

		if (strncmp(buf, "SNE T:", 6) == 0 || strncmp(buf, "SJO T:", 6) == 0)
		{
			talker_ctx.mode = MRP_JOIN;
		}
		else
		{
			talker_ctx.mode = MRP_LEAVE;
		}
		l = 6; /* skip "Sxx T:" */
		while ((l < buflen) && ('S' != buf[l++]));
		if (l == buflen)
			return -1;
		l++;
		for(j = 0; j < 8 ; l+=2, j++)
		{
			sscanf(&buf[l],"%02x",&id);
			talker_ctx.stream_id[j] = (unsigned char)id;
		}
		l+=3;
		for(j = 0; j < 6 ; l+=2, j++)
		{
			sscanf(&buf[l],"%02x",&id);
			talker_ctx.dst_mac[j] = (unsigned char)id;
		}			
		l+=3;
		for(j = 0; j < 6 ; l+=2, j++)
		{
			sscanf(&buf[l],"%d", &talker_ctx.vid);
		}

		m_talker_cb(talker_ctx);

		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if ((i == buflen) || (buf[i] == '\0'))
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'S':

		/* handle the leave/join events */
		switch (buf[k + 4]) {
		case 'L':
			i = k + 5;
			while (buf[i] != 'D')
				i++;
			i += 2;	/* skip the ':' */
			sscanf(&(buf[i]), "%d", &substate);
			while (buf[i] != 'S')
				i++;
			i += 2;	/* skip the ':' */
			for (j = 0; j < 8; j++) {
				sscanf(&(buf[i + 2 * j]), "%02x", &id);
				listener_ctx.stream_id[j] = (unsigned char)id;
			} printf
			    ("EVENT on STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
			     (unsigned char)listener_ctx.stream_id[0], 
                             (unsigned char)listener_ctx.stream_id[1],
			     (unsigned char)listener_ctx.stream_id[2],
                             (unsigned char)listener_ctx.stream_id[3],
			     (unsigned char)listener_ctx.stream_id[4],
                             (unsigned char)listener_ctx.stream_id[5],
			     (unsigned char)listener_ctx.stream_id[6],
                             (unsigned char)listener_ctx.stream_id[7]);
			while (buf[i] != 'R')
				i++;
			i += 2;		/* skip the ':' */
			for (j = 0; j < 6; j++) {
				sscanf(&(buf[i + 2 * j]), "%02x", &id);
				listener_ctx.listener[j] = (unsigned char)id;
			} 
			switch (substate) {
			case 0:
				printf("with state ignore\n");
				break;
			case 1:
				printf("with state askfailed\n");
				break;
			case 2:
				printf("with state ready\n");
				break;
			case 3:
				printf("with state readyfail\n");
				break;
			default:
				printf("with state UNKNOWN (%d)\n", substate);
				break;
			}
			switch (buf[k + 1]) {
			case 'L':
				printf("got a leave indication\n");
                	        listener_ctx.mode = MRP_JOIN;
				break;
			case 'J':
			case 'N':
				printf("got a new/join indication\n");
				if (substate > MSRP_LISTENER_ASKFAILED) 
				{
	                	        listener_ctx.mode = MRP_JOIN;
				}
				else
				{
	                	        listener_ctx.mode = MRP_LEAVE;
				}
				break;
			}
			m_listener_cb(listener_ctx);
			break;
		case 'T':
			i = k + 4; /* skip "Sxx T:" */

			if (strncmp(buf, "SNE T:", 6) == 0 || strncmp(buf, "SJO T:", 6) == 0)
			{
				talker_ctx.mode = MRP_JOIN;
			}
			else
			{
				talker_ctx.mode = MRP_LEAVE;
			}
			while ((i < buflen) && ('S' != buf[i++]));
			if (i == buflen)
				return -1;
			i+=1;
			for(j = 0; j < 8 ; i+=2, j++)
			{
				sscanf(&buf[i],"%02x",&id);
				talker_ctx.stream_id[j] = (unsigned char)id;
			}
			i+=3;
			for(j = 0; j < 6 ; i+=2, j++)
			{
				sscanf(&buf[i],"%02x",&id);
				talker_ctx.dst_mac[j] = (unsigned char)id;
			}			
			i+=3;
			sscanf(&buf[i],"%04x",&id);
                        talker_ctx.vid = id;

			m_talker_cb(talker_ctx);
                        break;
		}
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if ((i == buflen) || (buf[i] == '\0'))
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case '\0':
		break;
	}
	return 0;
}

void *mrp_monitor_thread(void *arg)
{
	char *msgbuf;
	struct sockaddr_in client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;
	struct pollfd fds;
	int rc;
        int *halt_tx = (int *) arg;
	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return NULL;
	while (!*halt_tx) {
		fds.fd = m_sock;
		fds.events = POLLIN;
		fds.revents = 0;
		rc = poll(&fds, 1, 100);
		if (rc < 0) {
			free(msgbuf);
			pthread_exit(NULL);
		}
		if (rc == 0)
			continue;
		if ((fds.revents & POLLIN) == 0) {
			free(msgbuf);
			pthread_exit(NULL);
		}
		memset(&msg, 0, sizeof(msg));
		memset(&client_addr, 0, sizeof(client_addr));
		memset(msgbuf, 0, MAX_MRPD_CMDSZ);
		iov.iov_len = MAX_MRPD_CMDSZ;
		iov.iov_base = msgbuf;
		msg.msg_name = &client_addr;
		msg.msg_namelen = sizeof(client_addr);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		bytes = recvmsg(m_sock, &msg, 0);
		if (bytes < 0)
			continue;
		process_mrp_msg(msgbuf, bytes);
	}
	free(msgbuf);
	pthread_exit(NULL);
}

/*
 * public
 */

int mrp_connect(int *halt_tx)
{
	struct sockaddr_in addr;
	int m_sock_fd = -1;
	int rc;
	m_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_sock_fd < 0)
		goto out;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0);

	if(0 > (bind(m_sock_fd, (struct sockaddr*)&addr, sizeof(addr))))
	{
		fprintf(stderr, "Could not bind m_socket.\n");
		close(m_sock);
		return -1;
	}

	m_sock = m_sock_fd;

	rc = pthread_attr_init(&monitor_attr);
	rc |= pthread_create(&monitor_thread, NULL, mrp_monitor_thread, halt_tx);
	return rc;

 out:	if (m_sock_fd != -1)
		close(m_sock_fd);
	m_sock_fd = -1;
	return -1;
}

int mrp_disconnect()
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 64);
	sprintf(msgbuf, "BYE");
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int mrp_monitor(void)
{
	int rc;
	rc = pthread_attr_init(&monitor_attr);
	rc |= pthread_create(&monitor_thread, NULL, mrp_monitor_thread, NULL);
	return rc;
}

int mrp_register_domain(struct mrp_domain_attr *reg_class)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);

	sprintf(msgbuf, "S+D:C=%d,P=%d,V=%04x", reg_class->id, reg_class->priority, reg_class->vid);
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}


int
mrp_advertise_stream(uint8_t * streamid,
		     uint8_t * destaddr,
		     int pktsz, int interval, int latency, int vid, int priority)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);

	sprintf(msgbuf, "S++:S=%02x%02x%02x%02x%02x%02x%02x%02x"
		",A=%02x%02x%02x%02x%02x%02x"
		",V=%04x"
		",Z=%d"
		",I=%d"
		",P=%d"
		",L=%d", streamid[0], streamid[1], streamid[2],
		streamid[3], streamid[4], streamid[5], streamid[6],
		streamid[7], destaddr[0], destaddr[1], destaddr[2],
		destaddr[3], destaddr[4], destaddr[5], vid, pktsz,
		interval, (priority << 5)+16, latency);

	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int
mrp_unadvertise_stream(uint8_t * streamid,
		       uint8_t * destaddr,
		       int pktsz, int interval, int latency, int vid, int priority)
{
	char *msgbuf;
	int rc;
	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S--:S=%02x%02x%02x%02x%02x%02x%02x%02x"
		",A=%02x%02x%02x%02x%02x%02x"
		",V=%04x"
		",Z=%d"
		",I=%d"
		",P=%d"
		",L=%d", streamid[0], streamid[1], streamid[2],
		streamid[3], streamid[4], streamid[5], streamid[6],
		streamid[7], destaddr[0], destaddr[1], destaddr[2],
		destaddr[3], destaddr[4], destaddr[5], vid, pktsz,
		interval, (priority << 5)+16, latency);
	mrp_okay = 0;
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int mrp_join_vlan(struct mrp_domain_attr *reg_class)
{
	char *msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "V++:I=%04x\n",reg_class->vid);
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int report_domain_status(struct mrp_domain_attr *class_a)
{
	char* msgbuf;
	int rc;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S+D:C=%d,P=%d,V=%04x", class_a->id, class_a->priority, class_a->vid);
	rc = send_mrp_msg(msgbuf, 1500);
	free(msgbuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int send_ready(uint8_t *stream_id)
{
	char *databuf;
	int rc;

	databuf = malloc(1500);
	if (NULL == databuf)
		return -1;
	memset(databuf, 0, 1500);
	sprintf(databuf, "S+L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=2",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);

        printf("%s\n",databuf);
	rc = send_mrp_msg(databuf, 1500);

	free(databuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

int send_leave(uint8_t *stream_id)
{
	char *databuf;
	int rc;

	databuf = malloc(1500);
	if (NULL == databuf)
		return -1;
	memset(databuf, 0, 1500);
	sprintf(databuf, "S-L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=2",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
	rc = send_mrp_msg(databuf, 1500);
	free(databuf);

	if (rc != 1500)
		return -1;
	else
		return 0;
}

// TODO remove
int recv_mrp_okay()
{
	while ((mrp_okay == 0) && (mrp_error == 0))
		usleep(20000);
	return 0;
}
