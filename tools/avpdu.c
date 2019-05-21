/******************************************************************************

  Copyright (c) 2019, Ralf Beck
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

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

#include "avpdu.h"
#include <string.h>
#include <arpa/inet.h>

void init_avpdu_header
       (AVPDU_HEADER* p,
        uint8_t *dest,
        uint8_t *src,
        uint8_t *stream_id,
        uint8_t channels,
		uint32_t samplerate)
{
	memset(p, 0, sizeof(AVPDU_HEADER));

	uint16_t samples_per_packet;

	switch (samplerate)
	{
	    case 32000:
	    case 44100:
	    case 48000:
	    	samples_per_packet = 6;
	    	break;
	    case 88200:
	    case 96000:
	    	samples_per_packet = 12;
	    	break;
	    case 176400:
	    case 192000:
	    	samples_per_packet = 24;
	    	break;
	    default:
	    	return;
	}

	memcpy(&p->dest, dest, ETH_ALEN);
	memcpy(&p->src, src, ETH_ALEN);

	p->vlan_tag = htons(VLAN_TAG);
	p->avtp_vlan_tag = htonl(AVTP_VLAN_TAG);

	p->avtp_subtype = AVTP_IEC61883;
	p->flags1 = STREAM_ID_VALID;

	memcpy(&p->stream_id, stream_id, STREAM_ID_LEN);

	p->stream_data_length = BYTES_PER_SAMPLE *
			                channels *
							samples_per_packet +
							CIP_HEADER_LEN;

	p->cip_0 = CIP_0;
	p->cip_1 = CIP_1;
	p->cip_2 = CIP_2;

	p->data_block_size = channels;

	p->cip_4 = CIP_4;
	p->cip_6 = CIP_6;

	switch (samplerate)
	{
	    case 32000:
	    	p->fdf = 0;
	    	p->syt = 0x08;
	    	break;
	    case 44100:
	    	p->fdf = 1;
	    	p->syt = 0x08;
	    	break;
	    case 48000:
	    	p->fdf = 2;
	    	p->syt = 0x08;
	    	break;
	    case 88200:
	    	p->fdf = 3;
	    	p->syt = 0x10;
	    	break;
	    case 96000:
	    	p->fdf = 4;
	    	p->syt = 0x10;
	    	break;
	    case 176400:
	    	p->fdf = 5;
	    	p->syt = 0x20;
	    	break;
	    case 192000:
	    	p->fdf = 6;
	    	p->syt = 0x20;
	    	break;
	    default:
	    	return;
	}

}
void init_avpdu_header_no_vlan
       (AVPDU_HEADER_NO_VLAN* p,
        uint8_t *dest,
        uint8_t *src,
        uint8_t *stream_id,
        uint8_t channels,
		uint32_t samplerate)
{
	memset(p, 0, sizeof(AVPDU_HEADER_NO_VLAN));

	uint16_t samples_per_packet;

	switch (samplerate)
	{
	    case 32000:
	    case 44100:
	    case 48000:
	    	samples_per_packet = 6;
	    	break;
	    case 88200:
	    case 96000:
	    	samples_per_packet = 12;
	    	break;
	    case 176400:
	    case 192000:
	    	samples_per_packet = 24;
	    	break;
	    default:
	    	return;
	}

	memcpy(&p->dest, dest, ETH_ALEN);
	memcpy(&p->src, src, ETH_ALEN);

	p->avtp_subtype = AVTP_IEC61883;
	p->flags1 = STREAM_ID_VALID;

	memcpy(&p->stream_id, stream_id, STREAM_ID_LEN);

	p->stream_data_length = BYTES_PER_SAMPLE *
			                channels *
							samples_per_packet +
							CIP_HEADER_LEN;

	p->cip_0 = CIP_0;
	p->cip_1 = CIP_1;
	p->cip_2 = CIP_2;

	p->data_block_size = channels;

	p->cip_4 = CIP_4;
	p->cip_6 = CIP_6;

	switch (samplerate)
	{
	    case 32000:
	    	p->fdf = 0;
	    	p->syt = 0x08;
	    	break;
	    case 44100:
	    	p->fdf = 1;
	    	p->syt = 0x08;
	    	break;
	    case 48000:
	    	p->fdf = 2;
	    	p->syt = 0x08;
	    	break;
	    case 88200:
	    	p->fdf = 3;
	    	p->syt = 0x10;
	    	break;
	    case 96000:
	    	p->fdf = 4;
	    	p->syt = 0x10;
	    	break;
	    case 176400:
	    	p->fdf = 5;
	    	p->syt = 0x20;
	    	break;
	    case 192000:
	    	p->fdf = 6;
	    	p->syt = 0x20;
	    	break;
	    default:
	    	return;
	}

}
