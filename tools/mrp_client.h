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

#ifndef _TALKER_MRP_CLIENT_H_
#define _TALKER_MRP_CLIENT_H_

/*
 * simple_talker MRP client part
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <poll.h>

/* global defines */

#define MRPD_PORT_DEFAULT 7500
#define MAX_MRPD_CMDSZ (1500)
#define MSRP_LISTENER_ASKFAILED 1
#define MSRP_LISTENER_READY 2

#define MRP_JOIN 0
#define MRP_LEAVE 0

struct mrp_talker_ctx
{
	int mode;
        char stream_id[8];
        char dst_mac[6];
        int vid;
};

typedef void (*talker_cb_t)(const struct mrp_talker_ctx);

#define MRP_JOIN 0
#define MRP_LEAVE 0

struct mrp_listener_ctx
{
	int mode;
        char stream_id[8];
        char listener[6];
};

typedef void (*listener_cb_t)(const struct mrp_listener_ctx);

struct mrp_domain_ctx
{
	int mode;
	int id;
	int priority;
	int vid;
};

typedef void (*domain_cb_t)(const struct mrp_domain_ctx);

struct mrp_domain_attr
{
	int id;
	int priority;
	int vid;
};

extern volatile int mrp_error;

/* functions */

// from talker
int send_mrp_msg(char *notify_data, int notify_len);
int mrp_connect();
int mrp_disconnect();
int mrp_register_domain(struct mrp_domain_attr *reg_class);
int mrp_join_vlan(struct mrp_domain_attr *reg_class);
int mrp_advertise_stream(uint8_t * streamid, uint8_t * destaddr, 
                         int pktsz, int interval, int latency, int vid, int priority);
int mrp_unadvertise_stream(uint8_t * streamid, uint8_t * destaddr, 
                           int pktsz, int interval, int latency, int vid, int priority);

// from listener
int report_domain_status(struct mrp_domain_attr *class_a);

int send_ready(uint8_t *stream_id);
int send_leave(uint8_t *stream_id);

// all
int mrp_client_init(talker_cb_t talker_cb,
                    listener_cb_t listener_cb,
                    domain_cb_t domain_cb);

#endif /* _TALKER_MRP_CLIENT_H_ */
